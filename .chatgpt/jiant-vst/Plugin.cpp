// SPDX-License-Identifier: GPL-3.0-only
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

extern "C" {
float* web_out_l(void);
float* web_out_r(void);
void web_render(uint32_t n);
uint8_t* web_nor(void);
uint32_t web_nor_size(void);
void web_nor_erase(void);
void web_seed(uint32_t s);
void web_boot(void);
void web_buttons(uint32_t labels);
void web_keys(uint32_t mask);
void web_enc(uint32_t role, int32_t n);
void web_master(uint32_t v);
uint32_t web_midi(uint32_t status, uint32_t d1, uint32_t d2);
uint16_t* web_screen(void);
uint32_t web_lit_keys(void);
uint32_t web_mid_keys(void);
uint32_t web_sample_rate(void);
}

namespace
{
juce::CriticalSection gCoreLock;
bool gCoreBooted = false;

constexpr int kKnobCount = 8;
constexpr int kButtonCount = 14;
constexpr int kKeyCount = 27;
constexpr int kPhysicalBaseNote = 53; // F3 ... G5, exactly as JIANT's web host exposes it.
constexpr int kScreenW = 240;
constexpr int kScreenH = 240;
constexpr size_t kSourceRing = 262144;

// UI order requested by the user: MASTER, SELECT, PRESETS, ALGORITHM, K1..K4.
// JIANT's native encoder-role order is SELECT, ALGORITHM, PRESETS, K1..K4.
constexpr std::array<int, kKnobCount> kEncoderRole { -1, 0, 2, 1, 3, 4, 5, 6 };

constexpr std::array<const char*, kKnobCount> kKnobLabels {
    "MASTER", "SELECT", "PRESETS", "ALGORITHM", "KNOB 1", "KNOB 2", "KNOB 3", "KNOB 4"
};

// These are JIANT's native panel label indices from firmware/src/panel.c.
constexpr std::array<const char*, kButtonCount> kButtonLabels {
    "FX", "SCL", "ENV", "LFO", "EDIT", "GLO",
    "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC", "OCT-", "OCT+"
};

juce::Colour thermalPink()   { return juce::Colour::fromRGB (255, 88, 132); }
juce::Colour thermalAmber()  { return juce::Colour::fromRGB (255, 199, 100); }
juce::Colour warmWhite()     { return juce::Colour::fromRGB (255, 244, 216); }
juce::Colour panelBlack()    { return juce::Colour::fromRGB (29, 29, 31); }
juce::Colour insetBlack()    { return juce::Colour::fromRGB (18, 18, 20); }
juce::Colour controlGrey()   { return juce::Colour::fromRGB (55, 54, 52); }
juce::Colour outlineGrey()   { return juce::Colour::fromRGB (58, 58, 63); }

bool isBlackPitchClass (int note)
{
    const int pc = ((note % 12) + 12) % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

int foldMidiNoteToPhysicalKey (int note)
{
    while (note < kPhysicalBaseNote)
        note += 12;
    while (note >= kPhysicalBaseNote + kKeyCount)
        note -= 12;
    return juce::jlimit (0, kKeyCount - 1, note - kPhysicalBaseNote);
}
}

class JiantEditor;

class JiantProcessor final : public juce::AudioProcessor
{
public:
    JiantProcessor()
        : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          sourceL (kSourceRing, 0.0f), sourceR (kSourceRing, 0.0f)
    {
        for (auto& v : knobUi)
            v.store (0.5f);
        knobUi[0].store (700.0f / 1023.0f);
        lastLaunchkeyCc.fill (-1);
        for (auto& n : midiNotes)
            n.store (0);
    }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        const auto out = layouts.getMainOutputChannelSet();
        return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    }

    void prepareToPlay (double sampleRate, int) override
    {
        hostRate = sampleRate > 1000.0 ? sampleRate : 44100.0;
        sourceGenerated = 0;
        sourcePhase = 0.0;
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        juce::ScopedNoDenormals noDenormals;
        const juce::ScopedLock lock (gCoreLock);
        ensureCoreBootedLocked();

        for (const auto metadata : midi)
            handleMidiLocked (metadata.getMessage());
        midi.clear();

        const int n = buffer.getNumSamples();
        if (n <= 0)
            return;

        const double coreRate = static_cast<double> (web_sample_rate());
        const double ratio = coreRate / hostRate;
        const uint64_t lastNeeded = static_cast<uint64_t> (std::floor (sourcePhase + ratio * static_cast<double> (n - 1))) + 1u;

        while (sourceGenerated <= lastNeeded)
        {
            const uint64_t missing = lastNeeded + 1u - sourceGenerated;
            const uint32_t chunk = static_cast<uint32_t> (juce::jmin<uint64_t> (1024u, missing));
            web_render (chunk);
            const float* l = web_out_l();
            const float* r = web_out_r();
            for (uint32_t i = 0; i < chunk; ++i)
            {
                const size_t w = static_cast<size_t> ((sourceGenerated + i) % kSourceRing);
                sourceL[w] = l[i];
                sourceR[w] = r[i];
            }
            sourceGenerated += chunk;
        }

        buffer.clear();
        float* outL = buffer.getWritePointer (0);
        float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

        for (int i = 0; i < n; ++i)
        {
            const uint64_t a = static_cast<uint64_t> (std::floor (sourcePhase));
            const uint64_t b = a + 1u;
            const float frac = static_cast<float> (sourcePhase - static_cast<double> (a));
            const float la = sourceL[static_cast<size_t> (a % kSourceRing)];
            const float lb = sourceL[static_cast<size_t> (b % kSourceRing)];
            const float ra = sourceR[static_cast<size_t> (a % kSourceRing)];
            const float rb = sourceR[static_cast<size_t> (b % kSourceRing)];
            const float l = la + (lb - la) * frac;
            const float r = ra + (rb - ra) * frac;
            outL[i] = outR != nullptr ? l : 0.5f * (l + r);
            if (outR != nullptr)
                outR[i] = r;
            sourcePhase += ratio;
        }
    }

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    void getStateInformation (juce::MemoryBlock& destData) override
    {
        juce::MemoryOutputStream out (destData, false);
        out.writeInt (0x4A49414E); // JIAN
        out.writeInt (1);
        for (const auto& v : knobUi)
            out.writeFloat (v.load());

        const juce::ScopedLock lock (gCoreLock);
        if (gCoreBooted)
        {
            const uint32_t bytes = web_nor_size();
            out.writeInt (static_cast<int> (bytes));
            out.write (web_nor(), bytes);
        }
        else if (!pendingFlash.empty())
        {
            out.writeInt (static_cast<int> (pendingFlash.size()));
            out.write (pendingFlash.data(), pendingFlash.size());
        }
        else
        {
            out.writeInt (0);
        }
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        juce::MemoryInputStream in (data, static_cast<size_t> (sizeInBytes), false);
        if (in.readInt() != 0x4A49414E)
            return;
        const int version = in.readInt();
        if (version != 1)
            return;

        for (auto& v : knobUi)
            v.store (juce::jlimit (0.0f, 1.0f, in.readFloat()));

        const int bytes = in.readInt();
        if (bytes <= 0 || bytes > 2 * 1024 * 1024 || in.getNumBytesRemaining() < bytes)
            return;

        std::vector<uint8_t> restored (static_cast<size_t> (bytes));
        in.read (restored.data(), bytes);

        const juce::ScopedLock lock (gCoreLock);
        if (!gCoreBooted)
        {
            pendingFlash = std::move (restored);
        }
        else if (static_cast<uint32_t> (bytes) == web_nor_size())
        {
            std::memcpy (web_nor(), restored.data(), static_cast<size_t> (bytes));
        }
    }

    float getKnobUi (int index) const
    {
        return juce::isPositiveAndBelow (index, kKnobCount) ? knobUi[static_cast<size_t> (index)].load() : 0.0f;
    }

    void setKnobFromUi (int index, float value)
    {
        if (!juce::isPositiveAndBelow (index, kKnobCount))
            return;

        value = juce::jlimit (0.0f, 1.0f, value);
        const float previous = knobUi[static_cast<size_t> (index)].exchange (value);
        const juce::ScopedLock lock (gCoreLock);
        ensureCoreBootedLocked();

        if (index == 0)
        {
            web_master (static_cast<uint32_t> (std::lround (value * 1023.0f)));
            return;
        }

        int steps = static_cast<int> (std::lround ((value - previous) * 96.0f));
        steps = juce::jlimit (-24, 24, steps);
        if (steps != 0)
            web_enc (static_cast<uint32_t> (kEncoderRole[static_cast<size_t> (index)]), steps);
    }

    void setButtonFromUi (int index, bool down)
    {
        if (!juce::isPositiveAndBelow (index, kButtonCount))
            return;
        const uint32_t bit = 1u << static_cast<uint32_t> (index);
        if (down)
            buttonMask.fetch_or (bit);
        else
            buttonMask.fetch_and (~bit);

        const juce::ScopedLock lock (gCoreLock);
        ensureCoreBootedLocked();
        web_buttons (buttonMask.load());
    }

    void setKeyFromUi (int index, bool down)
    {
        if (!juce::isPositiveAndBelow (index, kKeyCount))
            return;

        const uint32_t bit = 1u << static_cast<uint32_t> (index);
        if (down)
            physicalKeyMask.fetch_or (bit);
        else
            physicalKeyMask.fetch_and (~bit);

        const juce::ScopedLock lock (gCoreLock);
        ensureCoreBootedLocked();
        web_keys (physicalKeyMask.load());
    }

    uint32_t getPhysicalKeyMask() const
    {
        return physicalKeyMask.load();
    }

    uint32_t getMidiVisualKeyMask() const
    {
        uint32_t mask = 0;
        for (int note = 0; note < 128; ++note)
        {
            if (midiNotes[static_cast<size_t> (note)].load() != 0)
            {
                const int key = foldMidiNoteToPhysicalKey (note);
                mask |= 1u << static_cast<uint32_t> (key);
            }
        }
        return mask;
    }

    void copyUiFrame (std::array<uint16_t, kScreenW * kScreenH>& dst, uint32_t& litKeys)
    {
        const juce::ScopedLock lock (gCoreLock);
        ensureCoreBootedLocked();
        std::memcpy (dst.data(), web_screen(), dst.size() * sizeof (uint16_t));
        litKeys = (web_lit_keys() | web_mid_keys()) & ((1u << kKeyCount) - 1u);
    }

private:
    void ensureCoreBootedLocked()
    {
        if (gCoreBooted)
            return;

        web_nor_erase();
        if (!pendingFlash.empty() && pendingFlash.size() == web_nor_size())
            std::memcpy (web_nor(), pendingFlash.data(), pendingFlash.size());
        web_seed (0x4A49414Eu);
        web_boot();
        web_master (static_cast<uint32_t> (std::lround (knobUi[0].load() * 1023.0f)));
        gCoreBooted = true;
    }

    void handleMidiLocked (const juce::MidiMessage& m)
    {
        // Launchkey preset: 8 knobs = MIDI ch.16 CC21..28.
        if (m.isController() && m.getChannel() == 16 && m.getControllerNumber() >= 21 && m.getControllerNumber() <= 28)
        {
            const int index = m.getControllerNumber() - 21;
            const int value = m.getControllerValue();
            knobUi[static_cast<size_t> (index)].store (static_cast<float> (value) / 127.0f);

            if (index == 0)
            {
                web_master (static_cast<uint32_t> (std::lround (value * 1023.0 / 127.0)));
            }
            else
            {
                int& last = lastLaunchkeyCc[static_cast<size_t> (index)];
                if (last >= 0)
                {
                    int delta = value - last;
                    if (delta > 64) delta -= 128;
                    if (delta < -64) delta += 128;
                    delta = juce::jlimit (-16, 16, delta);
                    if (delta != 0)
                        web_enc (static_cast<uint32_t> (kEncoderRole[static_cast<size_t> (index)]), delta);
                }
                last = value;
            }
            return;
        }

        // Launchkey drum preset: ch.10, first six pads of each row.
        // Upper row notes 44..49 -> FX..GLO; lower row notes 36..41 -> HOME..REC.
        if ((m.isNoteOn() || m.isNoteOff()) && m.getChannel() == 10)
        {
            const int note = m.getNoteNumber();
            int button = -1;
            if (note >= 44 && note <= 49)
                button = note - 44;
            else if (note >= 36 && note <= 41)
                button = 6 + note - 36;

            if (button >= 0)
            {
                const uint32_t bit = 1u << static_cast<uint32_t> (button);
                if (m.isNoteOn())
                    buttonMask.fetch_or (bit);
                else
                    buttonMask.fetch_and (~bit);
                web_buttons (buttonMask.load());
                return;
            }
        }

        // Visual keyboard feedback follows musical incoming MIDI, independent of channel.
        if (m.isNoteOn())
            midiNotes[static_cast<size_t> (m.getNoteNumber())].store (1);
        else if (m.isNoteOff())
            midiNotes[static_cast<size_t> (m.getNoteNumber())].store (0);

        // Everything else stays musical MIDI and is fed straight to JIANT.
        const auto* raw = m.getRawData();
        const int rawSize = m.getRawDataSize();
        if (rawSize > 0 && raw[0] >= 0x80 && raw[0] < 0xF0)
        {
            const uint32_t d1 = rawSize > 1 ? raw[1] : 0;
            const uint32_t d2 = rawSize > 2 ? raw[2] : 0;
            web_midi (raw[0], d1, d2);
        }
    }

    double hostRate = 44100.0;
    std::vector<float> sourceL, sourceR;
    uint64_t sourceGenerated = 0;
    double sourcePhase = 0.0;
    std::array<std::atomic<float>, kKnobCount> knobUi;
    std::array<int, kKnobCount> lastLaunchkeyCc;
    std::array<std::atomic<uint8_t>, 128> midiNotes;
    std::atomic<uint32_t> buttonMask { 0 };
    std::atomic<uint32_t> physicalKeyMask { 0 };
    std::vector<uint8_t> pendingFlash;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JiantProcessor)
};

class JiantLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    JiantLookAndFeel()
    {
        setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.92f));
        setColour (juce::TextButton::textColourOnId, panelBlack());
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                               static_cast<float> (width), static_cast<float> (height)).reduced (5.0f);
        const float diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
        auto knob = juce::Rectangle<float> (0.0f, 0.0f, diameter, diameter).withCentre (bounds.getCentre());

        g.setColour (juce::Colour::fromRGB (11, 11, 13));
        g.fillEllipse (knob.expanded (3.0f));
        g.setColour (juce::Colour::fromRGB (45, 45, 48));
        g.fillEllipse (knob);
        g.setColour (outlineGrey());
        g.drawEllipse (knob, 1.6f);

        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        const auto centre = knob.getCentre();
        const float radius = knob.getWidth() * 0.5f;
        const juce::Point<float> p1 (centre.x + std::sin (angle) * radius * 0.30f,
                                     centre.y - std::cos (angle) * radius * 0.30f);
        const juce::Point<float> p2 (centre.x + std::sin (angle) * radius * 0.72f,
                                     centre.y - std::cos (angle) * radius * 0.72f);
        g.setColour (warmWhite());
        g.drawLine ({ p1, p2 }, 3.0f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour&, bool isHighlighted, bool isButtonDown) override
    {
        auto r = button.getLocalBounds().toFloat().reduced (1.0f);
        auto fill = controlGrey();
        if (isButtonDown)
            fill = warmWhite();
        else if (isHighlighted)
            fill = fill.brighter (0.08f);

        g.setColour (fill);
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (juce::Colours::black.withAlpha (0.30f));
        g.drawRoundedRectangle (r, 7.0f, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool, bool isButtonDown) override
    {
        g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        auto colour = button.findColour (juce::TextButton::textColourOffId);
        if (isButtonDown)
            colour = panelBlack();
        g.setColour (colour);
        g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (3),
                          juce::Justification::centred, 2);
    }
};

class JiantTouchKeyboard final : public juce::Component
{
public:
    explicit JiantTouchKeyboard (JiantProcessor& p) : processor (p)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    ~JiantTouchKeyboard() override
    {
        releaseMouseKey();
    }

    void setActivityMasks (uint32_t physical, uint32_t midi, uint32_t leds)
    {
        physical &= ((1u << kKeyCount) - 1u);
        midi &= ((1u << kKeyCount) - 1u);
        leds &= ((1u << kKeyCount) - 1u);
        if (physical != physicalMask || midi != midiMask || leds != ledMask)
        {
            physicalMask = physical;
            midiMask = midi;
            ledMask = leds;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto panel = getLocalBounds().toFloat();
        g.setColour (insetBlack());
        g.fillRoundedRectangle (panel, 18.0f);

        for (int i = 0; i < kKeyCount; ++i)
        {
            const uint32_t bit = 1u << static_cast<uint32_t> (i);
            const bool mouseActive = (physicalMask & bit) != 0;
            const bool midiActive = (midiMask & bit) != 0;
            const bool firmwareLit = (ledMask & bit) != 0;
            const auto r = keyBounds[static_cast<size_t> (i)];

            g.setColour ((mouseActive || midiActive) ? juce::Colour::fromRGB (25, 24, 26)
                                                      : juce::Colour::fromRGB (13, 13, 15));
            g.fillRoundedRectangle (r, r.getWidth() * 0.45f);
            g.setColour ((mouseActive || midiActive) ? thermalPink().withAlpha (0.70f) : outlineGrey());
            g.drawRoundedRectangle (r, r.getWidth() * 0.45f, 1.3f);

            const float barW = juce::jmax (4.0f, r.getWidth() * 0.13f);
            auto bar = juce::Rectangle<float> (barW, r.getHeight() * 0.66f)
                           .withCentre (r.getCentre());

            if (mouseActive)
                g.setColour (warmWhite());
            else if (midiActive)
                g.setColour (thermalPink());
            else if (firmwareLit)
                g.setColour (thermalAmber());
            else
                g.setColour (juce::Colour::fromRGB (135, 134, 136));
            g.fillRoundedRectangle (bar, barW * 0.5f);
        }
    }

    void resized() override
    {
        for (auto& r : keyBounds)
            r = {};

        auto area = getLocalBounds().toFloat().reduced (18.0f, 12.0f);
        int naturalCount = 0;
        for (int i = 0; i < kKeyCount; ++i)
            if (!isBlackPitchClass (kPhysicalBaseNote + i))
                ++naturalCount;

        const float step = area.getWidth() / static_cast<float> (naturalCount);
        const float naturalW = juce::jmin (48.0f, step * 0.72f);
        const float blackW = juce::jmin (44.0f, step * 0.62f);
        const float topY = area.getY();
        const float topH = area.getHeight() * 0.44f;
        const float bottomY = area.getY() + area.getHeight() * 0.54f;
        const float bottomH = area.getBottom() - bottomY;

        int naturalsBefore = 0;
        for (int i = 0; i < kKeyCount; ++i)
        {
            const int note = kPhysicalBaseNote + i;
            if (isBlackPitchClass (note))
            {
                const float cx = area.getX() + static_cast<float> (naturalsBefore) * step;
                keyBounds[static_cast<size_t> (i)] = juce::Rectangle<float> (blackW, topH)
                                                        .withCentre ({ cx, topY + topH * 0.5f });
            }
            else
            {
                const float cx = area.getX() + (static_cast<float> (naturalsBefore) + 0.5f) * step;
                keyBounds[static_cast<size_t> (i)] = juce::Rectangle<float> (naturalW, bottomH)
                                                        .withCentre ({ cx, bottomY + bottomH * 0.5f });
                ++naturalsBefore;
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        moveMouseKeyTo (keyAt (e.position));
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        moveMouseKeyTo (keyAt (e.position));
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        releaseMouseKey();
    }

private:
    int keyAt (juce::Point<float> p) const
    {
        for (int i = 0; i < kKeyCount; ++i)
            if (keyBounds[static_cast<size_t> (i)].contains (p))
                return i;
        return -1;
    }

    void moveMouseKeyTo (int key)
    {
        if (key == mouseKey)
            return;
        if (mouseKey >= 0)
            processor.setKeyFromUi (mouseKey, false);
        mouseKey = key;
        if (mouseKey >= 0)
            processor.setKeyFromUi (mouseKey, true);
    }

    void releaseMouseKey()
    {
        if (mouseKey >= 0)
            processor.setKeyFromUi (mouseKey, false);
        mouseKey = -1;
    }

    JiantProcessor& processor;
    std::array<juce::Rectangle<float>, kKeyCount> keyBounds {};
    uint32_t physicalMask = 0;
    uint32_t midiMask = 0;
    uint32_t ledMask = 0;
    int mouseKey = -1;
};

class JiantEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit JiantEditor (JiantProcessor& p)
        : AudioProcessorEditor (&p), processor (p), screen (juce::Image::RGB, kScreenW, kScreenH, true), touchKeyboard (p)
    {
        setLookAndFeel (&look);

        for (int i = 0; i < kKnobCount; ++i)
        {
            auto slider = std::make_unique<juce::Slider>();
            slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider->setRange (0.0, 1.0, 0.001);
            slider->setValue (processor.getKnobUi (i), juce::dontSendNotification);
            slider->setDoubleClickReturnValue (true, i == 0 ? 700.0 / 1023.0 : 0.5);
            slider->onValueChange = [this, i, s = slider.get()] {
                processor.setKnobFromUi (i, static_cast<float> (s->getValue()));
            };
            addAndMakeVisible (*slider);
            knobs.push_back (std::move (slider));

            auto label = std::make_unique<juce::Label>();
            label->setText (kKnobLabels[static_cast<size_t> (i)], juce::dontSendNotification);
            label->setJustificationType (juce::Justification::centred);
            label->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.92f));
            label->setFont (juce::FontOptions (13.0f, juce::Font::bold));
            addAndMakeVisible (*label);
            knobLabels.push_back (std::move (label));
        }

        for (int i = 0; i < kButtonCount; ++i)
        {
            auto button = std::make_unique<juce::TextButton> (kButtonLabels[static_cast<size_t> (i)]);
            button->setClickingTogglesState (false);
            button->setColour (juce::TextButton::textColourOffId,
                               i == 11 ? juce::Colour::fromRGB (255, 72, 72) : juce::Colours::white.withAlpha (0.94f));
            button->onStateChange = [this, i, b = button.get()] {
                processor.setButtonFromUi (i, b->isDown());
            };
            addAndMakeVisible (*button);
            buttons.push_back (std::move (button));
        }

        addAndMakeVisible (touchKeyboard);

        // Keep setSize last. JUCE may synchronously call resized().
        setResizable (true, true);
        setResizeLimits (900, 580, 1600, 1000);
        setSize (1080, 680);
        startTimerHz (30);
    }

    ~JiantEditor() override
    {
        stopTimer();
        setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour::fromRGB (150, 150, 153));

        auto body = panelBounds.toFloat();
        g.setColour (panelBlack());
        g.fillRoundedRectangle (body, 39.0f);

        if (!octavePanelBounds.isEmpty())
        {
            g.setColour (insetBlack());
            g.fillRoundedRectangle (octavePanelBounds.toFloat(), 12.0f);
        }
        if (!functionPanelBounds.isEmpty())
        {
            g.setColour (insetBlack());
            g.fillRoundedRectangle (functionPanelBounds.toFloat(), 14.0f);
        }

        auto display = displayBounds.toFloat();
        g.setColour (juce::Colour::fromRGB (8, 8, 10));
        g.fillRoundedRectangle (display.expanded (9.0f), 16.0f);
        g.setColour (juce::Colour::fromRGB (2, 2, 4));
        g.fillRoundedRectangle (display.expanded (3.0f), 10.0f);
        g.drawImageWithin (screen, displayBounds.getX(), displayBounds.getY(), displayBounds.getWidth(), displayBounds.getHeight(),
                           juce::RectanglePlacement::centred | juce::RectanglePlacement::stretchToFit, false);
    }

    void resized() override
    {
        if (knobs.size() != kKnobCount || knobLabels.size() != kKnobCount || buttons.size() != kButtonCount)
            return;

        panelBounds = getLocalBounds().reduced (16);
        auto inner = panelBounds.reduced (32, 27);

        const int keyboardH = juce::jlimit (190, 310, static_cast<int> (std::lround (inner.getHeight() * 0.44)));
        auto keyboardArea = inner.removeFromBottom (keyboardH);
        inner.removeFromBottom (18);
        touchKeyboard.setBounds (keyboardArea);

        auto top = inner;
        const int gap = juce::jlimit (12, 24, top.getWidth() / 55);
        const int leftW = juce::jlimit (200, 280, static_cast<int> (std::lround (top.getWidth() * 0.245)));
        const int displayW = juce::jlimit (205, 295, static_cast<int> (std::lround (top.getWidth() * 0.27)));

        auto left = top.removeFromLeft (leftW);
        top.removeFromLeft (gap);
        auto displayArea = top.removeFromLeft (juce::jmin (displayW, top.getWidth()));
        top.removeFromLeft (juce::jmin (gap, top.getWidth()));
        auto right = top;

        const int displaySize = juce::jmin (displayArea.getWidth() - 4, displayArea.getHeight() - 4);
        displayBounds = juce::Rectangle<int> (displaySize, displaySize).withCentre (displayArea.getCentre());

        // Left side: MASTER/SELECT, PRESETS/ALGORITHM, then native OCT-/OCT+.
        octavePanelBounds = left.removeFromBottom (55);
        left.removeFromBottom (8);
        const int leftCellW = left.getWidth() / 2;
        const int leftCellH = left.getHeight() / 2;
        for (int i = 0; i < 4; ++i)
        {
            const int col = i % 2;
            const int row = i / 2;
            auto cell = juce::Rectangle<int> (left.getX() + col * leftCellW,
                                               left.getY() + row * leftCellH,
                                               leftCellW, leftCellH).reduced (5);
            auto label = cell.removeFromTop (24);
            knobLabels[static_cast<size_t> (i)]->setBounds (label);
            knobs[static_cast<size_t> (i)]->setBounds (cell.reduced (8, 1));
        }

        auto oct = octavePanelBounds.reduced (10, 8);
        const int octGap = 12;
        const int octW = (oct.getWidth() - octGap) / 2;
        buttons[12]->setBounds (oct.removeFromLeft (octW));
        oct.removeFromLeft (octGap);
        buttons[13]->setBounds (oct);

        // Right side: K1..K4 on top, then the 12 function buttons in two rows.
        const int knobRowH = juce::jlimit (105, 145, static_cast<int> (std::lround (right.getHeight() * 0.43)));
        auto knobRow = right.removeFromTop (knobRowH);
        right.removeFromTop (8);
        functionPanelBounds = right;

        const int rightCellW = juce::jmax (1, knobRow.getWidth() / 4);
        for (int i = 4; i < 8; ++i)
        {
            const int col = i - 4;
            auto cell = juce::Rectangle<int> (knobRow.getX() + col * rightCellW,
                                               knobRow.getY(), rightCellW, knobRow.getHeight()).reduced (4);
            auto label = cell.removeFromTop (24);
            knobLabels[static_cast<size_t> (i)]->setBounds (label);
            knobs[static_cast<size_t> (i)]->setBounds (cell.reduced (7, 1));
        }

        auto buttonArea = functionPanelBounds.reduced (12, 12);
        const int buttonGap = 6;
        const int bw = juce::jmax (1, (buttonArea.getWidth() - 5 * buttonGap) / 6);
        const int bh = juce::jmax (1, (buttonArea.getHeight() - buttonGap) / 2);
        for (int row = 0; row < 2; ++row)
            for (int col = 0; col < 6; ++col)
            {
                const int i = row * 6 + col;
                buttons[static_cast<size_t> (i)]->setBounds (buttonArea.getX() + col * (bw + buttonGap),
                                                              buttonArea.getY() + row * (bh + buttonGap), bw, bh);
            }
    }

private:
    void timerCallback() override
    {
        std::array<uint16_t, kScreenW * kScreenH> raw {};
        uint32_t litKeys = 0;
        processor.copyUiFrame (raw, litKeys);

        for (int y = 0; y < kScreenH; ++y)
        {
            for (int x = 0; x < kScreenW; ++x)
            {
                const uint16_t sw = raw[static_cast<size_t> (y * kScreenW + x)];
                const uint16_t c = static_cast<uint16_t> ((sw >> 8) | (sw << 8));
                const uint8_t r = static_cast<uint8_t> (((c >> 11) & 31u) * 255u / 31u);
                const uint8_t g = static_cast<uint8_t> (((c >> 5) & 63u) * 255u / 63u);
                const uint8_t b = static_cast<uint8_t> ((c & 31u) * 255u / 31u);
                screen.setPixelAt (x, y, juce::Colour::fromRGB (r, g, b));
            }
        }

        // Keep the GUI knobs visually in sync when the Launchkey moves them.
        for (int i = 0; i < kKnobCount; ++i)
            knobs[static_cast<size_t> (i)]->setValue (processor.getKnobUi (i), juce::dontSendNotification);

        touchKeyboard.setActivityMasks (processor.getPhysicalKeyMask(), processor.getMidiVisualKeyMask(), litKeys);
        repaint (displayBounds.expanded (10));
    }

    JiantProcessor& processor;
    JiantLookAndFeel look;
    juce::Image screen;
    JiantTouchKeyboard touchKeyboard;
    juce::Rectangle<int> panelBounds;
    juce::Rectangle<int> displayBounds;
    juce::Rectangle<int> octavePanelBounds;
    juce::Rectangle<int> functionPanelBounds;
    std::vector<std::unique_ptr<juce::Slider>> knobs;
    std::vector<std::unique_ptr<juce::Label>> knobLabels;
    std::vector<std::unique_ptr<juce::TextButton>> buttons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JiantEditor)
};

juce::AudioProcessorEditor* JiantProcessor::createEditor()
{
    return new JiantEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JiantProcessor();
}

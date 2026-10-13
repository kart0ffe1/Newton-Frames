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
void web_enc(uint32_t role, int32_t n);
void web_master(uint32_t v);
uint32_t web_midi(uint32_t status, uint32_t d1, uint32_t d2);
uint16_t* web_screen(void);
uint32_t web_sample_rate(void);
}

namespace
{
juce::CriticalSection gCoreLock;
bool gCoreBooted = false;

constexpr int kKnobCount = 8;
constexpr int kButtonCount = 12;
constexpr int kScreenW = 240;
constexpr int kScreenH = 240;
constexpr size_t kSourceRing = 262144;

// UI order requested by the user: MASTER, SELECT, PRESETS, ALGORITHM, K1..K4.
// JIANT's native encoder-role order is SELECT, ALGORITHM, PRESETS, K1..K4.
constexpr std::array<int, kKnobCount> kEncoderRole { -1, 0, 2, 1, 3, 4, 5, 6 };

constexpr std::array<const char*, kKnobCount> kKnobLabels {
    "MASTER", "SELECT", "PRESETS", "ALGORITHM", "KNOB 1", "KNOB 2", "KNOB 3", "KNOB 4"
};

constexpr std::array<const char*, kButtonCount> kButtonLabels {
    "FX", "SCL", "ENV", "LFO", "EDIT", "GLO",
    "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"
};

juce::Colour thermalPink()   { return juce::Colour::fromRGB (255, 88, 132); }
juce::Colour thermalAmber()  { return juce::Colour::fromRGB (255, 199, 100); }
juce::Colour thermalViolet() { return juce::Colour::fromRGB (88, 48, 114); }
juce::Colour panelBlack()    { return juce::Colour::fromRGB (9, 7, 14); }
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

    void copyScreen (std::array<uint16_t, kScreenW * kScreenH>& dst)
    {
        const juce::ScopedLock lock (gCoreLock);
        ensureCoreBootedLocked();
        std::memcpy (dst.data(), web_screen(), dst.size() * sizeof (uint16_t));
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
    std::atomic<uint32_t> buttonMask { 0 };
    std::vector<uint8_t> pendingFlash;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JiantProcessor)
};

class JiantLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    JiantLookAndFeel()
    {
        setColour (juce::Slider::rotarySliderFillColourId, thermalPink());
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromRGB (38, 22, 48));
        setColour (juce::Slider::thumbColourId, thermalAmber());
        setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (30, 18, 39));
        setColour (juce::TextButton::buttonOnColourId, thermalPink().withAlpha (0.72f));
        setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.88f));
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    }
};

class JiantEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit JiantEditor (JiantProcessor& p)
        : AudioProcessorEditor (&p), processor (p), screen (juce::Image::RGB, kScreenW, kScreenH, true)
    {
        setLookAndFeel (&look);
        setSize (1040, 650);
        setResizable (true, true);
        setResizeLimits (850, 560, 1500, 950);

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
            label->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.82f));
            label->setFont (juce::FontOptions (13.0f, juce::Font::bold));
            addAndMakeVisible (*label);
            knobLabels.push_back (std::move (label));
        }

        for (int i = 0; i < kButtonCount; ++i)
        {
            auto button = std::make_unique<juce::TextButton> (kButtonLabels[static_cast<size_t> (i)]);
            button->setClickingTogglesState (false);
            button->onStateChange = [this, i, b = button.get()] {
                processor.setButtonFromUi (i, b->isDown());
            };
            addAndMakeVisible (*button);
            buttons.push_back (std::move (button));
        }

        startTimerHz (30);
    }

    ~JiantEditor() override
    {
        stopTimer();
        setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        juce::ColourGradient bg (panelBlack(), 0.0f, 0.0f,
                                 juce::Colour::fromRGB (35, 12, 45), bounds.getWidth(), bounds.getHeight(), false);
        bg.addColour (0.55, juce::Colour::fromRGB (18, 8, 28));
        g.setGradientFill (bg);
        g.fillAll();

        g.setColour (thermalPink().withAlpha (0.16f));
        g.fillEllipse (bounds.getWidth() - 320.0f, -130.0f, 430.0f, 430.0f);
        g.setColour (thermalAmber().withAlpha (0.08f));
        g.fillEllipse (-130.0f, bounds.getHeight() - 220.0f, 420.0f, 420.0f);

        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (31.0f, juce::Font::bold));
        g.drawText ("JIANT FM", 28, 13, 300, 40, juce::Justification::centredLeft);
        g.setColour (thermalAmber());
        g.setFont (juce::FontOptions (12.5f));
        g.drawText ("NATIVE FIRMWARE • VST3", 206, 23, 260, 24, juce::Justification::centredLeft);

        auto display = displayBounds.toFloat();
        g.setColour (thermalPink().withAlpha (0.35f));
        g.fillRoundedRectangle (display.expanded (7.0f), 13.0f);
        g.setColour (juce::Colour::fromRGB (3, 3, 5));
        g.fillRoundedRectangle (display.expanded (4.0f), 10.0f);
        g.drawImageWithin (screen, displayBounds.getX(), displayBounds.getY(), displayBounds.getWidth(), displayBounds.getHeight(),
                           juce::RectanglePlacement::centred | juce::RectanglePlacement::stretchToFit, false);

        g.setColour (juce::Colours::white.withAlpha (0.62f));
        g.setFont (juce::FontOptions (11.5f));
        g.drawFittedText ("Launchkey: Encoders CH16 CC21–28  •  Pads CH10: 44–49 / 36–41  •  Keyboard MIDI passes directly to JIANT",
                          footerBounds, juce::Justification::centred, 1);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (28);
        area.removeFromTop (42);
        footerBounds = area.removeFromBottom (28);
        area.removeFromBottom (8);

        const int leftWidth = juce::jmin (520, static_cast<int> (area.getWidth() * 0.54f));
        auto left = area.removeFromLeft (leftWidth);
        area.removeFromLeft (22);
        auto right = area;

        const int displaySize = juce::jmin (left.getWidth() - 10, left.getHeight() - 150);
        displayBounds = juce::Rectangle<int> (left.getX() + (left.getWidth() - displaySize) / 2,
                                              left.getY() + 8,
                                              displaySize, displaySize);

        auto buttonArea = left.withTop (displayBounds.getBottom() + 24);
        const int gap = 6;
        const int bw = (buttonArea.getWidth() - 5 * gap) / 6;
        const int bh = juce::jmin (38, (buttonArea.getHeight() - gap) / 2);
        for (int row = 0; row < 2; ++row)
            for (int col = 0; col < 6; ++col)
            {
                const int i = row * 6 + col;
                buttons[static_cast<size_t> (i)]->setBounds (buttonArea.getX() + col * (bw + gap),
                                                              buttonArea.getY() + row * (bh + gap), bw, bh);
            }

        const int cols = 2;
        const int rows = 4;
        const int cellW = right.getWidth() / cols;
        const int cellH = right.getHeight() / rows;
        for (int i = 0; i < kKnobCount; ++i)
        {
            const int col = i % cols;
            const int row = i / cols;
            auto cell = juce::Rectangle<int> (right.getX() + col * cellW, right.getY() + row * cellH, cellW, cellH).reduced (8);
            auto label = cell.removeFromBottom (23);
            knobs[static_cast<size_t> (i)]->setBounds (cell.reduced (8, 0));
            knobLabels[static_cast<size_t> (i)]->setBounds (label);
        }
    }

private:
    void timerCallback() override
    {
        std::array<uint16_t, kScreenW * kScreenH> raw {};
        processor.copyScreen (raw);
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
        repaint (displayBounds.expanded (8));
    }

    JiantProcessor& processor;
    JiantLookAndFeel look;
    juce::Image screen;
    juce::Rectangle<int> displayBounds;
    juce::Rectangle<int> footerBounds;
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

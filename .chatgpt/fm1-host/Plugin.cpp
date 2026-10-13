// SPDX-License-Identifier: GPL-3.0-only
#include <JuceHeader.h>
#include "FirmwareApi.h"
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#if defined(_WIN32)
 #include <windows.h>
#endif

namespace
{
constexpr int kKnobCount = 8;
constexpr int kButtonCount = 14;
constexpr int kKeyCount = 27;
constexpr int kPhysicalBaseNote = 53;
constexpr int kScreenW = 240;
constexpr int kScreenH = 240;
constexpr size_t kSourceRing = 262144;
constexpr std::array<int, kKnobCount> kEncoderRole { -1, 0, 2, 1, 3, 4, 5, 6 };
constexpr std::array<const char*, kKnobCount> kKnobLabels {
    "MASTER", "SELECT", "PRESETS", "ALGORITHM", "KNOB 1", "KNOB 2", "KNOB 3", "KNOB 4"
};
constexpr std::array<const char*, kButtonCount> kButtonLabels {
    "FX", "SCL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC", "OCT-", "OCT+"
};

juce::Colour thermalPink()  { return juce::Colour::fromRGB (255, 88, 132); }
juce::Colour thermalAmber() { return juce::Colour::fromRGB (255, 199, 100); }
juce::Colour warmWhite()    { return juce::Colour::fromRGB (255, 244, 216); }
juce::Colour panelBlack()   { return juce::Colour::fromRGB (29, 29, 31); }
juce::Colour insetBlack()   { return juce::Colour::fromRGB (18, 18, 20); }
juce::Colour controlGrey()  { return juce::Colour::fromRGB (55, 54, 52); }
juce::Colour outlineGrey()  { return juce::Colour::fromRGB (58, 58, 63); }

bool isBlackPitchClass (int note)
{
    const int pc = ((note % 12) + 12) % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

int foldMidiNoteToPhysicalKey (int note)
{
    while (note < kPhysicalBaseNote) note += 12;
    while (note >= kPhysicalBaseNote + kKeyCount) note -= 12;
    return juce::jlimit (0, kKeyCount - 1, note - kPhysicalBaseNote);
}

juce::File thisPluginBinary()
{
#if defined(_WIN32)
    HMODULE module = nullptr;
    if (GetModuleHandleExA (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR> (&thisPluginBinary), &module) != 0)
    {
        std::array<char, 32768> path {};
        const DWORD n = GetModuleFileNameA (module, path.data(), static_cast<DWORD> (path.size()));
        if (n > 0 && n < path.size())
            return juce::File (juce::String::fromUTF8 (path.data(), static_cast<int> (n)));
    }
#endif
    return juce::File::getSpecialLocation (juce::File::currentExecutableFile);
}

juce::String safeId (juce::String id)
{
    return juce::File::createLegalFileName (id.isNotEmpty() ? id : "unknown");
}

struct FirmwareEntry
{
    juce::String id;
    juce::String name;
    juce::File library;
    bool bundled = false;
};
}

class FirmwareHostEditor;

class FirmwareHostProcessor final : public juce::AudioProcessor, private juce::Timer
{
public:
    FirmwareHostProcessor()
        : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          sourceL (kSourceRing, 0.0f), sourceR (kSourceRing, 0.0f)
    {
        for (auto& v : knobUi) v.store (0.5f);
        knobUi[0].store (700.0f / 1023.0f);
        lastLaunchkeyCc.fill (-1);
        for (auto& n : midiNotes) n.store (0);
        userFirmwareFolder.createDirectory();
        rescanFirmwares();
        const int jiant = indexOfFirmwareId ("jiant");
        if (!firmwares.empty()) loadFirmware (jiant >= 0 ? jiant : 0);
        startTimer (500);
    }

    ~FirmwareHostProcessor() override
    {
        stopTimer();
        persistFlash (true);
        unloadBackend();
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
        const juce::ScopedLock lock (coreLock);
        hostRate = sampleRate > 1000.0 ? sampleRate : 44100.0;
        resetAudioClockLocked();
    }
    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        juce::ScopedNoDenormals noDenormals;
        const juce::ScopedLock lock (coreLock);
        if (api == nullptr)
        {
            buffer.clear();
            midi.clear();
            return;
        }

        for (const auto metadata : midi) handleMidiLocked (metadata.getMessage());
        midi.clear();
        const int n = buffer.getNumSamples();
        if (n <= 0) return;

        const double coreRate = static_cast<double> (api->sample_rate());
        const double ratio = coreRate / hostRate;
        const uint64_t lastNeeded = static_cast<uint64_t> (std::floor (sourcePhase + ratio * static_cast<double> (n - 1))) + 1u;
        while (sourceGenerated <= lastNeeded)
        {
            const uint64_t missing = lastNeeded + 1u - sourceGenerated;
            const uint32_t chunk = static_cast<uint32_t> (juce::jmin<uint64_t> (1024u, missing));
            api->render (chunk);
            const float* l = api->out_l();
            const float* r = api->out_r();
            for (uint32_t i = 0; i < chunk; ++i)
            {
                const size_t w = static_cast<size_t> ((sourceGenerated + i) % kSourceRing);
                sourceL[w] = l[i]; sourceR[w] = r[i];
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
            if (outR != nullptr) outR[i] = r;
            sourcePhase += ratio;
        }
    }

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    void getStateInformation (juce::MemoryBlock& destData) override
    {
        juce::MemoryOutputStream out (destData, false);
        out.writeInt (0x464D3148); // FM1H
        out.writeInt (1);
        out.writeString (currentFirmwareId);
        for (const auto& v : knobUi) out.writeFloat (v.load());
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        juce::MemoryInputStream in (data, static_cast<size_t> (sizeInBytes), false);
        if (in.readInt() != 0x464D3148 || in.readInt() != 1) return;
        const auto wanted = in.readString();
        for (auto& v : knobUi) v.store (juce::jlimit (0.0f, 1.0f, in.readFloat()));
        const int idx = indexOfFirmwareId (wanted);
        if (idx >= 0 && wanted != currentFirmwareId) loadFirmware (idx);
    }

    void rescanFirmwares()
    {
        std::vector<FirmwareEntry> found;
        scanFirmwareRoot (bundledFirmwareFolder(), true, found);
        scanFirmwareRoot (userFirmwareFolder, false, found);
        std::vector<FirmwareEntry> merged;
        for (const auto& e : found)
        {
            auto it = std::find_if (merged.begin(), merged.end(), [&] (const FirmwareEntry& x) { return x.id == e.id; });
            if (it == merged.end()) merged.push_back (e);
            else if (!e.bundled) *it = e;
        }
        firmwares = std::move (merged);
    }

    int getFirmwareCount() const { return static_cast<int> (firmwares.size()); }
    juce::String getFirmwareName (int i) const { return juce::isPositiveAndBelow (i, getFirmwareCount()) ? firmwares[(size_t) i].name : juce::String(); }
    int getCurrentFirmwareIndex() const { return indexOfFirmwareId (currentFirmwareId); }
    juce::File getUserFirmwareFolder() const { return userFirmwareFolder; }

    bool selectFirmwareIndex (int index)
    {
        if (!juce::isPositiveAndBelow (index, getFirmwareCount())) return false;
        if (firmwares[(size_t) index].id == currentFirmwareId) return true;
        return loadFirmware (index);
    }

    float getKnobUi (int index) const
    {
        return juce::isPositiveAndBelow (index, kKnobCount) ? knobUi[(size_t) index].load() : 0.0f;
    }

    void setKnobFromUi (int index, float value)
    {
        if (!juce::isPositiveAndBelow (index, kKnobCount)) return;
        value = juce::jlimit (0.0f, 1.0f, value);
        const float previous = knobUi[(size_t) index].exchange (value);
        const juce::ScopedLock lock (coreLock);
        if (api == nullptr) return;
        if (index == 0)
        {
            api->master (static_cast<uint32_t> (std::lround (value * 1023.0f)));
            return;
        }
        int steps = static_cast<int> (std::lround ((value - previous) * 96.0f));
        steps = juce::jlimit (-24, 24, steps);
        if (steps != 0) api->enc (static_cast<uint32_t> (kEncoderRole[(size_t) index]), steps);
    }

    void setButtonFromUi (int index, bool down)
    {
        if (!juce::isPositiveAndBelow (index, kButtonCount)) return;
        const uint32_t bit = 1u << static_cast<uint32_t> (index);
        if (down) buttonMask.fetch_or (bit); else buttonMask.fetch_and (~bit);
        const juce::ScopedLock lock (coreLock);
        if (api != nullptr) api->buttons (buttonMask.load());
    }

    void setKeyFromUi (int index, bool down)
    {
        if (!juce::isPositiveAndBelow (index, kKeyCount)) return;
        const uint32_t bit = 1u << static_cast<uint32_t> (index);
        if (down) physicalKeyMask.fetch_or (bit); else physicalKeyMask.fetch_and (~bit);
        const juce::ScopedLock lock (coreLock);
        if (api != nullptr) api->keys (physicalKeyMask.load());
    }

    uint32_t getPhysicalKeyMask() const { return physicalKeyMask.load(); }
    uint32_t getMidiVisualKeyMask() const
    {
        uint32_t mask = 0;
        for (int note = 0; note < 128; ++note)
            if (midiNotes[(size_t) note].load() != 0)
                mask |= 1u << static_cast<uint32_t> (foldMidiNoteToPhysicalKey (note));
        return mask;
    }

    bool copyUiFrame (std::array<uint16_t, kScreenW * kScreenH>& dst, uint32_t& litKeys)
    {
        const juce::ScopedLock lock (coreLock);
        if (api == nullptr || api->screen() == nullptr)
        {
            dst.fill (0); litKeys = 0; return false;
        }
        std::memcpy (dst.data(), api->screen(), dst.size() * sizeof (uint16_t));
        litKeys = (api->lit_keys() | api->mid_keys()) & ((1u << kKeyCount) - 1u);
        return true;
    }

private:
    static juce::File userDataRoot()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("FM1 Firmware Host");
    }

    static juce::File bundledFirmwareFolder()
    {
        auto f = thisPluginBinary();
        auto contents = f.getParentDirectory().getParentDirectory();
        return contents.getChildFile ("Resources").getChildFile ("Firmwares");
    }

    int indexOfFirmwareId (const juce::String& id) const
    {
        for (int i = 0; i < getFirmwareCount(); ++i) if (firmwares[(size_t) i].id == id) return i;
        return -1;
    }

    static bool validApi (const FM1BackendApiV1* a)
    {
        return a != nullptr && a->abi_version == FM1_BACKEND_ABI_VERSION && a->firmware_id && a->firmware_name
            && a->out_l && a->out_r && a->render && a->nor && a->nor_size && a->nor_erase && a->seed && a->boot
            && a->buttons && a->keys && a->enc && a->master && a->midi && a->screen && a->lit_keys && a->mid_keys && a->sample_rate;
    }

    void scanFirmwareRoot (const juce::File& root, bool bundled, std::vector<FirmwareEntry>& out)
    {
        if (!root.isDirectory()) return;
#if defined(_WIN32)
        const juce::String pattern = "fm1_backend.dll";
#elif defined(__APPLE__)
        const juce::String pattern = "libfm1_backend.dylib";
#else
        const juce::String pattern = "libfm1_backend.so";
#endif
        juce::Array<juce::File> libs;
        root.findChildFiles (libs, juce::File::findFiles, true, pattern);
        for (const auto& libFile : libs)
        {
            juce::DynamicLibrary lib;
            if (!lib.open (libFile.getFullPathName())) continue;
            auto getApi = reinterpret_cast<FM1BackendGetApiFn> (lib.getFunction ("fm1_backend_get_api"));
            const FM1BackendApiV1* candidate = getApi != nullptr ? getApi() : nullptr;
            if (!validApi (candidate)) continue;
            out.push_back ({ juce::String::fromUTF8 (candidate->firmware_id), juce::String::fromUTF8 (candidate->firmware_name), libFile, bundled });
        }
    }

    bool loadFirmware (int index)
    {
        if (!juce::isPositiveAndBelow (index, getFirmwareCount())) return false;
        persistFlash (true);
        const auto entry = firmwares[(size_t) index];
        const juce::ScopedLock lock (coreLock);
        unloadBackendLocked();

        auto tempRoot = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("FM1 Firmware Host").getChildFile (juce::Uuid().toString());
        if (!tempRoot.createDirectory()) return false;
#if defined(_WIN32)
        tempBackendFile = tempRoot.getChildFile ("fm1_backend.dll");
#else
        tempBackendFile = tempRoot.getChildFile (entry.library.getFileName());
#endif
        if (!entry.library.copyFileTo (tempBackendFile)) return false;

        auto newLib = std::make_unique<juce::DynamicLibrary>();
        if (!newLib->open (tempBackendFile.getFullPathName())) return false;
        auto getApi = reinterpret_cast<FM1BackendGetApiFn> (newLib->getFunction ("fm1_backend_get_api"));
        const FM1BackendApiV1* newApi = getApi != nullptr ? getApi() : nullptr;
        if (!validApi (newApi)) return false;

        backendLib = std::move (newLib); api = newApi;
        currentFirmwareId = juce::String::fromUTF8 (api->firmware_id);
        buttonMask.store (0); physicalKeyMask.store (0); for (auto& n : midiNotes) n.store (0); lastLaunchkeyCc.fill (-1);
        api->nor_erase(); loadFlashLocked(); api->seed (0x4A49414Eu); api->boot();
        api->master (static_cast<uint32_t> (std::lround (knobUi[0].load() * 1023.0f)));
        lastFlashWriteCount = api->flash_writes_count ? api->flash_writes_count() : 0u;
        resetAudioClockLocked();
        return true;
    }

    juce::File stateFileForCurrentFirmware() const
    {
        auto dir = userDataRoot().getChildFile ("State").getChildFile (safeId (currentFirmwareId));
        dir.createDirectory();
        return dir.getChildFile ("flash.bin");
    }

    void loadFlashLocked()
    {
        if (api == nullptr) return;
        const size_t bytes = static_cast<size_t> (api->nor_size());
        if (bytes == 0 || bytes > 4u * 1024u * 1024u) return;
        auto state = stateFileForCurrentFirmware();
        if (!state.existsAsFile() && currentFirmwareId == "jiant")
        {
            const auto legacy = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("JIANT FM").getChildFile ("jiant_flash.bin");
            if (legacy.existsAsFile()) legacy.copyFileTo (state);
        }
        juce::MemoryBlock data;
        if (state.existsAsFile() && state.loadFileAsData (data) && data.getSize() == bytes) std::memcpy (api->nor(), data.getData(), bytes);
    }

    void persistFlash (bool force)
    {
        std::vector<uint8_t> snapshot; juce::File state; uint32_t writeCount = 0;
        {
            const juce::ScopedLock lock (coreLock);
            if (api == nullptr) return;
            writeCount = api->flash_writes_count ? api->flash_writes_count() : lastFlashWriteCount + 1u;
            if (!force && writeCount == lastFlashWriteCount) return;
            const size_t bytes = static_cast<size_t> (api->nor_size());
            if (bytes == 0 || bytes > 4u * 1024u * 1024u) return;
            snapshot.resize (bytes); std::memcpy (snapshot.data(), api->nor(), bytes); state = stateFileForCurrentFirmware();
        }
        juce::TemporaryFile temp (state);
        if (temp.getFile().replaceWithData (snapshot.data(), snapshot.size()) && temp.overwriteTargetFileWithTemporary()) lastFlashWriteCount = writeCount;
    }

    void timerCallback() override { persistFlash (false); }
    void unloadBackend() { const juce::ScopedLock lock (coreLock); unloadBackendLocked(); }
    void unloadBackendLocked()
    {
        api = nullptr;
        if (backendLib) backendLib->close();
        backendLib.reset();
        if (tempBackendFile.existsAsFile()) tempBackendFile.deleteFile();
        const auto dir = tempBackendFile.getParentDirectory(); if (dir.isDirectory()) dir.deleteRecursively();
        tempBackendFile = {}; currentFirmwareId.clear(); resetAudioClockLocked();
    }

    void resetAudioClockLocked()
    {
        sourceGenerated = 0; sourcePhase = 0.0;
        std::fill (sourceL.begin(), sourceL.end(), 0.0f); std::fill (sourceR.begin(), sourceR.end(), 0.0f);
    }

    void handleMidiLocked (const juce::MidiMessage& m)
    {
        if (api == nullptr) return;
        if (m.isController() && m.getChannel() == 16 && m.getControllerNumber() >= 21 && m.getControllerNumber() <= 28)
        {
            const int index = m.getControllerNumber() - 21, value = m.getControllerValue();
            knobUi[(size_t) index].store ((float) value / 127.0f);
            if (index == 0) api->master ((uint32_t) std::lround (value * 1023.0 / 127.0));
            else
            {
                int& last = lastLaunchkeyCc[(size_t) index];
                if (last >= 0)
                {
                    int delta = value - last; if (delta > 64) delta -= 128; if (delta < -64) delta += 128; delta = juce::jlimit (-16, 16, delta);
                    if (delta != 0) api->enc ((uint32_t) kEncoderRole[(size_t) index], delta);
                }
                last = value;
            }
            return;
        }
        if ((m.isNoteOn() || m.isNoteOff()) && m.getChannel() == 10)
        {
            const int note = m.getNoteNumber(); int button = -1;
            if (note >= 44 && note <= 49) button = note - 44; else if (note >= 36 && note <= 41) button = 6 + note - 36;
            if (button >= 0)
            {
                const uint32_t bit = 1u << (uint32_t) button; if (m.isNoteOn()) buttonMask.fetch_or (bit); else buttonMask.fetch_and (~bit);
                api->buttons (buttonMask.load()); return;
            }
        }
        if (m.isNoteOn()) midiNotes[(size_t) m.getNoteNumber()].store (1); else if (m.isNoteOff()) midiNotes[(size_t) m.getNoteNumber()].store (0);
        const auto* raw = m.getRawData(); const int rawSize = m.getRawDataSize();
        if (rawSize > 0 && raw[0] >= 0x80 && raw[0] < 0xF0) api->midi (raw[0], rawSize > 1 ? raw[1] : 0, rawSize > 2 ? raw[2] : 0);
    }

    juce::CriticalSection coreLock;
    std::unique_ptr<juce::DynamicLibrary> backendLib;
    const FM1BackendApiV1* api = nullptr;
    juce::File tempBackendFile;
    const juce::File userFirmwareFolder = userDataRoot().getChildFile ("Firmwares");
    std::vector<FirmwareEntry> firmwares;
    juce::String currentFirmwareId;
    uint32_t lastFlashWriteCount = 0;
    double hostRate = 44100.0;
    std::vector<float> sourceL, sourceR;
    uint64_t sourceGenerated = 0;
    double sourcePhase = 0.0;
    std::array<std::atomic<float>, kKnobCount> knobUi;
    std::array<int, kKnobCount> lastLaunchkeyCc;
    std::array<std::atomic<uint8_t>, 128> midiNotes;
    std::atomic<uint32_t> buttonMask { 0 }, physicalKeyMask { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FirmwareHostProcessor)
};

class HostLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    HostLookAndFeel()
    {
        setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.92f));
        setColour (juce::TextButton::textColourOnId, panelBlack());
        setColour (juce::ComboBox::backgroundColourId, insetBlack());
        setColour (juce::ComboBox::textColourId, juce::Colours::white);
        setColour (juce::ComboBox::outlineColourId, outlineGrey());
    }
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (5.0f);
        const float d = juce::jmin (b.getWidth(), b.getHeight()); auto k = juce::Rectangle<float> (0, 0, d, d).withCentre (b.getCentre());
        g.setColour (juce::Colour::fromRGB (11,11,13)); g.fillEllipse (k.expanded (3)); g.setColour (juce::Colour::fromRGB (45,45,48)); g.fillEllipse (k); g.setColour (outlineGrey()); g.drawEllipse (k, 1.6f);
        const float a = start + pos * (end - start), r = k.getWidth() * 0.5f; const auto c = k.getCentre(); g.setColour (warmWhite());
        g.drawLine ({ c.x + std::sin (a) * r * 0.3f, c.y - std::cos (a) * r * 0.3f, c.x + std::sin (a) * r * 0.72f, c.y - std::cos (a) * r * 0.72f }, 3.0f);
    }
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hi, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1); auto fill = controlGrey(); if (down || b.getToggleState()) fill = warmWhite(); else if (hi) fill = fill.brighter (0.08f);
        g.setColour (fill); g.fillRoundedRectangle (r, 7); g.setColour (juce::Colours::black.withAlpha (0.3f)); g.drawRoundedRectangle (r, 7, 1);
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down) override
    {
        g.setFont (juce::FontOptions (12.5f, juce::Font::bold)); g.setColour ((down || b.getToggleState()) ? panelBlack() : b.findColour (juce::TextButton::textColourOffId));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3), juce::Justification::centred, 2);
    }
};

class TouchKeyboard final : public juce::Component
{
public:
    explicit TouchKeyboard (FirmwareHostProcessor& p) : processor (p)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor); setWantsKeyboardFocus (false); setMouseClickGrabsKeyboardFocus (false);
    }
    ~TouchKeyboard() override { moveTo (-1); }
    void setMasks (uint32_t physical, uint32_t midi, uint32_t leds)
    {
        if (physical != physicalMask || midi != midiMask || leds != ledMask) { physicalMask = physical; midiMask = midi; ledMask = leds; repaint(); }
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (insetBlack()); g.fillRoundedRectangle (getLocalBounds().toFloat(), 18);
        for (int i = 0; i < kKeyCount; ++i)
        {
            const uint32_t bit = 1u << (uint32_t) i; const bool mouse = physicalMask & bit, midi = midiMask & bit, lit = ledMask & bit; auto r = keyBounds[(size_t) i];
            g.setColour ((mouse || midi) ? juce::Colour::fromRGB (25,24,26) : juce::Colour::fromRGB (13,13,15)); g.fillRoundedRectangle (r, r.getWidth() * 0.45f);
            g.setColour ((mouse || midi) ? thermalPink().withAlpha (0.7f) : outlineGrey()); g.drawRoundedRectangle (r, r.getWidth() * 0.45f, 1.3f);
            const float bw = juce::jmax (4.0f, r.getWidth() * 0.13f); auto bar = juce::Rectangle<float> (bw, r.getHeight() * 0.66f).withCentre (r.getCentre());
            g.setColour (mouse ? warmWhite() : midi ? thermalPink() : lit ? thermalAmber() : juce::Colour::fromRGB (135,134,136)); g.fillRoundedRectangle (bar, bw * 0.5f);
        }
    }
    void resized() override
    {
        auto area = getLocalBounds().toFloat().reduced (18,12); int naturals = 0; for (int i=0;i<kKeyCount;++i) if (!isBlackPitchClass(kPhysicalBaseNote+i)) ++naturals;
        const float step = area.getWidth()/(float)naturals, nw = juce::jmin(48.0f,step*0.72f), bw=juce::jmin(44.0f,step*0.62f), topH=area.getHeight()*0.44f, bottomY=area.getY()+area.getHeight()*0.54f;
        int before=0; for(int i=0;i<kKeyCount;++i) if(isBlackPitchClass(kPhysicalBaseNote+i)){const float cx=area.getX()+(float)before*step; keyBounds[(size_t)i]=juce::Rectangle<float>(bw,topH).withCentre({cx,area.getY()+topH*0.5f});} else {const float cx=area.getX()+((float)before+0.5f)*step; keyBounds[(size_t)i]=juce::Rectangle<float>(nw,area.getBottom()-bottomY).withCentre({cx,(bottomY+area.getBottom())*0.5f}); ++before;}
    }
    void mouseDown (const juce::MouseEvent& e) override { moveTo(keyAt(e.position)); }
    void mouseDrag (const juce::MouseEvent& e) override { moveTo(keyAt(e.position)); }
    void mouseUp (const juce::MouseEvent&) override { moveTo(-1); }
private:
    int keyAt(juce::Point<float> p) const { for(int i=0;i<kKeyCount;++i) if(keyBounds[(size_t)i].contains(p)) return i; return -1; }
    void moveTo(int k){if(k==mouseKey)return; if(mouseKey>=0)processor.setKeyFromUi(mouseKey,false); mouseKey=k; if(mouseKey>=0)processor.setKeyFromUi(mouseKey,true);}
    FirmwareHostProcessor& processor; std::array<juce::Rectangle<float>,kKeyCount> keyBounds{}; uint32_t physicalMask=0,midiMask=0,ledMask=0; int mouseKey=-1;
};

class FirmwareHostEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FirmwareHostEditor (FirmwareHostProcessor& p) : AudioProcessorEditor(&p), processor(p), screen(juce::Image::RGB,kScreenW,kScreenH,true), keyboard(p)
    {
        setLookAndFeel(&look);
        firmwareLabel.setText("FIRMWARE",juce::dontSendNotification); firmwareLabel.setColour(juce::Label::textColourId,juce::Colours::white.withAlpha(0.75f)); firmwareLabel.setFont(juce::FontOptions(12.0f,juce::Font::bold)); addAndMakeVisible(firmwareLabel);
        firmwareBox.setWantsKeyboardFocus(false); firmwareBox.setMouseClickGrabsKeyboardFocus(false); firmwareBox.onChange=[this]{if(!updatingFirmwareBox)processor.selectFirmwareIndex(firmwareBox.getSelectedId()-1);}; addAndMakeVisible(firmwareBox);
        openFolder.setButtonText("FIRMWARE FOLDER"); openFolder.setWantsKeyboardFocus(false); openFolder.setMouseClickGrabsKeyboardFocus(false); openFolder.onClick=[this]{auto f=processor.getUserFirmwareFolder();f.createDirectory();f.startAsProcess();}; addAndMakeVisible(openFolder);
        rescan.setButtonText("RESCAN"); rescan.setWantsKeyboardFocus(false); rescan.setMouseClickGrabsKeyboardFocus(false); rescan.onClick=[this]{processor.rescanFirmwares();populateFirmwareBox();}; addAndMakeVisible(rescan);
        for(int i=0;i<kKnobCount;++i){auto s=std::make_unique<juce::Slider>();s->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s->setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);s->setWantsKeyboardFocus(false);s->setMouseClickGrabsKeyboardFocus(false);s->setRange(0,1,0.001);s->setValue(processor.getKnobUi(i),juce::dontSendNotification);s->setDoubleClickReturnValue(true,i==0?700.0/1023.0:0.5);s->onValueChange=[this,i,r=s.get()]{processor.setKnobFromUi(i,(float)r->getValue());};addAndMakeVisible(*s);knobs.push_back(std::move(s));auto l=std::make_unique<juce::Label>();l->setText(kKnobLabels[(size_t)i],juce::dontSendNotification);l->setJustificationType(juce::Justification::centred);l->setColour(juce::Label::textColourId,juce::Colours::white.withAlpha(0.92f));l->setFont(juce::FontOptions(13.0f,juce::Font::bold));addAndMakeVisible(*l);knobLabels.push_back(std::move(l));}
        for(int i=0;i<kButtonCount;++i){auto b=std::make_unique<juce::TextButton>(kButtonLabels[(size_t)i]);b->setClickingTogglesState(false);b->setWantsKeyboardFocus(false);b->setMouseClickGrabsKeyboardFocus(false);b->setColour(juce::TextButton::textColourOffId,i==11?juce::Colour::fromRGB(255,72,72):juce::Colours::white.withAlpha(0.94f));b->onStateChange=[this,i,r=b.get()]{const bool down=r->isDown();if(i>=12){processor.setButtonFromUi(i,down);return;}if(down){if(functionLatched[(size_t)i]){functionLatched[(size_t)i]=false;r->setToggleState(false,juce::dontSendNotification);processor.setButtonFromUi(i,false);}else if(juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown()){functionLatched[(size_t)i]=true;r->setToggleState(true,juce::dontSendNotification);processor.setButtonFromUi(i,true);}else processor.setButtonFromUi(i,true);r->repaint();}else if(!functionLatched[(size_t)i])processor.setButtonFromUi(i,false);};addAndMakeVisible(*b);buttons.push_back(std::move(b));}
        addAndMakeVisible(keyboard); populateFirmwareBox(); setResizable(true,true); setResizeLimits(940,620,1650,1050); setSize(1120,730); startTimerHz(30);
    }
    ~FirmwareHostEditor() override { stopTimer(); setLookAndFeel(nullptr); }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour::fromRGB(145,145,148));g.setColour(panelBlack());g.fillRoundedRectangle(panelBounds.toFloat(),39);g.setColour(juce::Colour::fromRGB(9,9,11));g.fillRoundedRectangle(toolbarBounds.toFloat(),12);if(!octavePanelBounds.isEmpty()){g.setColour(insetBlack());g.fillRoundedRectangle(octavePanelBounds.toFloat(),12);}if(!functionPanelBounds.isEmpty()){g.setColour(insetBlack());g.fillRoundedRectangle(functionPanelBounds.toFloat(),14);}auto d=displayBounds.toFloat();g.setColour(juce::Colour::fromRGB(8,8,10));g.fillRoundedRectangle(d.expanded(9),16);g.setColour(juce::Colour::fromRGB(2,2,4));g.fillRoundedRectangle(d.expanded(3),10);g.drawImageWithin(screen,displayBounds.getX(),displayBounds.getY(),displayBounds.getWidth(),displayBounds.getHeight(),juce::RectanglePlacement::centred|juce::RectanglePlacement::stretchToFit,false);
    }
    void resized() override
    {
        if(knobs.size()!=kKnobCount||buttons.size()!=kButtonCount)return;panelBounds=getLocalBounds().reduced(16);auto inner=panelBounds.reduced(32,24);toolbarBounds=inner.removeFromTop(42);inner.removeFromTop(12);auto tb=toolbarBounds.reduced(10,6);firmwareLabel.setBounds(tb.removeFromLeft(72));firmwareBox.setBounds(tb.removeFromLeft(juce::jmin(250,tb.getWidth()/2)));tb.removeFromLeft(8);rescan.setBounds(tb.removeFromLeft(76));tb.removeFromLeft(8);openFolder.setBounds(tb.removeFromLeft(150));const int keyboardH=juce::jlimit(190,310,(int)std::lround(inner.getHeight()*0.44));auto keyboardArea=inner.removeFromBottom(keyboardH);inner.removeFromBottom(18);keyboard.setBounds(keyboardArea);auto top=inner;const int gap=juce::jlimit(12,24,top.getWidth()/55),leftW=juce::jlimit(200,280,(int)std::lround(top.getWidth()*0.245)),displayW=juce::jlimit(205,295,(int)std::lround(top.getWidth()*0.27));auto left=top.removeFromLeft(leftW);top.removeFromLeft(gap);auto displayArea=top.removeFromLeft(juce::jmin(displayW,top.getWidth()));top.removeFromLeft(juce::jmin(gap,top.getWidth()));auto right=top;const int ds=juce::jmin(displayArea.getWidth()-4,displayArea.getHeight()-4);displayBounds=juce::Rectangle<int>(ds,ds).withCentre(displayArea.getCentre());octavePanelBounds=left.removeFromBottom(55);left.removeFromBottom(8);const int lcw=left.getWidth()/2,lch=left.getHeight()/2;for(int i=0;i<4;++i){auto cell=juce::Rectangle<int>(left.getX()+(i%2)*lcw,left.getY()+(i/2)*lch,lcw,lch).reduced(5);knobLabels[(size_t)i]->setBounds(cell.removeFromTop(24));knobs[(size_t)i]->setBounds(cell.reduced(8,1));}auto oct=octavePanelBounds.reduced(10,8);const int ow=(oct.getWidth()-12)/2;buttons[12]->setBounds(oct.removeFromLeft(ow));oct.removeFromLeft(12);buttons[13]->setBounds(oct);const int krh=juce::jlimit(105,145,(int)std::lround(right.getHeight()*0.43));auto kr=right.removeFromTop(krh);right.removeFromTop(8);functionPanelBounds=right;const int rcw=juce::jmax(1,kr.getWidth()/4);for(int i=4;i<8;++i){auto cell=juce::Rectangle<int>(kr.getX()+(i-4)*rcw,kr.getY(),rcw,kr.getHeight()).reduced(4);knobLabels[(size_t)i]->setBounds(cell.removeFromTop(24));knobs[(size_t)i]->setBounds(cell.reduced(7,1));}auto ba=functionPanelBounds.reduced(12);const int bg=6,bw=juce::jmax(1,(ba.getWidth()-5*bg)/6),bh=juce::jmax(1,(ba.getHeight()-bg)/2);for(int row=0;row<2;++row)for(int col=0;col<6;++col)buttons[(size_t)(row*6+col)]->setBounds(ba.getX()+col*(bw+bg),ba.getY()+row*(bh+bg),bw,bh);
    }
private:
    void populateFirmwareBox(){updatingFirmwareBox=true;firmwareBox.clear(juce::dontSendNotification);for(int i=0;i<processor.getFirmwareCount();++i)firmwareBox.addItem(processor.getFirmwareName(i),i+1);const int current=processor.getCurrentFirmwareIndex();if(current>=0)firmwareBox.setSelectedId(current+1,juce::dontSendNotification);updatingFirmwareBox=false;}
    void timerCallback() override
    {
        std::array<uint16_t,kScreenW*kScreenH> raw{};uint32_t lit=0;processor.copyUiFrame(raw,lit);for(int y=0;y<kScreenH;++y)for(int x=0;x<kScreenW;++x){const uint16_t sw=raw[(size_t)(y*kScreenW+x)],c=(uint16_t)((sw>>8)|(sw<<8));screen.setPixelAt(x,y,juce::Colour::fromRGB((uint8_t)(((c>>11)&31u)*255u/31u),(uint8_t)(((c>>5)&63u)*255u/63u),(uint8_t)((c&31u)*255u/31u)));}for(int i=0;i<kKnobCount;++i)knobs[(size_t)i]->setValue(processor.getKnobUi(i),juce::dontSendNotification);keyboard.setMasks(processor.getPhysicalKeyMask(),processor.getMidiVisualKeyMask(),lit);const int current=processor.getCurrentFirmwareIndex();if(current>=0&&firmwareBox.getSelectedId()!=current+1){updatingFirmwareBox=true;firmwareBox.setSelectedId(current+1,juce::dontSendNotification);updatingFirmwareBox=false;}repaint(displayBounds.expanded(10));
    }
    FirmwareHostProcessor& processor;HostLookAndFeel look;juce::Image screen;TouchKeyboard keyboard;juce::Label firmwareLabel;juce::ComboBox firmwareBox;juce::TextButton openFolder,rescan;bool updatingFirmwareBox=false;std::array<bool,12> functionLatched{};juce::Rectangle<int> panelBounds,toolbarBounds,displayBounds,octavePanelBounds,functionPanelBounds;std::vector<std::unique_ptr<juce::Slider>> knobs;std::vector<std::unique_ptr<juce::Label>> knobLabels;std::vector<std::unique_ptr<juce::TextButton>> buttons;JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FirmwareHostEditor)
};

juce::AudioProcessorEditor* FirmwareHostProcessor::createEditor(){return new FirmwareHostEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new FirmwareHostProcessor();}

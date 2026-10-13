from pathlib import Path
import re
import sys

path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("Plugin.cpp")
s = path.read_text(encoding="utf-8")
orig = s

# Add a direct relative-encoder entry point for mouse/UI encoders.
needle = '''    void setButtonFromUi (int index, bool down)\n'''
insert = '''    void turnEncoderFromUi (int index, int steps)\n    {\n        if (index <= 0 || index >= kKnobCount || steps == 0) return;\n        steps = juce::jlimit (-64, 64, steps);\n        const juce::ScopedLock lock (coreLock);\n        if (api != nullptr) api->enc (static_cast<uint32_t> (kEncoderRole[(size_t) index]), steps);\n    }\n\n'''
if insert not in s:
    if needle not in s:
        raise SystemExit("could not find setButtonFromUi insertion point")
    s = s.replace(needle, insert + needle, 1)

# Let the look-and-feel know which rotary controls are endless and therefore
# must not show an absolute position marker.
old_sig = '''    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override\n'''
new_sig = '''    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& slider) override\n'''
if old_sig in s:
    s = s.replace(old_sig, new_sig, 1)
elif new_sig not in s:
    raise SystemExit("could not find drawRotarySlider signature")

old_line = '''        const float a = start + pos * (end - start), r = k.getWidth() * 0.5f; const auto c = k.getCentre(); g.setColour (warmWhite());\n        g.drawLine ({ c.x + std::sin (a) * r * 0.3f, c.y - std::cos (a) * r * 0.3f, c.x + std::sin (a) * r * 0.72f, c.y - std::cos (a) * r * 0.72f }, 3.0f);\n'''
new_line = '''        if (! static_cast<bool> (slider.getProperties().getWithDefault ("endless", false)))\n        {\n            const float a = start + pos * (end - start), r = k.getWidth() * 0.5f; const auto c = k.getCentre(); g.setColour (warmWhite());\n            g.drawLine ({ c.x + std::sin (a) * r * 0.3f, c.y - std::cos (a) * r * 0.3f, c.x + std::sin (a) * r * 0.72f, c.y - std::cos (a) * r * 0.72f }, 3.0f);\n        }\n'''
if old_line in s:
    s = s.replace(old_line, new_line, 1)
elif new_line not in s:
    raise SystemExit("could not find rotary marker drawing code")

# A Slider subclass is kept so the existing layout/vector code stays simple,
# but its mouse interaction is purely relative. Unbounded mouse movement means
# the encoder can be turned indefinitely without hitting 0/1 or a screen edge.
marker = '''class TouchKeyboard final : public juce::Component\n'''
endless_class = r'''class EndlessEncoder final : public juce::Slider
{
public:
    EndlessEncoder (FirmwareHostProcessor& p, int i) : processor (p), index (i)
    {
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setWantsKeyboardFocus (false);
        setMouseClickGrabsKeyboardFocus (false);
        setRange (0.0, 1.0, 0.001);
        setValue (0.5, juce::dontSendNotification);
        getProperties().set ("endless", true);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        lastDistance = 0;
        pixelAccumulator = 0.0f;
        e.source.enableUnboundedMouseMovement (true, false);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const int distance = -e.getDistanceFromDragStartY();
        const int deltaPixels = distance - lastDistance;
        lastDistance = distance;
        pixelAccumulator += static_cast<float> (deltaPixels) / 3.0f;
        const int steps = static_cast<int> (pixelAccumulator);
        if (steps != 0)
        {
            processor.turnEncoderFromUi (index, steps);
            pixelAccumulator -= static_cast<float> (steps);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        e.source.enableUnboundedMouseMovement (false);
        lastDistance = 0;
        pixelAccumulator = 0.0f;
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        const float d = std::abs (wheel.deltaY) >= std::abs (wheel.deltaX) ? wheel.deltaY : wheel.deltaX;
        if (d != 0.0f)
            processor.turnEncoderFromUi (index, d > 0.0f ? 1 : -1);
    }

private:
    FirmwareHostProcessor& processor;
    int index = 1;
    int lastDistance = 0;
    float pixelAccumulator = 0.0f;
};

'''
if endless_class not in s:
    if marker not in s:
        raise SystemExit("could not find TouchKeyboard insertion point")
    s = s.replace(marker, endless_class + marker, 1)

# Replace the editor knob creation loop. MASTER remains the original bounded
# Slider; all seven other controls use the relative EndlessEncoder.
start = '        for(int i=0;i<kKnobCount;++i){auto s=std::make_unique<juce::Slider>();'
end = 'knobLabels.push_back(std::move(l));}'
si = s.find(start)
if si >= 0:
    ei = s.find(end, si)
    if ei < 0:
        raise SystemExit("could not find end of knob creation loop")
    ei += len(end)
    replacement = '''        for (int i = 0; i < kKnobCount; ++i)\n        {\n            std::unique_ptr<juce::Slider> s;\n            if (i == 0)\n            {\n                auto master = std::make_unique<juce::Slider>();\n                master->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);\n                master->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);\n                master->setWantsKeyboardFocus (false);\n                master->setMouseClickGrabsKeyboardFocus (false);\n                master->setRange (0, 1, 0.001);\n                master->setValue (processor.getKnobUi (0), juce::dontSendNotification);\n                master->setDoubleClickReturnValue (true, 700.0 / 1023.0);\n                master->onValueChange = [this, r = master.get()] { processor.setKnobFromUi (0, (float) r->getValue()); };\n                s = std::move (master);\n            }\n            else\n            {\n                s = std::make_unique<EndlessEncoder> (processor, i);\n            }\n            addAndMakeVisible (*s);\n            knobs.push_back (std::move (s));\n\n            auto l = std::make_unique<juce::Label>();\n            l->setText (kKnobLabels[(size_t) i], juce::dontSendNotification);\n            l->setJustificationType (juce::Justification::centred);\n            l->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.92f));\n            l->setFont (juce::FontOptions (13.0f, juce::Font::bold));\n            addAndMakeVisible (*l);\n            knobLabels.push_back (std::move (l));\n        }'''
    s = s[:si] + replacement + s[ei:]
elif 'std::make_unique<EndlessEncoder> (processor, i)' not in s:
    raise SystemExit("could not find knob creation loop")

# Only MASTER has an absolute visual value. Endless encoders deliberately do
# not chase Launchkey/UI positions, so page switches cannot create value jumps.
old_timer = 'for(int i=0;i<kKnobCount;++i)knobs[(size_t)i]->setValue(processor.getKnobUi(i),juce::dontSendNotification);'
new_timer = 'knobs[0]->setValue (processor.getKnobUi (0), juce::dontSendNotification);'
if old_timer in s:
    s = s.replace(old_timer, new_timer, 1)
elif new_timer not in s:
    raise SystemExit("could not find timer knob refresh")

if s == orig:
    raise SystemExit("no changes made")
path.write_text(s, encoding="utf-8")
print(f"patched endless encoders in {path}")

#!/usr/bin/env python3
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: patch_ui_v3.py PATH/TO/Plugin.cpp")

p = Path(sys.argv[1])
s = p.read_text(encoding="utf-8")

# Make latched buttons visually stay pressed.
s = s.replace(
'''        auto fill = controlGrey();\n        if (isButtonDown)\n            fill = warmWhite();\n        else if (isHighlighted)\n            fill = fill.brighter (0.08f);\n''',
'''        auto fill = controlGrey();\n        const bool active = isButtonDown || button.getToggleState();\n        if (active)\n            fill = warmWhite();\n        else if (isHighlighted)\n            fill = fill.brighter (0.08f);\n''', 1)

s = s.replace(
'''        auto colour = button.findColour (juce::TextButton::textColourOffId);\n        if (isButtonDown)\n            colour = panelBlack();\n''',
'''        auto colour = button.findColour (juce::TextButton::textColourOffId);\n        if (isButtonDown || button.getToggleState())\n            colour = panelBlack();\n''', 1)

# Mouse keyboard should never steal the host's computer-keyboard focus.
s = s.replace(
'''    explicit JiantTouchKeyboard (JiantProcessor& p) : processor (p)\n    {\n        setMouseCursor (juce::MouseCursor::PointingHandCursor);\n    }\n''',
'''    explicit JiantTouchKeyboard (JiantProcessor& p) : processor (p)\n    {\n        setMouseCursor (juce::MouseCursor::PointingHandCursor);\n        setWantsKeyboardFocus (false);\n        setMouseClickGrabsKeyboardFocus (false);\n    }\n''', 1)

old_knob = '''            slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);\n            slider->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);\n'''
new_knob = '''            slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);\n            slider->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);\n            slider->setWantsKeyboardFocus (false);\n            slider->setMouseClickGrabsKeyboardFocus (false);\n'''
if old_knob not in s:
    raise SystemExit("knob setup block changed")
s = s.replace(old_knob, new_knob, 1)

old_buttons = '''        for (int i = 0; i < kButtonCount; ++i)\n        {\n            auto button = std::make_unique<juce::TextButton> (kButtonLabels[static_cast<size_t> (i)]);\n            button->setClickingTogglesState (false);\n            button->setColour (juce::TextButton::textColourOffId,\n                               i == 11 ? juce::Colour::fromRGB (255, 72, 72) : juce::Colours::white.withAlpha (0.94f));\n            button->onStateChange = [this, i, b = button.get()] {\n                processor.setButtonFromUi (i, b->isDown());\n            };\n            addAndMakeVisible (*button);\n            buttons.push_back (std::move (button));\n        }\n'''
new_buttons = '''        for (int i = 0; i < kButtonCount; ++i)\n        {\n            auto button = std::make_unique<juce::TextButton> (kButtonLabels[static_cast<size_t> (i)]);\n            button->setClickingTogglesState (false);\n            button->setWantsKeyboardFocus (false);\n            button->setMouseClickGrabsKeyboardFocus (false);\n            button->setColour (juce::TextButton::textColourOffId,\n                               i == 11 ? juce::Colour::fromRGB (255, 72, 72) : juce::Colours::white.withAlpha (0.94f));\n            button->onStateChange = [this, i, b = button.get()] {\n                const bool down = b->isDown();\n\n                // OCT-/OCT+ stay momentary. The 12 function buttons can be latched\n                // by Shift-clicking them. A later click on the same button releases it.\n                if (i >= 12)\n                {\n                    processor.setButtonFromUi (i, down);\n                    return;\n                }\n\n                if (down)\n                {\n                    if (functionLatched[static_cast<size_t> (i)])\n                    {\n                        functionLatched[static_cast<size_t> (i)] = false;\n                        b->setToggleState (false, juce::dontSendNotification);\n                        processor.setButtonFromUi (i, false);\n                    }\n                    else if (juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown())\n                    {\n                        functionLatched[static_cast<size_t> (i)] = true;\n                        b->setToggleState (true, juce::dontSendNotification);\n                        processor.setButtonFromUi (i, true);\n                    }\n                    else\n                    {\n                        processor.setButtonFromUi (i, true);\n                    }\n                    b->repaint();\n                }\n                else if (!functionLatched[static_cast<size_t> (i)])\n                {\n                    processor.setButtonFromUi (i, false);\n                }\n            };\n            addAndMakeVisible (*button);\n            buttons.push_back (std::move (button));\n        }\n'''
if old_buttons not in s:
    raise SystemExit("button setup block changed")
s = s.replace(old_buttons, new_buttons, 1)

old_fields = '''    std::vector<std::unique_ptr<juce::Slider>> knobs;\n    std::vector<std::unique_ptr<juce::Label>> knobLabels;\n    std::vector<std::unique_ptr<juce::TextButton>> buttons;\n'''
new_fields = '''    std::vector<std::unique_ptr<juce::Slider>> knobs;\n    std::vector<std::unique_ptr<juce::Label>> knobLabels;\n    std::vector<std::unique_ptr<juce::TextButton>> buttons;\n    std::array<bool, 12> functionLatched {};\n'''
if old_fields not in s:
    raise SystemExit("editor field block changed")
s = s.replace(old_fields, new_fields, 1)

p.write_text(s, encoding="utf-8")
print(f"patched Shift-latch buttons and keyboard focus in {p}")

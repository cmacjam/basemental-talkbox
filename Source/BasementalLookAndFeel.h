#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace Colours_
{
    const juce::Colour tolex      { 0xff16131b };   // purple-black
    const juce::Colour panel      { 0xff221e29 };
    const juce::Colour panelEdge  { 0xff3a3344 };
    const juce::Colour aluminium  { 0xffc9c7c2 };
    const juce::Colour feltRed    { 0xffc2185b };   // magenta (LEDs, stripe)
    const juce::Colour accent     { 0xfff0a830 };   // gold
    const juce::Colour cream      { 0xffeee3c8 };
    const juce::Colour textDim    { 0xff9a90a6 };
    const juce::Colour tubeClear  { 0xffbfd8e6 };   // the talk box hose
}

class BasementalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BasementalLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, Colours_::cream);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, Colours_::cream);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff121110));
        setColour (juce::ComboBox::outlineColourId, Colours_::panelEdge);
        setColour (juce::ComboBox::textColourId, Colours_::cream);
        setColour (juce::ComboBox::arrowColourId, Colours_::accent);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1d1b19));
        setColour (juce::PopupMenu::textColourId, Colours_::cream);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours_::accent.withAlpha (0.35f));
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff141312));
        setColour (juce::TextButton::buttonOnColourId, Colours_::accent);
        setColour (juce::TextButton::textColourOffId, Colours_::textDim);
        setColour (juce::TextButton::textColourOnId, juce::Colour (0xff1a1817));
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff121110));
        setColour (juce::TooltipWindow::textColourId, Colours_::cream);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace juce;
        auto bounds = Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
        const float size = jmin (bounds.getWidth(), bounds.getHeight());
        auto r = bounds.withSizeKeepingCentre (size, size);
        const auto c = r.getCentre();
        const float radius = size * 0.5f;
        const float angle = startAngle + pos * (endAngle - startAngle);

        // scale ticks
        g.setColour (Colours_::textDim.withAlpha (0.6f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
            const Point<float> p1 = c.getPointOnCircumference (radius - 1.0f, a);
            const Point<float> p2 = c.getPointOnCircumference (radius - (i % 5 == 0 ? 6.0f : 4.0f), a);
            g.drawLine ({ p1, p2 }, 1.0f);
        }

        // value arc
        const bool enabled = s.isEnabled();
        Path arc;
        arc.addCentredArc (c.x, c.y, radius - 9.0f, radius - 9.0f, 0.0f, startAngle, angle, true);
        g.setColour (enabled ? Colours_::accent : Colours_::textDim);
        g.strokePath (arc, PathStrokeType (2.5f, PathStrokeType::curved, PathStrokeType::rounded));

        // knob body (black skirted, like amp knobs)
        const float kr = radius - 13.0f;
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillEllipse (c.x - kr + 1.5f, c.y - kr + 2.5f, kr * 2, kr * 2);
        ColourGradient grad (Colour (0xff3c3935), c.x, c.y - kr, Colour (0xff0d0c0b), c.x, c.y + kr, false);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
        g.setColour (Colour (0xff57524b));
        g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.0f);

        // inner cap
        const float ir = kr * 0.62f;
        ColourGradient cap (Colour (0xff2d2a27), c.x, c.y - ir, Colour (0xff181614), c.x, c.y + ir, false);
        g.setGradientFill (cap);
        g.fillEllipse (c.x - ir, c.y - ir, ir * 2, ir * 2);

        // pointer
        g.setColour (enabled ? Colours_::cream : Colours_::textDim);
        const Point<float> pA = c.getPointOnCircumference (kr * 0.25f, angle);
        const Point<float> pB = c.getPointOnCircumference (kr * 0.92f, angle);
        g.drawLine ({ pA, pB }, 2.4f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool) override
    {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();
        // LED
        const float d = 10.0f;
        Rectangle<float> led (r.getX() + 2.0f, r.getCentreY() - d * 0.5f, d, d);
        if (on)
        {
            g.setColour (Colours_::feltRed.brighter (0.6f).withAlpha (0.35f));
            g.fillEllipse (led.expanded (4.0f));
        }
        g.setColour (on ? Colour (0xffff3d8b) : Colour (0xff3a1426));
        g.fillEllipse (led);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.drawEllipse (led, 1.0f);

        g.setColour (highlighted ? Colours_::cream : Colours_::textDim);
        g.setFont (FontOptions (12.0f, Font::bold));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (d + 8.0f), Justification::centredLeft);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (14.0f, juce::Font::bold); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::FontOptions (12.5f, juce::Font::bold); }
};

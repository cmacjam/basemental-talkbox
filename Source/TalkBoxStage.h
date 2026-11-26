// Basemental Talk Box - the stage: an (original, generic) singer in profile with the talk box hose
// in their mouth, taped up a mic stand, running down to the talk box on the floor.
// Everything moves with the real signal:
//   lips/jaw  <- the mouth gate and the measured vowel (wide EE vs round OO)
//   hose      <- pulses of sound travel from the box up to the mouth with the synth level
//   box       <- driver cone throbs, LED lights while notes are held
//   gold face trim glows brighter while the singer is "talking"
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BasementalLookAndFeel.h"

class TalkBoxStage : public juce::Component
{
public:
    static constexpr float kW = 420.0f, kH = 352.0f;

    struct Frame
    {
        float open = 0, wide = 0.5f, out = 0, voice = 0;
        bool notes = false, voiceMode = true;
        juce::String vowelLabel;
    };

    void update (const Frame& f, float dt)
    {
        // smooth so the face moves like a face, not a meter
        auto glide = [dt] (float& v, float target, float tau) { v += (target - v) * juce::jmin (1.0f, dt / tau); };
        glide (open, f.open, 0.035f);
        glide (wide, f.wide, 0.09f);
        glide (out, juce::jlimit (0.0f, 1.0f, f.out * 2.2f), 0.05f);
        glide (voice, juce::jlimit (0.0f, 1.0f, f.voice * 4.0f), 0.05f);
        notes = f.notes; voiceMode = f.voiceMode; vowelLabel = f.vowelLabel;
        phase += dt * (0.8f + 1.6f * out);
        if (phase > 1000.0f) phase -= 1000.0f;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.addTransform (AffineTransform::scale ((float) getWidth() / kW, (float) getHeight() / kH));
        const auto gold = Colours_::accent, goldHi = Colour (0xfff7d37a);

        // --- stage: dark backdrop, spotlight, floor ---
        g.setColour (Colour (0xff100d14));
        g.fillRoundedRectangle (0, 0, kW, kH, 8);
        {
            ColourGradient spot (Colour (0xff3a2f48).withAlpha (0.85f), 165, 120, Colour (0x00100d14), 165 + 230, 120, true);
            g.setGradientFill (spot);
            g.fillEllipse (-70, -110, 470, 470);
        }
        g.setColour (Colour (0xff1d1823));
        g.fillRect (0.0f, 318.0f, kW, kH - 318.0f);
        g.setColour (gold.withAlpha (0.18f));
        g.drawLine (0, 318, kW, 318, 1.0f);

        // --- mic stand (behind the hose) ---
        const float poleX = 286.0f;
        g.setColour (Colour (0xff4a4552));
        g.fillRect (poleX - 2.0f, 150.0f, 4.0f, 165.0f);
        g.drawLine (poleX, 314, poleX - 26, 330, 3.0f);
        g.drawLine (poleX, 314, poleX + 26, 330, 3.0f);
        g.drawLine (poleX, 314, poleX + 2, 333, 3.0f);
        // boom/clip and mic body pointing at the mouth
        g.setColour (Colour (0xff5b5663));
        g.fillRoundedRectangle (240.0f, 145.0f, 50.0f, 8.0f, 3.0f);
        {
            ColourGradient grille (Colour (0xffd9d6dd), 222, 140, Colour (0xff77737d), 222, 158, false);
            g.setGradientFill (grille);
            g.fillRoundedRectangle (214.0f, 139.0f, 30.0f, 20.0f, 9.0f);
            g.setColour (Colour (0xff2a2730));
            for (int i = 0; i < 4; ++i) g.drawVerticalLine (219 + i * 6, 141.0f, 157.0f);
        }

        // --- the talk box on the floor ---
        const Rectangle<float> box (322.0f, 266.0f, 74.0f, 52.0f);
        {
            ColourGradient body (Colour (0xff2e2936), box.getX(), box.getY(), Colour (0xff18151d), box.getX(), box.getBottom(), false);
            g.setGradientFill (body);
            g.fillRoundedRectangle (box, 6.0f);
            g.setColour (gold.withAlpha (0.85f));
            g.drawRoundedRectangle (box, 6.0f, 1.3f);
            // driver cone (throbs with the sound)
            const float throb = 1.0f + 0.09f * out * (0.5f + 0.5f * std::sin (phase * 40.0f));
            const auto c = Point<float> (box.getX() + 24, box.getCentreY() + 4);
            g.setColour (Colour (0xff0c0a0f));
            g.fillEllipse (Rectangle<float> (34 * throb, 34 * throb).withCentre (c));
            g.setColour (gold.withAlpha (0.35f + 0.5f * out));
            g.drawEllipse (Rectangle<float> (26 * throb, 26 * throb).withCentre (c), 1.5f);
            g.drawEllipse (Rectangle<float> (12 * throb, 12 * throb).withCentre (c), 1.2f);
            // LED + label
            g.setColour (notes ? Colour (0xffff3d8b) : Colour (0xff3a1426));
            g.fillEllipse (box.getRight() - 16, box.getY() + 8, 7, 7);
            if (notes) { g.setColour (Colour (0x55ff3d8b)); g.fillEllipse (box.getRight() - 19, box.getY() + 5, 13, 13); }
            g.setColour (Colours_::textDim);
            g.setFont (FontOptions (9.0f, Font::bold));
            g.setFont (FontOptions (8.0f, Font::bold));
            g.drawText ("TALK", Rectangle<float> (box.getX() + 44, box.getBottom() - 26, 28, 10), Justification::centred);
            g.drawText ("BOX", Rectangle<float> (box.getX() + 44, box.getBottom() - 16, 28, 10), Justification::centred);
            // coupler on top where the hose screws in
            g.setColour (Colour (0xff6c6774));
            g.fillRoundedRectangle (box.getX() + 46, box.getY() - 8, 12, 10, 2);
            // cable to the keyboard (off stage, right)
            Path cable;
            cable.startNewSubPath (box.getRight(), box.getBottom() - 10);
            cable.cubicTo (box.getRight() + 12, box.getBottom() - 4, kW - 14, 332, kW, 326);
            g.setColour (Colour (0xff0a090c));
            g.strokePath (cable, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (Colours_::textDim.withAlpha (0.8f));
            g.setFont (FontOptions (9.0f));
            g.drawText ("FROM SYNTH", Rectangle<float> (kW - 84, 334, 80, 12), Justification::centredRight);
        }

        // --- the singer (original stylised figure) ---
        const float jaw = 13.0f * open;                    // jaw drop
        const float lipFwd = 3.5f * (1.0f - wide) - 1.0f; // OO pushes lips forward, EE pulls back
        const float lipGap = 1.0f + 10.0f * open * (0.55f + 0.45f * (1.0f - std::abs (wide - 0.5f) * 1.2f));
        const float sway = 1.2f * std::sin (phase * 2.2f) * out;

        g.saveState();
        g.addTransform (AffineTransform::rotation (sway * 0.01f, 140, 300));

        // hair: a full, rounded natural, sitting on the head (drawn first, the face overlaps it)
        {
            Path hair;
            hair.addEllipse (70, 20, 120, 104);
            hair.addEllipse (62, 52, 70, 92);
            ColourGradient hg (Colour (0xff2a2128), 110, 30, Colour (0xff09070a), 120, 140, false);
            g.setGradientFill (hg);
            g.fillPath (hair);
            Random tex (7);   // a little curl texture
            g.setColour (Colour (0xff3a2e37).withAlpha (0.55f));
            for (int i = 0; i < 140; ++i)
            {
                const float a = tex.nextFloat() * MathConstants<float>::twoPi, r = std::sqrt (tex.nextFloat());
                const float hx = 128 + std::cos (a) * 58 * r, hy = 72 + std::sin (a) * 50 * r;
                if (hair.contains (hx, hy)) g.fillEllipse (hx, hy, 2.2f, 2.2f);
            }
        }

        Path face;
        face.startNewSubPath (110, 62);
        face.cubicTo (150, 50, 182, 70, 188, 96);                 // crown -> forehead
        face.quadraticTo (194, 104, 192, 112);                    // brow
        face.quadraticTo (196, 120, 206, 130);                    // nose
        face.quadraticTo (204, 136, 195, 137);                    // under the nose
        face.quadraticTo (197 + lipFwd, 139, 199 + lipFwd, 142 - lipGap * 0.25f);   // upper lip
        face.lineTo (192, 145);                                   // mouth corner
        face.lineTo (198 + lipFwd, 148 + lipGap * 0.75f + jaw * 0.6f);              // lower lip
        face.quadraticTo (196, 156 + jaw, 191, 163 + jaw);        // chin
        face.quadraticTo (182, 176 + jaw, 168, 178 + jaw * 0.5f); // jaw line
        face.lineTo (172, 202);                                   // throat
        face.cubicTo (196, 214, 218, 236, 222, 260);              // chest
        face.lineTo (230, 352);
        face.lineTo (48, 352);
        face.cubicTo (52, 270, 70, 238, 112, 214);                // shoulder / back
        face.lineTo (124, 196);                                   // back of neck
        face.cubicTo (100, 168, 86, 132, 92, 98);                 // back of head
        face.quadraticTo (96, 72, 110, 62);
        face.closeSubPath();

        {
            ColourGradient skin (Colour (0xff3b2a33), 120, 80, Colour (0xff1a1419), 200, 320, false);
            g.setGradientFill (skin);
            g.fillPath (face);
            // rim light from the spot
            g.setColour (Colour (0xff6a4f5c).withAlpha (0.45f));
            g.strokePath (face, PathStrokeType (3.0f), AffineTransform::translation (1.2f, -0.8f));
        }

        // the mouth cavity (dark gap between the lips)
        {
            Path cavity;
            cavity.startNewSubPath (199 + lipFwd, 142 - lipGap * 0.25f);
            cavity.lineTo (192, 145);
            cavity.lineTo (198 + lipFwd, 148 + lipGap * 0.75f + jaw * 0.6f);
            cavity.closeSubPath();
            g.setColour (Colour (0xff09060a));
            g.fillPath (cavity);
        }

        // shades
        g.setColour (Colour (0xff050406));
        g.fillRoundedRectangle (166.0f, 102.0f, 26.0f, 9.0f, 3.5f);
        g.setColour (goldHi.withAlpha (0.55f));
        g.drawLine (170, 104.5f, 180, 104.5f, 1.0f);
        g.setColour (Colour (0xff050406));
        g.drawLine (166, 106, 132, 104, 2.0f);   // arm to the ear
        g.fillEllipse (124, 108, 12, 18);        // ear

        // GOLD TRIM around the face, glowing when the singer talks
        {
            const float glow = juce::jlimit (0.0f, 1.0f, 0.35f + 0.65f * juce::jmax (open, voice));
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (gold.withAlpha (0.06f * glow * (float) (4 - i)));
                g.strokePath (face, PathStrokeType (2.0f + 2.5f * (float) i, PathStrokeType::curved, PathStrokeType::rounded));
            }
            ColourGradient trim (goldHi, 190, 90, Colour (0xffb07a1e), 120, 300, false);
            g.setGradientFill (trim);
            g.strokePath (face, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.restoreState();

        // --- the hose: mouth -> along the mic -> down the stand -> along the floor -> into the box ---
        Path hose;
        hose.startNewSubPath (box.getX() + 52, box.getY() - 8);         // box coupler
        hose.cubicTo (box.getX() + 52, 236, poleX + 10, 330, poleX + 5, 300);
        hose.lineTo (poleX + 5, 172);
        hose.cubicTo (poleX + 5, 150, 270, 137, 248, 137);
        hose.lineTo (212, 138);
        hose.quadraticTo (202 + lipFwd, 139, 194, 144 + open * 2.0f);    // into the corner of the mouth
        const float len = hose.getLength();

        g.setColour (Colours_::tubeClear.withAlpha (0.16f + 0.25f * out));
        g.strokePath (hose, PathStrokeType (7.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Colours_::tubeClear.withAlpha (0.75f));
        g.strokePath (hose, PathStrokeType (1.3f, PathStrokeType::curved, PathStrokeType::rounded));
        // tape holding the hose to the stand and the mic
        g.setColour (Colour (0xffcfc8b8).withAlpha (0.8f));
        for (float y : { 200.0f, 260.0f }) g.fillRect (poleX - 3, y, 13.0f, 5.0f);
        g.fillRect (232.0f, 134.0f, 5.0f, 9.0f);

        // sound pulses travelling up the hose
        if (out > 0.01f)
        {
            for (int i = 0; i < 9; ++i)
            {
                const float t = std::fmod (phase * 0.55f + (float) i / 9.0f, 1.0f);
                const auto p = hose.getPointAlongPath (t * len);
                const float a = out * (0.35f + 0.65f * std::sin (t * MathConstants<float>::pi));
                g.setColour (goldHi.withAlpha (a * 0.9f));
                g.fillEllipse (Rectangle<float> (4.5f, 4.5f).withCentre (p));
                g.setColour (Colour (0xffff3d8b).withAlpha (a * 0.25f));
                g.fillEllipse (Rectangle<float> (11.0f, 11.0f).withCentre (p));
            }
        }

        // sound coming out of the mouth into the mic
        if (out * open > 0.02f)
        {
            for (int i = 0; i < 3; ++i)
            {
                const float t = std::fmod (phase * 1.4f + (float) i / 3.0f, 1.0f);
                const float r = 8.0f + 30.0f * t;
                Path arc;
                arc.addCentredArc (200, 146, r, r * 1.1f, 0.0f, MathConstants<float>::pi * 0.25f, MathConstants<float>::pi * 0.75f, true);
                g.setColour (gold.withAlpha ((1.0f - t) * out * open * 0.9f));
                g.strokePath (arc, PathStrokeType (1.4f));
            }
        }

        // mini meters (top right): VOICE in, MOUTH open, OUT
        {
            auto bar = [&] (float y, float v, const char* name, Colour c)
            {
                g.setColour (Colours_::textDim);
                g.setFont (FontOptions (8.5f, Font::bold));
                g.drawText (name, Rectangle<float> (292, y - 1, 40, 10), Justification::centredLeft);
                g.setColour (Colour (0xff0b090d)); g.fillRoundedRectangle (334, y, 74, 7, 2);
                g.setColour (c); g.fillRoundedRectangle (335, y + 1, 72 * jlimit (0.0f, 1.0f, v), 5, 2);
            };
            bar (12, voice, "VOICE", Colours_::tubeClear);
            bar (24, open, "MOUTH", Colour (0xffff3d8b));
            bar (36, out, "OUT", Colours_::accent);
        }

        // current vowel / mode tag
        g.setColour (Colours_::textDim);
        g.setFont (FontOptions (10.0f, Font::bold));
        g.drawText (voiceMode ? "MOUTH: YOUR VOICE" : "MOUTH: VOWEL " + vowelLabel, Rectangle<float> (12, 10, 200, 14), Justification::centredLeft);
    }

private:
    float open = 0, wide = 0.5f, out = 0, voice = 0, phase = 0;
    bool notes = false, voiceMode = true;
    juce::String vowelLabel;
};

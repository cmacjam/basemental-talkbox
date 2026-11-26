// Raj T Vocal - the stage: an (original, generic) singer in profile with the talk box hose
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
        g.fillRoundedRectangle (248.0f, 149.0f, 42.0f, 8.0f, 3.0f);
        {
            ColourGradient grille (Colour (0xffd9d6dd), 230, 143, Colour (0xff77737d), 230, 161, false);
            g.setGradientFill (grille);
            g.fillRoundedRectangle (222.0f, 143.0f, 30.0f, 20.0f, 9.0f);
            g.setColour (Colour (0xff2a2730));
            for (int i = 0; i < 4; ++i) g.drawVerticalLine (227 + i * 6, 145.0f, 161.0f);
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

        // --- the singer: an original, realistic man in profile, eyes closed, feeling it ---
        const float jaw = 12.0f * open;                    // jaw drop
        const float lipFwd = 3.0f * (1.0f - wide) - 0.8f;  // OO pushes the lips forward, EE pulls them back
        const float lipGap = 0.6f + 6.5f * open * (0.6f + 0.4f * (1.0f - std::abs (wide - 0.5f) * 1.2f));
        const float sway = std::sin (phase * 2.2f) * out;

        g.saveState();
        g.addTransform (AffineTransform::rotation (sway * 0.012f, 150, 330));

        const Colour skinLight (0xff9a6a50), skinMid (0xff7a4e3a), skinDark (0xff4a2c22), lipCol (0xff6a3631);

        // neck (trapezius at the back, Adam's apple at the front)
        Path neck;
        neck.startNewSubPath (116, 160);                                      // nape
        neck.cubicTo (114, 190, 108, 212, 96, 236);
        neck.lineTo (196, 236);
        neck.cubicTo (186, 222, 180, 210, 178, 200 + jaw * 0.3f);
        neck.quadraticTo (182, 193 + jaw * 0.4f, 177, 187 + jaw * 0.6f);   // Adam's apple
        neck.lineTo (166, 180 + jaw * 0.5f);
        neck.lineTo (146, 164);
        neck.closeSubPath();
        {
            ColourGradient ng (skinMid, 170, 190, skinDark, 120, 200, false);
            g.setGradientFill (ng); g.fillPath (neck);
            g.setColour (Colours::black.withAlpha (0.35f));   // shadow under the jaw
            g.fillEllipse (138, 176 + jaw * 0.6f, 46, 14);
        }

        // shirt + jacket (over the base of the neck)
        {
            Path body;
            body.startNewSubPath (40, 352);
            body.cubicTo (44, 262, 80, 226, 112, 216);
            body.quadraticTo (150, 232, 186, 218);
            body.cubicTo (216, 228, 236, 252, 244, 290);
            body.lineTo (252, 352);
            body.closeSubPath();
            ColourGradient jacket (Colour (0xff2b2230), 150, 220, Colour (0xff0e0b10), 140, 352, false);
            g.setGradientFill (jacket); g.fillPath (body);
            // open shirt collar and lapel edge
            Path collar;
            collar.startNewSubPath (150, 226); collar.lineTo (186, 218); collar.lineTo (174, 250); collar.closeSubPath();
            g.setColour (Colour (0xff5a1e34)); g.fillPath (collar);
            Path lapel; lapel.startNewSubPath (186, 222); lapel.cubicTo (200, 250, 204, 290, 206, 352);
            g.setColour (Colours_::accent.withAlpha (0.35f)); g.strokePath (lapel, PathStrokeType (1.2f));
            g.setColour (Colour (0xff3a2e40).withAlpha (0.6f));
            g.strokePath (body, PathStrokeType (1.0f));
            g.setColour (Colours::black.withAlpha (0.35f));      // collar shadow on the neck
            g.drawLine (114, 217, 184, 220, 2.0f);
        }

        // head + face profile
        Path head;
        head.startNewSubPath (122, 52);                                       // crown
        head.cubicTo (150, 40, 176, 52, 184, 74);                             // top of the head
        head.quadraticTo (188, 84, 190, 96);                                  // forehead
        head.quadraticTo (196, 101, 195, 107);                                // brow ridge
        head.quadraticTo (193, 112, 196, 116);                                // nasal root
        head.cubicTo (201, 124, 208, 130, 212, 137);                          // nose bridge -> tip
        head.quadraticTo (213, 142, 207, 143);                                // tip, underside
        head.quadraticTo (202, 143, 200, 145);                                // columella -> subnasale
        head.quadraticTo (200 + lipFwd * 0.5f, 148, 202 + lipFwd, 150 - lipGap * 0.3f);   // philtrum -> upper lip
        head.quadraticTo (199 + lipFwd, 153 - lipGap * 0.2f, 195, 153);       // upper lip into the corner
        head.lineTo (199 + lipFwd, 155 + lipGap * 0.7f + jaw * 0.55f);        // lower lip
        head.quadraticTo (203 + lipFwd, 159 + lipGap * 0.7f + jaw * 0.6f, 198, 163 + jaw * 0.75f);
        head.quadraticTo (195, 166 + jaw * 0.8f, 198, 171 + jaw);              // mentolabial fold -> chin
        head.quadraticTo (201, 180 + jaw, 191, 184 + jaw);                     // chin
        head.quadraticTo (176, 186 + jaw * 0.7f, 160, 180 + jaw * 0.35f);      // underside of the jaw
        head.quadraticTo (150, 176, 146, 164);                                // jaw angle
        head.quadraticTo (132, 162, 116, 160);                                // under the ear to the nape
        head.cubicTo (104, 150, 100, 132, 100, 110);                          // back of the skull
        head.cubicTo (100, 82, 106, 62, 122, 52);
        head.closeSubPath();
        {
            ColourGradient skin (skinLight, 196, 96, skinDark, 110, 150, true);
            g.setGradientFill (skin); g.fillPath (head);
            // soft highlights: forehead, cheekbone, nose bridge
            g.setColour (Colour (0xffc89474).withAlpha (0.28f));
            g.fillEllipse (176, 80, 16, 22);
            g.fillEllipse (170, 122, 22, 12);
            g.drawLine (199, 118, 208, 134, 2.2f);
            // cheek shadow + nasolabial fold
            ColourGradient cheek (skinDark.withAlpha (0.28f), 176, 148, skinDark.withAlpha (0.0f), 192, 162, true);
            g.setGradientFill (cheek);
            g.fillEllipse (156, 130, 40, 36);
            Path fold; fold.startNewSubPath (198, 144); fold.quadraticTo (190, 150, 188, 160 + jaw * 0.4f);
            g.setColour (skinDark.withAlpha (0.6f)); g.strokePath (fold, PathStrokeType (1.2f));
        }

        // lips (colour) + the dark gap when the mouth opens
        {
            Path upper; upper.startNewSubPath (200, 146);
            upper.quadraticTo (200 + lipFwd * 0.5f, 148, 202 + lipFwd, 150 - lipGap * 0.3f);
            upper.quadraticTo (199 + lipFwd, 153 - lipGap * 0.2f, 195, 153); upper.lineTo (196, 149); upper.closeSubPath();
            Path lower; lower.startNewSubPath (195, 153.5f + jaw * 0.5f);
            lower.lineTo (199 + lipFwd, 155 + lipGap * 0.7f + jaw * 0.55f);
            lower.quadraticTo (203 + lipFwd, 159 + lipGap * 0.7f + jaw * 0.6f, 198, 163 + jaw * 0.75f);
            lower.lineTo (194, 158 + jaw * 0.6f); lower.closeSubPath();
            g.setColour (lipCol); g.fillPath (upper); g.fillPath (lower);
            g.setColour (Colour (0xffb07a6a).withAlpha (0.35f));
            g.drawLine (198, 158 + lipGap * 0.7f + jaw * 0.55f, 201 + lipFwd, 158 + lipGap * 0.7f + jaw * 0.58f, 1.2f);
            Path cavity;
            cavity.startNewSubPath (202 + lipFwd, 150 - lipGap * 0.3f + 2.5f);
            cavity.lineTo (195, 153);
            cavity.lineTo (199 + lipFwd, 155 + lipGap * 0.7f + jaw * 0.55f - 0.5f);
            cavity.closeSubPath();
            g.setColour (Colour (0xff140809)); g.fillPath (cavity);
            if (open > 0.15f)   // a hint of teeth
            {
                g.setColour (Colour (0xffe8e0d4).withAlpha (jlimit (0.0f, 0.8f, open)));
                g.fillRect (198.5f + lipFwd * 0.7f, 150.5f - lipGap * 0.3f + 2.0f, 2.5f, 1.8f);
            }
        }

        // nostril
        {
            Path n; n.startNewSubPath (201, 140); n.quadraticTo (205, 137, 208, 140);
            g.setColour (skinDark.withAlpha (0.9f)); g.strokePath (n, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
        }

        // closed eye with lashes, eyebrow
        {
            Path lid; lid.startNewSubPath (176, 114); lid.quadraticTo (184, 119, 191, 114);
            g.setColour (Colour (0xff2a1712)); g.strokePath (lid, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
            for (int i = 0; i < 5; ++i)
            {
                const float t = 0.2f + 0.15f * (float) i, x = 176 + 15 * t, y = 114 + 4.5f * std::sin (t * MathConstants<float>::pi);
                g.drawLine (x, y, x - 1.5f, y + 3.0f, 1.0f);
            }
            g.setColour (skinDark.withAlpha (0.45f));
            g.fillEllipse (174, 106, 18, 8);                              // eyelid crease shadow
            Path brow; brow.startNewSubPath (170, 103); brow.quadraticTo (182, 96, 194, 101);
            g.setColour (Colour (0xff17100d)); g.strokePath (brow, PathStrokeType (3.2f, PathStrokeType::curved, PathStrokeType::rounded));
        }

        // ear
        {
            Path ear;
            ear.startNewSubPath (140, 116);
            ear.cubicTo (130, 112, 126, 130, 130, 144);
            ear.cubicTo (133, 154, 142, 156, 145, 150);
            ear.cubicTo (148, 142, 150, 124, 140, 116);
            g.setColour (skinMid); g.fillPath (ear);
            g.setColour (skinDark.withAlpha (0.9f)); g.strokePath (ear, PathStrokeType (1.2f));
            Path inner; inner.startNewSubPath (139, 122); inner.cubicTo (133, 124, 134, 138, 137, 144);
            inner.quadraticTo (142, 146, 142, 138);
            g.strokePath (inner, PathStrokeType (1.0f));
        }

        // hair: short natural cut with a clean hairline and sideburn, fine texture
        {
            Path hair;
            hair.startNewSubPath (118, 50);
            hair.cubicTo (150, 34, 182, 46, 188, 76);
            hair.quadraticTo (182, 74, 176, 78);           // hairline at the forehead
            hair.cubicTo (162, 84, 150, 98, 148, 114);      // temple
            hair.lineTo (150, 134);                         // sideburn
            hair.lineTo (144, 134);
            hair.quadraticTo (140, 118, 132, 114);
            hair.cubicTo (118, 118, 104, 140, 102, 120);
            hair.cubicTo (98, 90, 104, 62, 118, 50);
            hair.closeSubPath();
            ColourGradient hg (Colour (0xff2a1f1c), 150, 44, Colour (0xff0b0807), 110, 120, false);
            g.setGradientFill (hg); g.fillPath (hair);
            Random tex (5);
            for (int i = 0; i < 420; ++i)
            {
                const float hx = 100 + tex.nextFloat() * 90, hy = 36 + tex.nextFloat() * 100;
                if (hair.contains (hx, hy))
                {
                    g.setColour ((tex.nextBool() ? Colour (0xff3d2e28) : Colour (0xff050303)).withAlpha (0.7f));
                    g.fillEllipse (hx, hy, 1.6f, 1.6f);
                }
            }
        }

        // the GOLD outline around the head and face, glowing when he sings
        Path outline;
        {
            // same contour as the head, but open: from the nape over the skull, down the face to the jaw
            outline.startNewSubPath (116, 160);
            outline.cubicTo (104, 150, 100, 132, 100, 110);
            outline.cubicTo (100, 82, 106, 62, 122, 52);
            outline.cubicTo (150, 40, 176, 52, 184, 74);
            outline.quadraticTo (188, 84, 190, 96);
            outline.quadraticTo (196, 101, 195, 107);
            outline.quadraticTo (193, 112, 196, 116);
            outline.cubicTo (201, 124, 208, 130, 212, 137);
            outline.quadraticTo (213, 142, 207, 143);
            outline.quadraticTo (202, 143, 200, 145);
            outline.quadraticTo (200 + lipFwd * 0.5f, 148, 202 + lipFwd, 150 - lipGap * 0.3f);
            outline.startNewSubPath (199 + lipFwd, 155 + lipGap * 0.7f + jaw * 0.55f);        // lips stay un-outlined
            outline.quadraticTo (203 + lipFwd, 159 + lipGap * 0.7f + jaw * 0.6f, 198, 163 + jaw * 0.75f);
            outline.quadraticTo (195, 166 + jaw * 0.8f, 198, 171 + jaw);
            outline.quadraticTo (201, 180 + jaw, 191, 184 + jaw);
            outline.quadraticTo (176, 186 + jaw * 0.7f, 160, 180 + jaw * 0.35f);
            outline.quadraticTo (150, 176, 146, 164);
        }
        {
            const float glow = jlimit (0.0f, 1.0f, 0.35f + 0.65f * jmax (open, voice));
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (gold.withAlpha (0.06f * glow * (float) (4 - i)));
                g.strokePath (outline, PathStrokeType (2.0f + 2.5f * (float) i, PathStrokeType::curved, PathStrokeType::rounded));
            }
            ColourGradient trim (goldHi, 200, 90, Colour (0xffb07a1e), 110, 200, false);
            g.setGradientFill (trim);
            g.strokePath (outline, PathStrokeType (1.7f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.restoreState();

        // --- the hose: mouth -> along the mic -> down the stand -> along the floor -> into the box ---
        Path hose;
        hose.startNewSubPath (box.getX() + 52, box.getY() - 8);         // box coupler
        hose.cubicTo (box.getX() + 52, 236, poleX + 10, 330, poleX + 5, 300);
        hose.lineTo (poleX + 5, 172);
        hose.cubicTo (poleX + 5, 152, 272, 141, 252, 141);
        hose.lineTo (216, 142);
        hose.quadraticTo (206 + lipFwd, 146, 197, 152 + open * 2.0f);    // into the corner of the mouth
        const float len = hose.getLength();

        g.setColour (Colours_::tubeClear.withAlpha (0.16f + 0.25f * out));
        g.strokePath (hose, PathStrokeType (7.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Colours_::tubeClear.withAlpha (0.75f));
        g.strokePath (hose, PathStrokeType (1.3f, PathStrokeType::curved, PathStrokeType::rounded));
        // tape holding the hose to the stand and the mic
        g.setColour (Colour (0xffcfc8b8).withAlpha (0.8f));
        for (float y : { 200.0f, 260.0f }) g.fillRect (poleX - 3, y, 13.0f, 5.0f);
        g.fillRect (240.0f, 138.0f, 5.0f, 9.0f);

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
                arc.addCentredArc (204, 152, r, r * 1.1f, 0.0f, MathConstants<float>::pi * 0.25f, MathConstants<float>::pi * 0.75f, true);
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

// Raj T Vocal - the stage: Raj T (from his photo) with the talk box hose
// in their mouth, taped up a mic stand, running down to the talk box on the floor.
// Everything moves with the real signal:
//   lips/jaw  <- the mouth gate and the measured vowel (wide EE vs round OO)
//   hose      <- pulses of sound travel from the box up to the mouth with the synth level
//   box       <- driver cone throbs, LED lights while notes are held
//   gold face trim glows brighter while the singer is "talking"
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BasementalLookAndFeel.h"
#include "BinaryData.h"

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

        // --- mic stand (behind the hose), mic at mouth height on his right ---
        const float poleX = 312.0f, micY = 248.0f;
        g.setColour (Colour (0xff4a4552));
        g.fillRect (poleX - 2.0f, micY, 4.0f, 314.0f - micY);
        g.drawLine (poleX, 314, poleX - 26, 330, 3.0f);
        g.drawLine (poleX, 314, poleX + 26, 330, 3.0f);
        g.drawLine (poleX, 314, poleX + 2, 333, 3.0f);
        g.setColour (Colour (0xff5b5663));
        g.fillRoundedRectangle (276.0f, micY - 4.0f, poleX - 274.0f, 8.0f, 3.0f);   // boom + clip
        {
            ColourGradient grille (Colour (0xffd9d6dd), 262, micY - 10, Colour (0xff77737d), 262, micY + 10, false);
            g.setGradientFill (grille);
            g.fillRoundedRectangle (250.0f, micY - 10.0f, 30.0f, 20.0f, 9.0f);
            g.setColour (Colour (0xff2a2730));
            for (int i = 0; i < 4; ++i) g.drawVerticalLine (255 + i * 6, micY - 8.0f, micY + 8.0f);
        }

        // --- the talk box on the floor ---
        const Rectangle<float> box (330.0f, 266.0f, 74.0f, 52.0f);
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

        // --- the singer: Raj T, from his photo (cut out, gold outline, jaw moves with the voice) ---
        const float jaw = 7.0f * open;   // how far the jaw drops, in stage pixels
        const float sway = std::sin (phase * 2.2f) * out;
        const float ps = 290.0f / 533.0f, pox = -8.0f, poy = 62.0f;       // photo placement (533 px source)
        auto P = [&] (float x, float y) { return Point<float> (pox + x * ps, poy + y * ps); };
        const auto mouthL = P (262, 360), mouthC = P (320, 373), mouthR = P (388, 362);
        const float chinBottom = P (0, 456).y;

        g.saveState();
        g.addTransform (AffineTransform::rotation (sway * 0.008f, 150, 352));
        if (photo.isValid())
        {
            const auto place = AffineTransform::scale (ps).translated (pox, poy);
            g.drawImageTransformed (photo, place);

            if (jaw > 0.25f)
            {
                // inside of the mouth
                Path inside;
                inside.startNewSubPath (mouthL);
                inside.quadraticTo (mouthC.x, mouthC.y - 1.0f, mouthR.x, mouthR.y);
                inside.quadraticTo (mouthC.x, mouthC.y + jaw * 1.9f, mouthL.x, mouthL.y);
                inside.closeSubPath();
                g.setColour (Colour (0xff1a0b0b)); g.fillPath (inside);
                g.setColour (Colour (0xffe9e1d6).withAlpha (jlimit (0.0f, 0.7f, open)));   // a hint of top teeth
                g.fillRoundedRectangle (mouthC.x - 11.0f, mouthC.y - 0.5f, 22.0f, jaw * 0.35f, 1.5f);

                // lower lip + chin: stretched down from the bottom of the chin, so no seams
                Path jawArea;
                bool first = true;
                for (auto [x, y] : { std::pair<float, float> { 256, 362 }, { 320, 374 }, { 394, 364 }, { 410, 398 }, { 392, 430 },
                                     { 352, 452 }, { 300, 458 }, { 262, 446 }, { 242, 410 } })
                {
                    if (first) jawArea.startNewSubPath (P (x, y)); else jawArea.lineTo (P (x, y));
                    first = false;
                }
                jawArea.closeSubPath();
                const float top = mouthC.y, k = (chinBottom - (top + jaw)) / (chinBottom - top);
                const auto stretch = AffineTransform::translation (0, -chinBottom).scaled (1.0f, k).translated (0, chinBottom);
                Path movedJaw (jawArea); movedJaw.applyTransform (stretch);
                g.saveState();
                g.reduceClipRegion (movedJaw);
                g.drawImageTransformed (photo, place.followedBy (stretch));
                g.restoreState();
            }
        }

        // the GOLD outline around his head and shoulders, glowing when he sings
        {
            static const float pts[][2] = { {0,322},{60,302},{118,292},{160,292},{158,268},{147,254},{139,234},{137,210},{140,188},
                {150,178},{163,183},{170,152},{177,118},{190,86},{210,58},{240,37},{275,23},{318,17},{360,21},{398,39},{425,69},
                {440,104},{448,150},{452,195},{452,236},{448,270},{446,300},{462,322},{500,338},{533,352} };
            Path outline;
            outline.startNewSubPath (P (pts[0][0], pts[0][1]));
            for (size_t i = 1; i < std::size (pts); ++i) outline.lineTo (P (pts[i][0], pts[i][1]));
            outline = outline.createPathWithRoundedCorners (6.0f);
            const float glow = jlimit (0.0f, 1.0f, 0.35f + 0.65f * jmax (open, voice));
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (gold.withAlpha (0.07f * glow * (float) (4 - i)));
                g.strokePath (outline, PathStrokeType (2.0f + 2.5f * (float) i, PathStrokeType::curved, PathStrokeType::rounded));
            }
            ColourGradient trim (goldHi, 200, 60, Colour (0xffb07a1e), 60, 260, false);
            g.setGradientFill (trim);
            g.strokePath (outline, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.restoreState();

        // --- the hose: mouth -> along the mic -> down the stand -> along the floor -> into the box ---
        Path hose;
        hose.startNewSubPath (box.getX() + 52, box.getY() - 8);         // box coupler
        hose.cubicTo (box.getX() + 52, 236, poleX + 10, 330, poleX + 5, 300);
        hose.lineTo (poleX + 5, micY + 18);
        hose.cubicTo (poleX + 5, micY - 4, 296, micY - 14, 276, micY - 14);
        hose.lineTo (252, micY - 13);
        hose.quadraticTo (226, micY - 12, mouthR.x - 2.0f, mouthR.y + jaw * 0.4f);   // into the corner of his mouth
        const float len = hose.getLength();

        g.setColour (Colours_::tubeClear.withAlpha (0.16f + 0.25f * out));
        g.strokePath (hose, PathStrokeType (7.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Colours_::tubeClear.withAlpha (0.75f));
        g.strokePath (hose, PathStrokeType (1.3f, PathStrokeType::curved, PathStrokeType::rounded));
        // tape holding the hose to the stand and the mic
        g.setColour (Colour (0xffcfc8b8).withAlpha (0.8f));
        for (float y : { 282.0f, 302.0f }) g.fillRect (poleX - 3, y, 13.0f, 5.0f);
        g.fillRect (266.0f, micY - 18.0f, 5.0f, 9.0f);

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
                arc.addCentredArc (mouthC.x, mouthC.y + 4.0f, r, r * 1.1f, 0.0f, MathConstants<float>::pi * 0.25f, MathConstants<float>::pi * 0.75f, true);
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
    juce::Image photo = juce::ImageCache::getFromMemory (BinaryData::raj_png, BinaryData::raj_pngSize);
    float open = 0, wide = 0.5f, out = 0, voice = 0, phase = 0;
    bool notes = false, voiceMode = true;
    juce::String vowelLabel;
};

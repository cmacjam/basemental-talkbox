// Basemental Talk Box - full signal chain (no JUCE dependency)
//   Carrier synth -> Driver -> Tube -> Mouth (your voice or the vowel model) -> + consonants/leak
//   -> Mic -> Doubler -> Room -> Volume
#pragma once
#include "CarrierSynth.h"
#include "Mouth.h"
#include "TalkBoxFX.h"

namespace btb
{
struct EngineParams
{
    SynthParams synth;
    MouthParams mouth;
    float drive      = 0.3f;
    float tube       = 0.35f;
    float consonants = 0.3f;   // VOICE mode: your real mouth noise (s, t, k) picked up by the mic
    float leak       = 0.0f;   // synth heard straight from the box, not through the mouth
    bool  doublerOn  = false;  float doubler = 0.5f;
    bool  roomOn     = false;  float roomMix = 0.2f; float roomSize = 0.4f;
    float volumeDb   = 0.0f;
};

class TalkBoxEngine
{
public:
    void prepare (double sampleRate, int /*maxBlock*/)
    {
        sr = sampleRate;
        synth.prepare (sr); mouth.prepare (sr);
        driver.prepare (sr); tube.prepare (sr); mic.prepare (sr);
        doubler.prepare (sr); room.prepare (sr);
        consHp1.highpass (sr, 3200.0, 0.707); consHp2.highpass (sr, 3200.0, 0.707);
        volume.prepare (sr, 30.0f); volume.reset (dbToGain (params.volumeDb));
        applyParams();
    }

    void reset()
    {
        synth.reset(); mouth.reset(); driver.reset(); tube.reset(); mic.reset();
        doubler.reset(); room.reset(); consHp1.reset(); consHp2.reset();
    }

    void setParams (const EngineParams& p) { params = p; applyParams(); }

    void noteOn (int note, float vel) { if (synth.noteOn (note, vel)) mouth.phraseStart(); }
    void noteOff (int note)           { synth.noteOff (note); }
    void allNotesOff()                { synth.allNotesOff(); }
    void pitchBend (float norm)       { synth.setPitchBend (norm); }
    void modWheel (float v)
    {
        // VOICE mode: wheel = vibrato. VOWEL mode: wheel sweeps the vowel, so you can "talk" with your left hand.
        synth.setModWheel (v);
        mouth.setVowelOffset (params.mouth.useVoice ? 0.0f : v * 4.0f);
    }

    int activeVoices() const { return synth.activeVoices(); }
    float voiceLevel() const { return mouth.voiceLevel(); }
    float gateValue() const  { return mouth.gateValue(); }
    float mouthOpen() const  { return mouth.mouthOpen(); }
    float mouthWide() const  { return mouth.mouthWide(); }
    int heldNotes() const    { return synth.heldNotes(); }
    float outputLevel() const { return outPeak; }

    // voice may be nullptr (no sidechain). L/R are written (not added to).
    void process (const float* voice, float* L, float* R, int n)
    {
        const bool useVoice = params.mouth.useVoice;
        float pk = outPeak * 0.97f;
        for (int i = 0; i < n; ++i)
        {
            const float v = voice ? voice[i] : 0.0f;
            mouth.pushVoice (useVoice ? v : 0.0f);
            mouth.tick();

            const float c = synth.next();
            const float drv = tube.process (driver.process (c));
            float y = mouth.filter (drv) * 0.9f;
            if (useVoice)
            {
                y *= mouth.gateValue();
                y += consHp2.process (consHp1.process (v)) * params.consonants * 2.2f;
            }
            y += drv * params.leak * 0.25f;
            y = mic.process (y);

            float l, r;
            if (params.doublerOn) doubler.process (y, l, r); else { doubler.process (y, l, r); l = r = y; }
            if (params.roomOn) room.process (l, r, l, r);

            const float g = volume.next() * kMakeup;
            L[i] = softClip (l * g); R[i] = softClip (r * g);
            pk = std::max (pk, std::max (std::abs (L[i]), std::abs (R[i])));
        }
        outPeak = pk;
    }

private:
    static constexpr float kMakeup = 0.13f;  // -18 dB: the mouth's formant peaks add a lot of gain
    // transparent below -3 dBFS, rounds off above, never exceeds 0 dBFS (speaker/ear safety on stage)
    static inline float softClip (float x)
    {
        const float t = 0.708f, a = std::abs (x);
        if (a <= t) return x;
        const float over = (a - t) / (1.0f - t);
        const float y = t + (1.0f - t) * over / (1.0f + over);
        return x < 0 ? -y : y;
    }

    void applyParams()
    {
        synth.setParams (params.synth);
        synth.setWheelIsVibrato (params.mouth.useVoice);
        mouth.setParams (params.mouth);
        driver.setDrive (params.drive);
        tube.setAmount (params.tube);
        doubler.amount = params.doubler;
        room.mix = params.roomMix; room.setSize (params.roomSize);
        volume.set (dbToGain (params.volumeDb));
    }

    double sr = 48000;
    EngineParams params;
    CarrierSynth synth;
    Mouth mouth;
    Driver driver; Tube tube; Mic mic;
    Doubler doubler; Room room;
    Biquad consHp1, consHp2;
    Smoothed volume;
    float outPeak = 0.0f;
};
} // namespace btb

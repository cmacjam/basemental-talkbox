// Raj T Vocal - full signal chain (no JUCE dependency)
//   Carrier synth -> Driver -> Tube -> Mouth (your voice or the vowel model) -> + consonants/leak
//   -> Mic -> Doubler -> Room -> Volume
#pragma once
#include "CarrierSynth.h"
#include "Mouth.h"
#include "TalkBoxFX.h"
#include "PitchTracker.h"

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
    float volume     = 0.5f;   // 0..1 output level (linear: 0.5 = -6 dB)
    // AUTO-TUNE (VOICE mode, when no keys are held)
    bool  autoTune   = true;
    int   key        = 0;      // 0 = C
    int   scale      = 0;      // see ScaleSnapper::mask
    float retune     = 0.35f;  // 0 = instant/robotic .. 1 = slow/natural
    int   octave     = 0;      // -2..+2
};

class TalkBoxEngine
{
public:
    void prepare (double sampleRate, int /*maxBlock*/)
    {
        sr = sampleRate;
        // At 88.2/96 kHz and above the mouth runs at half rate (44.1/48 kHz): that's the rate its
        // analysis is tuned for, it halves the CPU, and the talk box never passes anything above ~6 kHz anyway.
        mouthDec = sr > 60000.0 ? 2 : 1;
        synth.prepare (sr); mouth.prepare (sr / mouthDec);
        for (auto* b : { &decV1, &decV2, &decC1, &decC2, &intY1, &intY2 }) b->lowpass (sr, std::min (16000.0, sr * 0.2), 0.707);
        decPhase = 0; heldY = 0;
        driver.prepare (sr); tube.prepare (sr); mic.prepare (sr);
        doubler.prepare (sr); room.prepare (sr);
        consHp1.highpass (sr, 3200.0, 0.707); consHp2.highpass (sr, 3200.0, 0.707);
        voiceHp1.highpass (sr, 75.0, 0.707); voiceHp2.highpass (sr, 75.0, 0.707);   // rumble, hum, handling noise
        tracker.prepare (sr);
        snapper.reset();
        volume.prepare (sr, 30.0f); volume.reset (params.volume);
        autoCheck = std::max (16, (int) (sr * 0.0015));
        applyParams();
    }

    void reset()
    {
        synth.reset(); mouth.reset(); driver.reset(); tube.reset(); mic.reset();
        doubler.reset(); room.reset(); consHp1.reset(); consHp2.reset();
        voiceHp1.reset(); voiceHp2.reset(); tracker.reset(); snapper.reset();
        for (auto* b : { &decV1, &decV2, &decC1, &decC2, &intY1, &intY2 }) b->reset();
        autoActive = false; unvoiced = 0;
    }

    void setParams (const EngineParams& p) { params = p; applyParams(); }

    void noteOn (int note, float vel)
    {
        if (autoActive) { synth.autoStop(); autoActive = false; snapper.reset(); }   // the keys take over
        if (synth.noteOn (note, vel)) mouth.phraseStart();
    }
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
    bool  isAutoTuning() const { return autoActive; }
    float detectedMidi() const { return lastDetected; }   // what you sang (-1 = nothing)
    float correctedMidi() const { return autoActive ? lastTarget : -1.0f; }

    // voice may be nullptr (no sidechain). L/R are written (not added to).
    void process (const float* voice, float* L, float* R, int n)
    {
        const bool useVoice = params.mouth.useVoice;
        float pk = outPeak * 0.97f;
        for (int i = 0; i < n; ++i)
        {
            const float v = voice ? voiceHp2.process (voiceHp1.process (voice[i])) : 0.0f;
            if (useVoice) tracker.push (v);
            if (++autoCount >= autoCheck) { autoCount = 0; updateAutoTune (useVoice); }

            const float c = synth.next();
            const float drv = tube.process (driver.process (c));
            float y;
            if (mouthDec == 1)
            {
                mouth.pushVoice (useVoice ? v : 0.0f);
                mouth.tick();
                y = mouth.filter (drv) * 0.9f;
            }
            else
            {
                // decimate voice + carrier by 2, run the mouth, interpolate back up
                const float vd = decV2.process (decV1.process (useVoice ? v : 0.0f));
                const float cd = decC2.process (decC1.process (drv));
                if (++decPhase >= mouthDec)
                {
                    decPhase = 0;
                    mouth.pushVoice (vd);
                    mouth.tick();
                    heldY = mouth.filter (cd) * 0.9f * (float) mouthDec;   // zero-stuffing gain
                    y = intY2.process (intY1.process (heldY));
                }
                else y = intY2.process (intY1.process (0.0f));
            }
            if (useVoice)
            {
                const float gte = mouth.gateValue();
                y *= gte;
                // consonants (S, T, K): only while you're actually singing, so mic hiss never gets through
                y += consHp2.process (consHp1.process (v)) * params.consonants * 2.2f * gte * gte;
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
    void updateAutoTune (bool useVoice)
    {
        const float m = tracker.getMidi();
        lastDetected = (useVoice && mouth.gateValue() > 0.2f) ? m : -1.0f;
        if (! params.autoTune || ! useVoice)
        {
            if (autoActive) synth.autoStop();
            autoActive = false;
            return;
        }
        if (synth.heldNotes() > 0) { autoActive = false; return; }   // keys are playing: they own the pitch
        const float retuneSec = 0.002f * std::pow (100.0f, std::clamp (params.retune, 0.0f, 1.0f));   // 2 ms .. 200 ms
        const bool voiced = m > 0.0f && mouth.gateValue() > 0.3f && tracker.getConfidence() > 0.25f;
        if (voiced)
        {
            unvoiced = 0;
            const float target = snapper.snap (m, params.key, params.scale) + 12.0f * (float) params.octave;
            lastTarget = target;
            if (! autoActive) { synth.autoStart (target, 0.85f, retuneSec); autoActive = true; }
            else synth.autoSetTarget (target, retuneSec);
        }
        else if (autoActive)
        {
            // hold the note through consonants and short gaps, release when you stop singing
            unvoiced += autoCheck;
            if (unvoiced > (int) (0.25 * sr) || mouth.gateValue() < 0.02f)
            {
                synth.autoStop(); autoActive = false; snapper.reset();
            }
        }
    }

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
        volume.set (std::clamp (params.volume, 0.0f, 1.0f));
    }

    double sr = 48000;
    EngineParams params;
    CarrierSynth synth;
    Mouth mouth;
    Driver driver; Tube tube; Mic mic;
    Doubler doubler; Room room;
    Biquad consHp1, consHp2, voiceHp1, voiceHp2;
    Biquad decV1, decV2, decC1, decC2, intY1, intY2;
    int mouthDec = 1, decPhase = 0;
    float heldY = 0.0f;
    PitchTracker tracker;
    ScaleSnapper snapper;
    bool autoActive = false;
    int autoCount = 0, autoCheck = 64, unvoiced = 0;
    float lastDetected = -1.0f, lastTarget = -1.0f;
    Smoothed volume;
    float outPeak = 0.0f;
};
} // namespace btb

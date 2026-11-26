// Basemental Talk Box - the carrier synth (what goes up the tube).
// Two engines, like the two keyboards associated with the classic funk talk box sound:
//   MINI : three-oscillator subtractive mono synth (saws + sub square) into a 4-pole transistor ladder
//   DX   : two-operator FM with modulator self-feedback (the buzzy, bright, saw-like FM "talker" patch)
// Mono legato with glide by default (lead lines), or 8-voice poly for chords.
#pragma once
#include "DSPUtils.h"
#include <array>

namespace btb
{
struct SynthParams
{
    bool  dx        = false;   // false = MINI, true = DX
    bool  poly      = false;
    float tone      = 0.72f;   // MINI: cutoff   | DX: modulation index
    float character = 0.15f;   // MINI: resonance| DX: modulator feedback
    float envAmt    = 0.35f;   // contour amount on cutoff / index
    float detune    = 0.12f;   // second oscillator detune / chorus
    float sub       = 0.15f;   // sub-octave level
    float attack    = 0.004f;  // seconds
    float decay     = 0.45f;   // seconds
    float sustain   = 0.8f;    // 0..1
    float release   = 0.12f;   // seconds
    float glide     = 0.06f;   // seconds (mono, legato notes only)
    float vibrato   = 0.15f;   // delayed vibrato depth 0..1
    float bendRange = 2.0f;    // semitones
    float tuneA4    = 440.0f;
};

//------------------------------------------------------------------------------
struct ADSR
{
    enum Stage { Idle, Attack, Decay, Sustain, Release } stage = Idle;
    float value = 0.0f;
    double sr = 48000.0;
    float aCoef = 0, dCoef = 0, rCoef = 0, sus = 0.8f;

    static float coefFor (double sr, float t) { return (float) std::exp (-1.0 / (std::max (0.0005, (double) t) * sr * 0.25)); }
    void set (double s, float a, float d, float su, float r)
    {
        sr = s; aCoef = coefFor (sr, a * 0.6f); dCoef = coefFor (sr, d); rCoef = coefFor (sr, r); sus = su;
    }
    void gateOn()  { stage = Attack; }
    void gateOff() { if (stage != Idle) stage = Release; }
    void reset()   { stage = Idle; value = 0.0f; }
    bool active() const { return stage != Idle; }

    inline float next()
    {
        switch (stage)
        {
            case Attack:  value = 1.25f + aCoef * (value - 1.25f); if (value >= 1.0f) { value = 1.0f; stage = Decay; } break;
            case Decay:   value = sus + dCoef * (value - sus); if (std::abs (value - sus) < 1e-4f) stage = Sustain; break;
            case Sustain: value = sus; break;
            case Release: value *= rCoef; if (value < 1e-5f) { value = 0.0f; stage = Idle; } break;
            case Idle:    break;
        }
        return value;
    }
};

inline float polyBlep (float t, float dt)
{
    if (t < dt)        { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

//------------------------------------------------------------------------------
// 4-pole transistor-ladder style lowpass with tanh stages (Huovilainen-flavoured, simplified)
struct Ladder
{
    float s[4] { 0, 0, 0, 0 };
    float g = 0.5f, k = 0.0f;
    void reset() { s[0] = s[1] = s[2] = s[3] = 0; }
    void set (double sr, float fc, float reso)
    {
        fc = std::clamp (fc, 20.0f, (float) (sr * 0.42));
        g = (float) (1.0 - std::exp (-2.0 * kPi * fc / sr));
        k = 3.9f * std::clamp (reso, 0.0f, 1.0f);
    }
    inline float process (float x)
    {
        const float in = fastTanh (x * (1.0f + 0.5f * k) - k * s[3]);
        s[0] += g * (in - fastTanh (s[0]));
        s[1] += g * (fastTanh (s[0]) - fastTanh (s[1]));
        s[2] += g * (fastTanh (s[1]) - fastTanh (s[2]));
        s[3] += g * (fastTanh (s[2]) - s[3]);
        return s[3];
    }
};

//------------------------------------------------------------------------------
struct CarrierVoice
{
    int note = -1;
    float velocity = 0.8f;
    float pitch = 60.0f, targetPitch = 60.0f;   // in semitones (MIDI note numbers)
    float glideCoef = 0.0f;
    double age = 0;                             // for voice stealing
    float vibOnset = 0.0f;                      // 0 -> 1 after note start (delayed vibrato)

    // oscillators
    float ph1 = 0, ph2 = 0.37f, ph3 = 0.11f;    // MINI saw, saw, sub square
    float phC = 0, phM = 0, phC2 = 0.5f, phS = 0;  // DX carrier, modulator, detuned carrier, sub
    float fb1 = 0, fb2 = 0;                     // DX modulator feedback memory
    ADSR env;
    Ladder ladder;
    OnePoleLP dxSmooth;                         // tames the very top of the FM spectrum

    bool active() const { return env.active(); }

    void start (int n, float vel, bool retrigger, bool glide, float glideSec, double sr)
    {
        note = n; velocity = vel; targetPitch = (float) n;
        if (! glide || ! env.active()) pitch = (float) n;
        glideCoef = glide ? (float) std::exp (-1.0 / (std::max (0.001f, glideSec) * sr * 0.3)) : 0.0f;
        if (retrigger || ! env.active())
        {
            if (! env.active() || env.value < 0.01f) { ladder.reset(); fb1 = fb2 = 0; }
            env.gateOn();
            vibOnset = 0.0f;
        }
    }
    void stop() { env.gateOff(); }
    void kill() { env.reset(); note = -1; }

    inline float render (const SynthParams& p, double sr, float pitchMod, float vibOnsetInc)
    {
        if (! env.active()) return 0.0f;
        pitch = targetPitch + glideCoef * (pitch - targetPitch);
        vibOnset = std::min (1.0f, vibOnset + vibOnsetInc);
        const float e = env.next();
        const float semis = pitch + pitchMod;
        const float f = p.tuneA4 * std::exp2 ((semis - 69.0f) / 12.0f);
        const float dt = (float) (f / sr);
        const float detRatio = std::exp2 (p.detune * 0.25f / 12.0f);  // up to a quarter semitone
        const float velGain = 0.55f + 0.45f * velocity;

        float out;
        if (! p.dx)
        {
            // --- MINI: saw + detuned saw + sub square, into the ladder ---
            const float dt2 = dt * detRatio, dt3 = dt * 0.5f;
            float s1 = 2.0f * ph1 - 1.0f - polyBlep (ph1, dt);
            float s2 = 2.0f * ph2 - 1.0f - polyBlep (ph2, dt2);
            float sq = (ph3 < 0.5f ? 1.0f : -1.0f) + polyBlep (ph3, dt3) - polyBlep (std::fmod (ph3 + 0.5f, 1.0f), dt3);
            ph1 += dt;  if (ph1 >= 1.0f) ph1 -= 1.0f;
            ph2 += dt2; if (ph2 >= 1.0f) ph2 -= 1.0f;
            ph3 += dt3; if (ph3 >= 1.0f) ph3 -= 1.0f;
            const float mix = s1 * 0.55f + s2 * 0.55f * std::min (1.0f, p.detune * 4.0f) + sq * 0.6f * p.sub;

            const float keyTrack = std::exp2 ((pitch - 60.0f) / 24.0f);        // 50 % key tracking
            const float octaves  = p.tone * 7.6f + p.envAmt * 4.5f * e;
            const float fc = 70.0f * std::exp2 (octaves) * keyTrack;
            ladder.set (sr, fc, p.character);
            out = ladder.process (mix * 0.7f) * 1.6f;
        }
        else
        {
            // --- DX: feedback modulator -> carrier (ratio 1:1), plus a detuned carrier and a sub sine ---
            const float fbAmt = p.character * 1.6f;
            const float m = std::sin ((float) (2.0 * kPi) * phM + fbAmt * 0.5f * (fb1 + fb2));
            fb2 = fb1; fb1 = m;
            const float index = (0.25f + p.tone * 3.3f) * (1.0f - p.envAmt + p.envAmt * (0.35f + 0.65f * e));
            const float c1 = std::sin ((float) (2.0 * kPi) * phC + index * m);
            const float c2 = std::sin ((float) (2.0 * kPi) * phC2 + index * m);
            const float sb = std::sin ((float) (2.0 * kPi) * phS);
            phM += dt;              if (phM >= 1.0f) phM -= 1.0f;
            phC += dt;              if (phC >= 1.0f) phC -= 1.0f;
            phC2 += dt * detRatio;  if (phC2 >= 1.0f) phC2 -= 1.0f;
            phS += dt * 0.5f;       if (phS >= 1.0f) phS -= 1.0f;
            dxSmooth.set (sr, std::min (16000.0, sr * 0.4));
            out = dxSmooth.process (c1 * 0.7f + c2 * 0.7f * std::min (1.0f, p.detune * 4.0f) + sb * 0.6f * p.sub);
        }
        return out * e * velGain;
    }
};

//------------------------------------------------------------------------------
class CarrierSynth
{
public:
    static constexpr int kVoices = 8;

    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset()
    {
        for (auto& v : voices) v.kill();
        stackSize = 0; vibPhase = 0;
    }
    void setParams (const SynthParams& np)
    {
        if (np.poly != p.poly) reset();
        p = np;
        for (auto& v : voices) v.env.set (sr, p.attack, p.decay, p.sustain, p.release);
    }
    void setPitchBend (float norm) { bend = std::clamp (norm, -1.0f, 1.0f); }  // -1..1
    void setModWheel (float v) { modWheel = std::clamp (v, 0.0f, 1.0f); }
    void setWheelIsVibrato (bool b) { wheelVibrato = b; }

    // returns true when this note-on started a fresh phrase (no notes were held): the mouth "opens"
    bool noteOn (int note, float vel)
    {
        const bool fresh = (stackSize == 0);
        if (! p.poly)
        {
            pushNote (note);
            auto& v = voices[0];
            const bool legato = v.active() && ! fresh;
            v.start (note, vel, ! legato, legato, p.glide, sr);
            v.age = ++clock;
        }
        else
        {
            pushNote (note);
            CarrierVoice* target = nullptr;
            for (auto& v : voices) if (v.note == note && v.active()) target = &v;
            if (! target) for (auto& v : voices) if (! v.active()) { target = &v; break; }
            if (! target) { target = &voices[0]; for (auto& v : voices) if (v.age < target->age) target = &v; }
            target->start (note, vel, true, false, 0.0f, sr);
            target->age = ++clock;
        }
        return fresh;
    }

    void noteOff (int note)
    {
        removeNote (note);
        if (! p.poly)
        {
            auto& v = voices[0];
            if (stackSize > 0) { const int last = stack[(size_t) stackSize - 1]; if (v.note != last) v.start (last, v.velocity, false, true, p.glide, sr); }
            else v.stop();
        }
        else
            for (auto& v : voices) if (v.note == note) v.stop();
    }

    void allNotesOff() { stackSize = 0; for (auto& v : voices) v.stop(); }
    int heldNotes() const { return stackSize; }
    int activeVoices() const { int n = 0; for (auto& v : voices) n += v.active() ? 1 : 0; return n; }
    bool anyActive() const { return activeVoices() > 0; }

    inline float next()
    {
        vibPhase += 5.6 / sr; if (vibPhase >= 1.0) vibPhase -= 1.0;
        const float lfo = std::sin ((float) (2.0 * kPi * vibPhase));
        const float wheel = wheelVibrato ? modWheel : 0.0f;
        const float onsetInc = (float) (1.0 / (0.35 * sr));
        float out = 0.0f;
        const int n = p.poly ? kVoices : 1;
        for (int i = 0; i < n; ++i)
        {
            auto& v = voices[(size_t) i];
            if (! v.active()) continue;
            const float vibSemis = lfo * (0.45f * p.vibrato * v.vibOnset + 0.6f * wheel);
            out += v.render (p, sr, bend * p.bendRange + vibSemis, onsetInc);
        }
        return p.poly ? out * 0.45f : out;
    }

private:
    void pushNote (int n)
    {
        removeNote (n);
        if (stackSize < (int) stack.size()) stack[(size_t) stackSize++] = n;
        else { for (int i = 1; i < stackSize; ++i) stack[(size_t) i - 1] = stack[(size_t) i]; stack[(size_t) stackSize - 1] = n; }
    }
    void removeNote (int n)
    {
        int w = 0;
        for (int i = 0; i < stackSize; ++i) if (stack[(size_t) i] != n) stack[(size_t) w++] = stack[(size_t) i];
        stackSize = w;
    }

    double sr = 48000.0;
    SynthParams p;
    std::array<CarrierVoice, kVoices> voices;
    std::array<int, 32> stack {};
    int stackSize = 0;
    double clock = 0, vibPhase = 0;
    float bend = 0.0f, modWheel = 0.0f;
    bool wheelVibrato = true;
};
} // namespace btb

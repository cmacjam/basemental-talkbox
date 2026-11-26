// Basemental Talk Box - the hardware around the mouth:
//   Driver : the compression driver inside the box (band-limited, saturates when pushed)
//   Tube   : the plastic hose from driver to mouth (a faint, damped pipe resonance)
//   Mic    : a dynamic vocal mic a couple of centimetres from the lips
//   Doubler + Room : studio stacking and a small room
#pragma once
#include "DSPUtils.h"

namespace btb
{
struct Driver
{
    Biquad hp, lp, honk;
    float drive = 0.25f, pre = 1.0f, post = 1.0f;
    double sr = 48000;
    void prepare (double s) { sr = s; hp.highpass (sr, 140.0, 0.707); lp.lowpass (sr, 5600.0, 0.707); honk.peak (sr, 1900.0, 0.9, 2.5); reset(); }
    void reset() { hp.reset(); lp.reset(); honk.reset(); }
    void setDrive (float d)
    {
        drive = std::clamp (d, 0.0f, 1.0f);
        pre = 0.8f + drive * drive * 9.0f;
        post = 1.0f / fastTanh (pre * 0.6f) * 0.6f;
    }
    inline float process (float x)
    {
        x = hp.process (x);
        const float y = fastTanh (pre * x + 0.08f * drive) - fastTanh (0.08f * drive);   // slightly asymmetric
        return lp.process (honk.process (y * post));
    }
};

struct Tube
{
    DelayLine dl; OnePoleLP damp; float fb = 0.1f, delay = 500.0f;
    void prepare (double sr) { dl.prepare ((int) (sr * 0.02)); damp.set (sr, 2200.0); delay = (float) (sr * 0.0105); reset(); }
    void reset() { dl.reset(); damp.reset(); }
    void setAmount (float a) { fb = 0.5f * std::clamp (a, 0.0f, 1.0f); }
    inline float process (float x)
    {
        const float y = x + fb * damp.process (dl.read (delay));
        dl.push (y);
        return y * (1.0f - 0.45f * fb);
    }
};

struct Mic
{
    Biquad hp, presence, top;
    void prepare (double sr) { hp.highpass (sr, 85.0, 0.707); presence.peak (sr, 4800.0, 1.1, 3.0); top.lowpass (sr, 14000.0, 0.707); reset(); }
    void reset() { hp.reset(); presence.reset(); top.reset(); }
    inline float process (float x) { return top.process (presence.process (hp.process (x))); }
};

struct Doubler
{
    DelayLine l, r; double ph = 0, sr = 48000; float amount = 0.0f;
    void prepare (double s) { sr = s; l.prepare ((int) (sr * 0.04)); r.prepare ((int) (sr * 0.04)); reset(); }
    void reset() { l.reset(); r.reset(); ph = 0; }
    inline void process (float x, float& outL, float& outR)
    {
        l.push (x); r.push (x);
        ph += 1.0 / sr; if (ph > 1000.0) ph -= 1000.0;
        const float mL = (float) std::sin (2.0 * kPi * 0.31 * ph), mR = (float) std::sin (2.0 * kPi * 0.43 * ph + 1.3);
        const float dL = (float) sr * (0.0115f + 0.0016f * mL), dR = (float) sr * (0.0175f + 0.0019f * mR);
        const float wL = l.read (dL), wR = r.read (dR);
        outL = x + amount * (0.85f * wL - 0.25f * wR);
        outR = x + amount * (0.85f * wR - 0.25f * wL);
    }
};

// Four-line feedback delay network room
struct Room
{
    DelayLine d[4]; OnePoleLP damp[4]; Biquad inHp;
    float len[4] {}, fb = 0.5f, mix = 0.0f; double sr = 48000;
    void prepare (double s)
    {
        sr = s;
        for (auto& x : d) x.prepare ((int) (sr * 0.2));
        for (auto& x : damp) x.set (sr, 5200.0);
        inHp.highpass (sr, 220.0, 0.707);
        setSize (0.5f); reset();
    }
    void reset() { for (auto& x : d) x.reset(); for (auto& x : damp) x.reset(); inHp.reset(); }
    void setSize (float size)
    {
        static const float base[4] = { 0.0231f, 0.0293f, 0.0371f, 0.0437f };
        const float sc = 0.6f + 1.6f * std::clamp (size, 0.0f, 1.0f);
        for (int i = 0; i < 4; ++i) len[i] = (float) (base[i] * sc * sr);
        fb = 0.55f + 0.33f * size;
    }
    inline void process (float inL, float inR, float& outL, float& outR)
    {
        const float x = inHp.process (0.5f * (inL + inR));
        float s[4];
        for (int i = 0; i < 4; ++i) s[i] = damp[i].process (d[i].read (len[i]));
        // Hadamard mix
        const float h0 = s[0] + s[1] + s[2] + s[3], h1 = s[0] - s[1] + s[2] - s[3];
        const float h2 = s[0] + s[1] - s[2] - s[3], h3 = s[0] - s[1] - s[2] + s[3];
        d[0].push (x + 0.5f * fb * h0); d[1].push (x + 0.5f * fb * h1);
        d[2].push (x + 0.5f * fb * h2); d[3].push (x + 0.5f * fb * h3);
        outL = inL + mix * 0.6f * (s[0] + s[2]);
        outR = inR + mix * 0.6f * (s[1] + s[3]);
    }
};
} // namespace btb

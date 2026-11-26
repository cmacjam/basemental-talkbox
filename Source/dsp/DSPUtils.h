// Basemental Talk Box - small DSP building blocks (no JUCE dependency)
#pragma once
#include <cmath>
#include <vector>
#include <cstdint>
#include <algorithm>

namespace btb
{
constexpr double kPi = 3.14159265358979323846;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

// Decay coefficient for a per-sample multiplier that reaches -60 dB after t60 seconds
inline double t60ToCoef (double t60, double sr)
{
    if (t60 <= 0.0) return 0.0;
    return std::exp (-6.907755278982137 / (t60 * sr));
}

//------------------------------------------------------------------------------
struct Smoothed
{
    float current = 0.0f, target = 0.0f, coef = 0.0f;
    void prepare (double sr, float timeMs = 20.0f) { coef = (float) std::exp (-1.0 / (sr * timeMs * 0.001)); }
    void reset (float v) { current = target = v; }
    void set (float v) { target = v; }
    inline float next() { current = target + coef * (current - target); return current; }
};

//------------------------------------------------------------------------------
// Cheap deterministic noise
struct Noise
{
    uint32_t s = 0x12345678u;
    void seed (uint32_t v) { s = v ? v : 1u; }
    inline float next() // -1..1
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) ((int32_t) s) * (1.0f / 2147483648.0f);
    }
};

//------------------------------------------------------------------------------
// RBJ biquad, transposed direct form II
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }

    inline float process (float x)
    {
        float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setNorm (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }

    void lowpass (double sr, double f, double q)
    {
        double w = 2 * kPi * std::min (f, sr * 0.49) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        setNorm ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass (double sr, double f, double q)
    {
        double w = 2 * kPi * std::min (f, sr * 0.49) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        setNorm ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void bandpass (double sr, double f, double q)
    {
        double w = 2 * kPi * std::min (f, sr * 0.49) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        setNorm (al, 0, -al, 1 + al, -2 * c, 1 - al);
    }
    void peak (double sr, double f, double q, double gainDb)
    {
        double A = std::pow (10.0, gainDb / 40.0);
        double w = 2 * kPi * std::min (f, sr * 0.49) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        setNorm (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (double sr, double f, double gainDb, double S = 0.8)
    {
        double A = std::pow (10.0, gainDb / 40.0);
        double w = 2 * kPi * std::min (f, sr * 0.49) / sr, c = std::cos (w), s = std::sin (w);
        double al = s / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2);
        double sa = 2 * std::sqrt (A) * al;
        setNorm (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
                 (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
    }
    void highShelf (double sr, double f, double gainDb, double S = 0.8)
    {
        double A = std::pow (10.0, gainDb / 40.0);
        double w = 2 * kPi * std::min (f, sr * 0.49) / sr, c = std::cos (w), s = std::sin (w);
        double al = s / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2);
        double sa = 2 * std::sqrt (A) * al;
        setNorm (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
                 (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
    }
};

//------------------------------------------------------------------------------
struct OnePoleLP
{
    float a = 0, z = 0;
    void set (double sr, double f) { a = (float) std::exp (-2 * kPi * f / sr); }
    void reset() { z = 0; }
    inline float process (float x) { z = x + a * (z - x); return z; }
};

//------------------------------------------------------------------------------
// Fractional delay line with linear interpolation
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;

    void prepare (int maxSamples)
    {
        int n = 1;
        while (n < maxSamples + 4) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1; w = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    inline void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    // delay measured from the most recently pushed sample (d >= 1)
    inline float read (float d) const
    {
        float rp = (float) w - d;
        int i = (int) std::floor (rp);
        float fr = rp - (float) i;
        float a = buf[(size_t) ((i) & mask)], b = buf[(size_t) ((i + 1) & mask)];
        return a + fr * (b - a);
    }
    inline float readInt (int d) const { return buf[(size_t) ((w - d) & mask)]; }
};

//------------------------------------------------------------------------------
// Fast rational tanh (accurate to ~0.1% in [-3, 3], clamped beyond)
inline float fastTanh (float x)
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

} // namespace btb

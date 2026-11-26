// Raj T Vocal - pitch tracker for AUTO-TUNE (YIN, de Cheveigne & Kawahara 2002)
// Runs on a ~12 kHz decimated copy of the voice, analyses every ~8 ms over a ~29 ms window,
// covers 65 Hz .. 1000 Hz, and median-filters the result to kill octave flips.
#pragma once
#include "DSPUtils.h"
#include <array>

namespace btb
{
class PitchTracker
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        dec = std::max (1, (int) std::lround (sr / 12000.0));
        dsr = sr / dec;
        tauMin = (int) std::floor (dsr / 1000.0);
        tauMax = (int) std::ceil (dsr / 65.0);
        W = tauMax;                                   // integration window
        size = W + tauMax + 2;
        buf.assign ((size_t) size, 0.0f);
        frame.assign ((size_t) size, 0.0f);
        d.assign ((size_t) tauMax + 2, 0.0f);
        lp1.lowpass (sr, 1400.0, 0.707); lp2.lowpass (sr, 1400.0, 0.707);
        hopDec = std::max (1, (int) std::lround (dsr * 0.008));
        reset();
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        pos = 0; decCount = 0; hopCount = 0;
        hist.fill (0.0f); histN = 0;
        midi = -1.0f; confidence = 0.0f;
        lp1.reset(); lp2.reset();
    }

    inline void push (float x)
    {
        const float y = lp2.process (lp1.process (x));
        if (++decCount < dec) return;
        decCount = 0;
        buf[(size_t) pos] = y;
        if (++pos >= size) pos = 0;
        if (++hopCount >= hopDec) { hopCount = 0; analyse(); }
    }

    // latest estimate: MIDI note (fractional), or -1 when unvoiced
    float getMidi() const       { return midi; }
    float getConfidence() const { return confidence; }   // 0..1

private:
    void analyse()
    {
        for (int i = 0; i < size; ++i) { int k = pos + i; if (k >= size) k -= size; frame[(size_t) i] = buf[(size_t) k]; }

        // energy check: don't guess on silence
        double e = 0; for (int i = 0; i < W; ++i) e += (double) frame[(size_t) i] * frame[(size_t) i];
        if (e / W < 1e-7) { midi = -1.0f; confidence = 0.0f; histN = 0; return; }

        // difference function + cumulative mean normalisation
        d[0] = 1.0f;
        double running = 0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double acc = 0;
            for (int j = 0; j < W; ++j) { const float t = frame[(size_t) j] - frame[(size_t) (j + tau)]; acc += (double) t * t; }
            running += acc;
            d[(size_t) tau] = running > 0 ? (float) (acc * tau / running) : 1.0f;
        }
        // absolute threshold: first dip below 0.15, else the global minimum
        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (d[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < tauMax && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau;
                best = tau; break;
            }
        if (best < 0)
        {
            best = tauMin;
            for (int tau = tauMin; tau < tauMax; ++tau) if (d[(size_t) tau] < d[(size_t) best]) best = tau;
        }
        const float aperiodic = d[(size_t) best];
        confidence = std::clamp (1.0f - aperiodic * 2.5f, 0.0f, 1.0f);
        if (aperiodic > 0.35f) { midi = -1.0f; histN = 0; return; }

        // parabolic interpolation
        float t = (float) best;
        if (best > 1 && best < tauMax - 1)
        {
            const float a = d[(size_t) best - 1], b = d[(size_t) best], c = d[(size_t) best + 1];
            const float den = a - 2.0f * b + c;
            if (std::abs (den) > 1e-9f) t += 0.5f * (a - c) / den;
        }
        const float f0 = (float) (dsr / t);
        const float m = 69.0f + 12.0f * std::log2 (f0 / 440.0f);

        // median of the last 3 estimates
        hist[(size_t) (histN % 3)] = m; ++histN;
        if (histN >= 3)
        {
            float a = hist[0], b = hist[1], c = hist[2];
            midi = std::max (std::min (a, b), std::min (std::max (a, b), c));
        }
        else midi = m;
    }

    double sr = 48000, dsr = 12000;
    int dec = 4, tauMin = 12, tauMax = 185, W = 185, size = 372, pos = 0, decCount = 0, hopDec = 96, hopCount = 0;
    std::vector<float> buf, frame, d;
    Biquad lp1, lp2;
    std::array<float, 3> hist {};
    int histN = 0;
    float midi = -1.0f, confidence = 0.0f;
};

// Scale snapping with hysteresis (so a note sitting between two scale steps doesn't flutter)
struct ScaleSnapper
{
    // scale masks over 12 semitones (bit n = semitone n above the key is in the scale)
    static int mask (int scale)
    {
        static const int m[] = {
            0xFFF,                                                   // Chromatic
            (1<<0)|(1<<2)|(1<<4)|(1<<5)|(1<<7)|(1<<9)|(1<<11),       // Major
            (1<<0)|(1<<2)|(1<<3)|(1<<5)|(1<<7)|(1<<8)|(1<<10),       // Minor
            (1<<0)|(1<<2)|(1<<4)|(1<<7)|(1<<9),                      // Major pentatonic
            (1<<0)|(1<<3)|(1<<5)|(1<<7)|(1<<10),                     // Minor pentatonic
            (1<<0)|(1<<3)|(1<<5)|(1<<6)|(1<<7)|(1<<10),              // Blues
            (1<<0)|(1<<2)|(1<<3)|(1<<5)|(1<<7)|(1<<9)|(1<<10),       // Dorian
            (1<<0)|(1<<2)|(1<<4)|(1<<5)|(1<<7)|(1<<9)|(1<<10),       // Mixolydian
        };
        return m[std::clamp (scale, 0, 7)];
    }

    float snap (float m, int key, int scale)
    {
        const int msk = mask (scale);
        auto inScale = [&] (int n) { return (msk >> (((n - key) % 12 + 12) % 12)) & 1; };
        int bestN = (int) std::lround (m);
        float bestD = 1e9f;
        for (int n = (int) std::floor (m) - 3; n <= (int) std::ceil (m) + 3; ++n)
            if (inScale (n)) { const float dd = std::abs ((float) n - m); if (dd < bestD) { bestD = dd; bestN = n; } }
        // hysteresis: keep the current note unless we're clearly nearer the new one
        if (current >= 0 && current != bestN && inScale (current))
            if (std::abs (m - (float) current) < std::abs (m - (float) bestN) + 0.3f) bestN = current;
        current = bestN;
        return (float) bestN;
    }
    void reset() { current = -1; }
    int current = -1;
};
} // namespace btb

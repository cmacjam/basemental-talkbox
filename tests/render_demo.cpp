// Offline test harness (no JUCE): checks the mouth maths and renders demo WAVs.
//   g++ -O2 -std=c++17 render_demo.cpp -o render_demo && mkdir -p out && ./render_demo
#include "../Source/dsp/TalkBoxEngine.h"
#include <cstdio>
#include <string>
#include <vector>
#include <functional>
#include <complex>

using namespace btb;

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    const int n = (int) L.size();
    const int dataBytes = n * 2 * 4;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f); w32 (16); w16 (3); w16 (2); w32 (sr); w32 (sr * 8); w16 (8); w16 (32);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (int i = 0; i < n; ++i) { std::fwrite (&L[i], 4, 1, f); std::fwrite (&R[i], 4, 1, f); }
    std::fclose (f);
}

//------------------------------------------------------------------------------
// A crude synthetic "singer" for testing: glottal pulses (or noise) through 4 formants.
struct FakeVoice
{
    double sr; double ph = 0; Noise nz; Biquad f[4]; float prevOut = 0;
    float curF[4] { 750, 1180, 2500, 3450 };
    explicit FakeVoice (double s) : sr (s) { nz.seed (99); }
    static void vowelFormants (char v, float* out)
    {
        static const std::pair<char, std::array<float, 4>> T[] = {
            { 'a', { 750, 1180, 2500, 3450 } }, { 'e', { 540, 1800, 2500, 3500 } }, { 'i', { 290, 2250, 3000, 3700 } },
            { 'o', { 520, 880, 2410, 3300 } }, { 'u', { 300, 820, 2240, 3250 } } };
        for (auto& t : T) if (t.first == v) for (int i = 0; i < 4; ++i) out[i] = t.second[(size_t) i];
    }
    // f0 = 0 -> silence, f0 < 0 -> whisper/noise ("s")
    float next (float f0, char vowel, float level)
    {
        float tgt[4]; vowelFormants (vowel, tgt);
        for (int i = 0; i < 4; ++i) { curF[i] += 0.004f * (tgt[i] - curF[i]); f[i].bandpass (sr, curF[i], curF[i] / (60.0 + 20 * i)); }
        float src;
        if (f0 > 0) { ph += f0 / sr; if (ph >= 1) ph -= 1; const float t = (float) ph; src = t < 0.6f ? std::sin ((float) kPi * t / 0.6f) : -1.5f * std::sin ((float) kPi * (t - 0.6f) / 0.4f); src += 0.002f * nz.next(); }
        else if (f0 < 0) src = nz.next();
        else return 0.0f;
        float y = 0; const float g[4] = { 1.0f, 0.7f, 0.35f, 0.2f };
        for (int i = 0; i < 4; ++i) y += g[i] * f[i].process (src);
        if (f0 < 0) { static Biquad hp; static bool init = false; if (! init) { hp.highpass (sr, 4500, 0.7); init = true; } y = hp.process (src) * 0.5f; }
        // lip radiation: +6 dB/oct, so the net voice tilt is about -6 dB/oct like a real singer
        const float rad = (y - prevOut) * 300.0f; prevOut = y;
        return rad * level;
    }
};

struct Ev { double t; int type; int note; float val; }; // 0 on, 1 off, 2 bend, 3 wheel

static void render (TalkBoxEngine& e, std::vector<Ev> evs, const std::vector<float>* voice, double seconds, int sr,
                    std::vector<float>& L, std::vector<float>& R)
{
    std::sort (evs.begin(), evs.end(), [] (auto& a, auto& b) { return a.t < b.t; });
    const int total = (int) (seconds * sr);
    L.assign ((size_t) total, 0); R.assign ((size_t) total, 0);
    size_t ei = 0; int pos = 0;
    while (pos < total)
    {
        int next = total;
        if (ei < evs.size()) next = std::min (total, (int) (evs[ei].t * sr));
        if (next > pos)
        {
            e.process (voice ? voice->data() + pos : nullptr, L.data() + pos, R.data() + pos, next - pos);
            pos = next;
        }
        while (ei < evs.size() && (int) (evs[ei].t * sr) <= pos)
        {
            auto& ev = evs[ei++];
            if (ev.type == 0) e.noteOn (ev.note, ev.val);
            else if (ev.type == 1) e.noteOff (ev.note);
            else if (ev.type == 2) e.pitchBend (ev.val);
            else e.modWheel (ev.val);
        }
    }
}

static double bandEnergy (const std::vector<float>& x, int start, int len, int sr, double f1, double f2)
{
    // Goertzel sweep in 25 Hz steps
    double sum = 0;
    for (double f = f1; f <= f2; f += 25)
    {
        const double w = 2 * kPi * f / sr, c = 2 * std::cos (w);
        double s1 = 0, s2 = 0;
        for (int i = 0; i < len; ++i)
        {
            const double win = 0.5 - 0.5 * std::cos (2 * kPi * i / len);
            const double s = x[(size_t) (start + i)] * win + c * s1 - s2; s2 = s1; s1 = s;
        }
        sum += s1 * s1 + s2 * s2 - c * s1 * s2;
    }
    return sum;
}

static double rmsOf (const std::vector<float>& x, int start, int len)
{
    double s = 0; for (int i = 0; i < len; ++i) s += (double) x[(size_t) (start + i)] * x[(size_t) (start + i)];
    return std::sqrt (s / std::max (1, len));
}

static int pitchPeriod (const std::vector<float>& x, int start, int len, int minLag, int maxLag)
{
    std::vector<double> ac ((size_t) maxLag + 1, 0.0); double bv = -1e30;
    for (int l = minLag; l <= maxLag; ++l)
    {
        double s = 0; for (int i = 0; i < len - l; ++i) s += (double) x[(size_t) (start + i)] * x[(size_t) (start + i + l)];
        ac[(size_t) l] = s / (len - l); bv = std::max (bv, ac[(size_t) l]);
    }
    for (int l = minLag + 1; l < maxLag; ++l)   // first strong local peak = fundamental period
        if (ac[(size_t) l] > 0.85 * bv && ac[(size_t) l] >= ac[(size_t) l - 1] && ac[(size_t) l] >= ac[(size_t) l + 1]) return l;
    return maxLag;
}

static int failures = 0;
static void check (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", what); if (! ok) ++failures; }

int main (int argc, char** argv)
{
    const int sr = argc > 1 ? std::atoi (argv[1]) : 48000;
    std::vector<float> L, R;

    //--- 1. Lattice == direct-form 1/A(z) -------------------------------------------------
    std::printf ("1. lattice vs direct form\n");
    {
        Noise nz; nz.seed (5);
        std::vector<float> sig (4096);
        Biquad b1, b2; b1.bandpass (sr, 700, 6); b2.bandpass (sr, 1800, 8);
        for (auto& s : sig) { const float n = nz.next(); s = b1.process (n) + 0.5f * b2.process (n); }
        const int p = 16; double Rr[Mouth::kMaxOrder + 1]; float k[Mouth::kMaxOrder + 1]; double a[Mouth::kMaxOrder + 1];
        for (int l = 0; l <= p; ++l) { double acc = 0; for (size_t i = (size_t) l; i < sig.size(); ++i) acc += sig[i] * sig[i - (size_t) l]; Rr[l] = acc; }
        Mouth::levinson (Rr, p, k, a);
        // lattice
        float bd[Mouth::kMaxOrder + 1] = { 0 }; double hist[Mouth::kMaxOrder + 1] = { 0 }; double maxErr = 0;
        for (int n = 0; n < 2000; ++n)
        {
            const float x = n == 0 ? 1.0f : (n % 97 == 0 ? 0.3f : 0.0f);
            float f = x;
            for (int i = p; i >= 1; --i) { f -= k[i] * bd[i - 1]; bd[i] = bd[i - 1] + k[i] * f; }
            bd[0] = f;
            double y = x; for (int i = 1; i <= p; ++i) y -= a[i] * hist[i];
            for (int i = p; i >= 2; --i) hist[i] = hist[i - 1]; hist[1] = y;
            maxErr = std::max (maxErr, std::abs (y - f));
        }
        std::printf ("  max |lattice - direct| = %.2e\n", maxErr);
        check (maxErr < 1e-3, "lattice matches 1/A(z)");
    }

    //--- 2. Voice mode: synth pitch, singer's vowels ----------------------------------------
    std::printf ("2. voice mode tracking (carrier E4 = 330 Hz, fake singer at 118 Hz)\n");
    {
        const double segLen = 0.6;
        const char vowels[] = { 'a', 'i', 'u', 'e', 'o' };
        std::vector<float> voice ((size_t) (sr * 4.2), 0.0f);
        FakeVoice fv (sr);
        for (size_t i = 0; i < voice.size(); ++i)
        {
            const double t = (double) i / sr;
            const int seg = (int) (t / segLen);
            if (seg < 5) voice[i] = fv.next (118.0f, vowels[seg], 0.25f);
            else if (t < 3.3) voice[i] = 0.0f;                       // silence -> gate closes
            else if (t < 3.6) voice[i] = fv.next (-1.0f, 'a', 0.2f); // "sss"
        }
        TalkBoxEngine e; EngineParams p; p.mouth.useVoice = true; p.consonants = 0.0f; p.tube = 0.0f; p.drive = 0.1f;
        e.setParams (p); e.prepare (sr, 512);
        render (e, { { 0.0, 0, 64, 0.9f }, { 4.0, 1, 64, 0 } }, &voice, 4.2, sr, L, R);

        // level of each synth harmonic (330 Hz * k) vs the singer's true formant curve at that frequency
        auto harmonicDb = [&] (int st, int len, int k) { const double f = 329.63 * k; return 10 * std::log10 (bandEnergy (L, st, len, sr, f - 12, f + 12) + 1e-20); };
        auto refDb = [&] (char v, double f)
        {
            float F[4]; FakeVoice::vowelFormants (v, F); const float g[4] = { 1.0f, 0.7f, 0.35f, 0.2f };
            std::complex<double> H = 0; const std::complex<double> z1 = std::polar (1.0, -2 * kPi * f / sr), z2 = z1 * z1;
            for (int i = 0; i < 4; ++i) { Biquad b; b.bandpass (sr, F[i], F[i] / (60.0 + 20 * i)); H += (double) g[i] * ((double) b.b0 + (double) b.b1 * z1 + (double) b.b2 * z2) / (1.0 + (double) b.a1 * z1 + (double) b.a2 * z2); }
            return 20 * std::log10 (std::abs (H) + 1e-12);
        };
        auto detilt = [] (std::vector<double> v)   // remove the straight-line dB-vs-log(f) tilt: compare formant shape only
        {
            const size_t n = v.size(); double sx = 0, sy = 0, sxx = 0, sxy = 0;
            for (size_t i = 0; i < n; ++i) { const double x = std::log2 ((double) i + 1); sx += x; sy += v[i]; sxx += x * x; sxy += x * v[i]; }
            const double b = (n * sxy - sx * sy) / (n * sxx - sx * sx), a = (sy - b * sx) / n;
            for (size_t i = 0; i < n; ++i) v[i] -= a + b * std::log2 ((double) i + 1);
            return v;
        };
        auto corr = [&] (std::vector<double> a, std::vector<double> b)
        {
            a = detilt (a); b = detilt (b);
            double ma = 0, mb = 0; for (size_t i = 0; i < a.size(); ++i) { ma += a[i]; mb += b[i]; } ma /= a.size(); mb /= b.size();
            double sab = 0, saa = 0, sbb = 0; for (size_t i = 0; i < a.size(); ++i) { sab += (a[i] - ma) * (b[i] - mb); saa += (a[i] - ma) * (a[i] - ma); sbb += (b[i] - mb) * (b[i] - mb); }
            return sab / std::sqrt (saa * sbb + 1e-20);
        };
        int diagWins = 0;
        for (int s = 0; s < 5; ++s)
        {
            const int st = (int) ((s * segLen + 0.25) * sr), len = (int) (0.3 * sr);
            std::vector<double> out; for (int k = 1; k <= 11; ++k) out.push_back (harmonicDb (st, len, k));
            double c[5];
            for (int r = 0; r < 5; ++r) { std::vector<double> ref; for (int k = 1; k <= 11; ++k) ref.push_back (refDb (vowels[r], 329.63 * k)); c[r] = corr (out, ref); }
            int best = 0; for (int r = 1; r < 5; ++r) if (c[r] > c[best]) best = r;
            diagWins += best == s ? 1 : 0;
            const int per = pitchPeriod (L, st, 4800, sr / 500, sr / 90);
            std::printf ("  sang '%c': match to own vowel r=%.2f, best match '%c', pitch %.0f Hz\n", vowels[s], c[s], vowels[best], (double) sr / per);
            if (s == 0) check (std::abs ((double) sr / per - 329.6) < 8, "output pitch is the synth's, not the singer's");
        }
        std::printf ("  (info) best-match vowel correct for %d of 5\n", diagWins);
        {
            auto curveDb = [&] (char v, double f1, double f2)
            {
                FakeVoice fv2 (sr); Mouth m; m.prepare (sr); MouthParams mp; m.setParams (mp);
                for (int i = 0; i < sr / 2; ++i) { m.pushVoice (fv2.next (118.0f, v, 0.25f)); m.tick(); }
                std::vector<float> h (8192); for (int n = 0; n < 8192; ++n) h[(size_t) n] = m.filter (n == 0 ? 1.0f : 0.0f);
                double acc = 0; int cnt = 0;
                for (double f = f1; f <= f2; f += 25, ++cnt)
                {
                    std::complex<double> z = 0; for (int n = 0; n < 8192; ++n) z += (double) h[(size_t) n] * std::polar (1.0, -2 * kPi * f * n / sr);
                    acc += 20 * std::log10 (std::abs (z) + 1e-12);
                }
                return acc / cnt;
            };
            const double iHi = curveDb ('i', 2000, 2600), uHi = curveDb ('u', 2000, 2600), aLo = curveDb ('a', 650, 850), iLo = curveDb ('i', 650, 850);
            std::printf ("  mouth curve: 2-2.6 kHz ee %.1f / oo %.1f dB,  650-850 Hz ah %.1f / ee %.1f dB\n", iHi, uHi, aLo, iLo);
            check (iHi > uHi + 2.0, "'ee' shapes more 2-2.6 kHz than 'oo'");
            check (aLo > iLo + 3.0, "'ah' shapes more 650-850 Hz than 'ee'");
        }
        const double talk = rmsOf (L, (int) (1.5 * sr), sr / 4), quiet = rmsOf (L, (int) (3.15 * sr), sr / 10);
        std::printf ("  talking rms %.4f, silent rms %.6f (%.1f dB down)\n", talk, quiet, 20 * std::log10 (quiet / talk + 1e-12));
        check (quiet < talk * 0.03, "gate closes when you stop talking");
        writeWav ("out/test_voice_vowels.wav", L, R, sr);
    }

    //--- 3. Vowel mode, both synths, stability over the whole range ----------------------
    std::printf ("3. vowel mode + range sweep\n");
    for (bool dx : { false, true })
        for (float vowel : { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f })
        {
            TalkBoxEngine e; EngineParams p; p.mouth.useVoice = false; p.mouth.vowel = vowel; p.synth.dx = dx; p.drive = 0.6f;
            e.setParams (p); e.prepare (sr, 512);
            std::vector<Ev> ev; for (int k = 0; k < 6; ++k) { ev.push_back ({ 0.05 + k * 0.3, 0, 36 + k * 9, 1.0f }); ev.push_back ({ 0.3 + k * 0.3, 1, 36 + k * 9, 0 }); }
            render (e, ev, nullptr, 2.0, sr, L, R);
            float pk = 0; bool bad = false;
            for (size_t i = 0; i < L.size(); ++i) { pk = std::max (pk, std::abs (L[i])); if (! std::isfinite (L[i])) bad = true; }
            std::printf ("  %s vowel %.0f: peak %.3f (%.1f dBFS)%s\n", dx ? "DX  " : "MINI", vowel, pk, 20 * std::log10 (pk + 1e-9), bad ? " NaN!" : "");
            check (! bad && pk < 1.6f && pk > 0.05f, "finite, sensible level");
        }

    //--- 4. Demos -----------------------------------------------------------------------------
    std::printf ("4. demos\n");
    // a one-bar funk lick at 100 bpm, played four times (mono legato, with bends)
    auto lick = [] (double t0, std::vector<Ev>& ev, int transpose)
    {
        const double b = 0.6; // one beat
        struct N { double t, d; int n; };
        const N ns[] = { { 0, 0.45, 55 }, { 0.75, 0.2, 58 }, { 1.0, 0.45, 60 }, { 1.5, 0.2, 62 }, { 1.75, 0.2, 60 },
                         { 2.0, 0.9, 63 }, { 3.0, 0.25, 62 }, { 3.25, 0.25, 60 }, { 3.5, 0.45, 58 } };
        for (auto& x : ns) { ev.push_back ({ t0 + x.t * b, 0, x.n + transpose, 0.9f }); ev.push_back ({ t0 + (x.t + x.d) * b, 1, x.n + transpose, 0 }); }
        // scoop into the long note
        ev.push_back ({ t0 + 1.98 * b, 2, 0, -0.5f }); ev.push_back ({ t0 + 2.12 * b, 2, 0, 0.0f });
    };
    const double barLen = 2.4, songLen = barLen * 4 + 1.5;

    // VOWEL mode demo: mod wheel does the talking
    for (bool dx : { false, true })
    {
        TalkBoxEngine e; EngineParams p; p.mouth.useVoice = false; p.mouth.vowel = 0.6f; p.mouth.wah = 0.7f;
        p.synth.dx = dx; p.doublerOn = true; p.doubler = 0.45f; p.roomOn = true; p.roomMix = 0.18f;
        e.setParams (p); e.prepare (sr, 512);
        std::vector<Ev> ev;
        for (int bar = 0; bar < 4; ++bar) lick (0.2 + bar * barLen, ev, bar == 2 ? 5 : 0);
        for (int k = 0; k < 220; ++k) { const double t = k * 0.05; const float w = 0.5f + 0.5f * (float) std::sin (2 * kPi * t / 0.6 - 1.2); ev.push_back ({ t, 3, 0, 0.75f * w * w }); }
        render (e, ev, nullptr, songLen, sr, L, R);
        float pk = 0; for (size_t i = 0; i < L.size(); ++i) pk = std::max (pk, std::max (std::abs (L[i]), std::abs (R[i])));
        const char* name = dx ? "out/demo_vowel_mode_dx.wav" : "out/demo_vowel_mode_mini.wav";
        std::printf ("  %-36s peak %.1f dBFS\n", name, 20 * std::log10 (pk));
        writeWav (name, L, R, sr);
    }

    // VOICE mode demo: the fake singer "mouths" syllables in time with the lick
    {
        std::vector<float> voice ((size_t) (songLen * sr), 0.0f);
        FakeVoice fv (sr);
        const char syl[] = { 'u', 'a', 'e', 'i', 'a', 'o', 'u', 'a', 'e' };
        const double beat = 0.6;
        const double on[] = { 0, 0.75, 1.0, 1.5, 1.75, 2.0, 3.0, 3.25, 3.5 }, dur[] = { 0.45, 0.2, 0.45, 0.2, 0.2, 0.9, 0.25, 0.25, 0.45 };
        for (size_t i = 0; i < voice.size(); ++i)
        {
            const double t = (double) i / sr - 0.2; if (t < 0) continue;
            const int bar = (int) (t / barLen); if (bar > 3) continue;
            const double tb = (t - bar * barLen) / beat;
            char v = 0;
            for (int s = 0; s < 9; ++s) if (tb >= on[s] && tb < on[s] + dur[s]) v = syl[s];
            if (v == 0) { voice[i] = fv.next (0.0f, 'a', 0.0f); continue; }
            voice[i] = fv.next (110.0f, v, 0.22f);
        }
        for (bool dx : { false, true })
        {
            TalkBoxEngine e; EngineParams p; p.mouth.useVoice = true; p.synth.dx = dx; p.consonants = 0.0f;
            p.doublerOn = true; p.doubler = 0.45f; p.roomOn = true; p.roomMix = 0.18f;
            e.setParams (p); e.prepare (sr, 512);
            std::vector<Ev> ev; for (int bar = 0; bar < 4; ++bar) lick (0.2 + bar * barLen, ev, bar == 2 ? 5 : 0);
            render (e, ev, &voice, songLen, sr, L, R);
            float pk = 0; for (size_t i = 0; i < L.size(); ++i) pk = std::max (pk, std::max (std::abs (L[i]), std::abs (R[i])));
            const char* name = dx ? "out/demo_voice_mode_dx.wav" : "out/demo_voice_mode_mini.wav";
            std::printf ("  %-36s peak %.1f dBFS\n", name, 20 * std::log10 (pk));
            writeWav (name, L, R, sr);
        }
    }

    std::printf (failures ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", failures);
    return failures ? 1 : 0;
}

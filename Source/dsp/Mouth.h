// Basemental Talk Box - the mouth.
// A real talk box plays the synth into your mouth; your vocal tract (tongue, lips, jaw) filters it,
// and the mic hears the synth's pitch with your vowels and consonants on it.
//
// In software the vocal tract is measured from your voice on the sidechain with linear prediction
// (LPC): every ~2.7 ms we fit an all-pole model to the last ~22 ms of (pre-emphasised) voice. That
// all-pole filter is the shape of your mouth, without your pitch. The carrier is run through the same
// filter as a lattice whose reflection coefficients are interpolated sample by sample (always stable).
//
// VOWEL mode builds the same lattice from a formant table instead, so it talks with no mic at all.
#pragma once
#include "DSPUtils.h"
#include <array>

namespace btb
{
struct MouthParams
{
    bool  useVoice    = true;   // true = VOICE (sidechain), false = VOWEL (internal)
    float sensitivity = 0.6f;   // voice gate threshold
    float response    = 0.3f;   // articulation smoothing (0 = fastest)
    float brightness  = 0.9f;   // pre-emphasis: how much of the mouth's top end shapes the synth
    float vowel       = 2.0f;   // 0..4  U  O  A  E  I
    float wah         = 0.5f;   // VOWEL mode: how closed the mouth starts on each new phrase
    float talkTime    = 0.18f;  // seconds to open the mouth
};

class Mouth
{
public:
    static constexpr int kMaxOrder = 48;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        order = std::clamp ((int) std::lround (sr * 0.00042), 12, kMaxOrder);   // 20 @ 48 kHz
        win   = (int) std::lround (sr * 0.022);
        hop   = std::max (16, (int) std::lround (sr * 0.0027));
        onsetHops = (int) std::ceil (0.7 * win / hop);   // ~16 ms of voice before trusting a new frame
        ring.assign ((size_t) win, 0.0f);
        frame.assign ((size_t) win, 0.0f);
        window.resize ((size_t) win);
        for (int i = 0; i < win; ++i) window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * kPi * (i + 0.5) / win));
        lagWin.resize ((size_t) kMaxOrder + 1);
        for (int l = 0; l <= kMaxOrder; ++l)
        {
            const double b = 2.0 * kPi * 45.0 * l / sr;   // 45 Hz bandwidth expansion
            lagWin[(size_t) l] = std::exp (-0.5 * b * b);
        }
        atkC = (float) (1.0 - std::exp (-1.0 / (0.002 * sr)));
        relC = (float) (1.0 - std::exp (-1.0 / (0.030 * sr)));
        gUp  = (float) (1.0 - std::exp (-1.0 / (0.0015 * sr)));
        gDn  = (float) (1.0 - std::exp (-1.0 / (0.015 * sr)));
        reset();
    }

    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        ringPos = 0; hopCount = 0; prevVoice = 0;
        kCur.fill (0); kTgt.fill (0); kStep.fill (0); kSmooth.fill (0); bd.fill (0);
        gCur = gTgt = gSmooth = gNorm = 1.0f; gStep = 0;
        rms = 0; gateSmooth = 0; open = 1.0f;
        haveShape = false;
        // start on a neutral open vowel so the first note isn't silent in VOWEL mode
        setVowelShape (2.0f, 1.0f);
        kCur = kSmooth = kTgt; gCur = gSmooth = gTgt;
        { double pr = 1; for (int i = 1; i <= order; ++i) pr *= 1.0 - (double) kCur[(size_t) i] * kCur[(size_t) i]; gNorm = (float) std::sqrt (pr); }
    }

    void setParams (const MouthParams& np)
    {
        if (np.useVoice != p.useVoice) { gateSmooth = 0; }
        p = np;
        preEmph = 0.6f + 0.39f * std::clamp (p.brightness, 0.0f, 1.0f);   // 0.95 at the default
        thrDb = -22.0f - 40.0f * std::clamp (p.sensitivity, 0.0f, 1.0f);   // -22 .. -62 dBFS
        threshold = dbToGain (thrDb);
        smoothA = (float) std::exp (-1.0 / (1.0 + 7.0 * p.response));  // per-hop smoothing of the shape
        openCoef = (float) std::exp (-(double) hop / (std::max (0.01f, p.talkTime) * sr * 0.3));
    }

    // VOWEL mode: a new phrase starts -> close the mouth, then it opens over talkTime
    void phraseStart() { open = 1.0f - std::clamp (p.wah, 0.0f, 1.0f); }
    void setVowelOffset (float o) { vowelOffset = o; }

    // Feed one voice sample (VOICE mode). Returns nothing; analysis happens every hop.
    inline void pushVoice (float x)
    {
        // level follower on the raw voice: 2 ms attack, 80 ms release
        const float a = std::abs (x);
        rms += (a > rms ? atkC : relC) * (a - rms);
        const float pe = x - preEmph * prevVoice; prevVoice = x;
        ring[(size_t) ringPos] = pe;
        if (++ringPos >= win) ringPos = 0;
    }

    inline void tick()
    {
        if (rms < threshold * 0.12f) framesAbove = 0;
        if (++hopCount >= hop)
        {
            hopCount = 0;
            if (p.useVoice) analyseVoice();
            else
            {
                open = 1.0f + openCoef * (open - 1.0f);
                setVowelShape (std::clamp (p.vowel + vowelOffset, 0.0f, 4.0f), open);
            }
            // per-hop smoothing, then linear interpolation over the next hop
            for (int i = 1; i <= order; ++i)
            {
                kSmooth[(size_t) i] = kTgt[(size_t) i] + smoothA * (kSmooth[(size_t) i] - kTgt[(size_t) i]);
                kStep[(size_t) i] = (kSmooth[(size_t) i] - kCur[(size_t) i]) / (float) hop;
            }
            gSmooth = gTgt + smoothA * (gSmooth - gTgt);
            gStep = (gSmooth - gCur) / (float) hop;
        }
        // filter gain from the live coefficients: E/R0 = prod(1 - k^2), exact for every interpolated shape
        double prod = 1.0;
        for (int i = 1; i <= order; ++i)
        {
            kCur[(size_t) i] += kStep[(size_t) i];
            prod *= 1.0 - (double) kCur[(size_t) i] * kCur[(size_t) i];
        }
        gCur += gStep;   // extra level factor (VOWEL mode: lips closing)
        gNorm = (float) std::sqrt (prod);

        // voice gate (VOICE mode); VOWEL mode is always open
        // expander: closed below the threshold, fully open 15 dB above it
        float target = 1.0f;
        if (p.useVoice)
        {
            const float x = std::clamp ((20.0f * std::log10 (rms + 1e-9f) - thrDb) / 15.0f, 0.0f, 1.0f);
            target = x * x * (3.0f - 2.0f * x);
        }
        gateSmooth += (target > gateSmooth ? gUp : gDn) * (target - gateSmooth);
    }

    // Filter one carrier sample through the mouth (all-pole lattice).
    inline float filter (float exc)
    {
        float f = exc;
        for (int i = order; i >= 1; --i)
        {
            f -= kCur[(size_t) i] * bd[(size_t) i - 1];
            bd[(size_t) i] = bd[(size_t) i - 1] + kCur[(size_t) i] * f;
        }
        bd[0] = f;
        return f * gNorm * gCur;
    }

    float gateValue() const { return gateSmooth; }
    float voiceLevel() const { return rms; }
    int getOrder() const { return order; }
    const float* reflection() const { return kCur.data(); }
    float gain() const { return gNorm * gCur; }

    // ----- public for tests -----
    // Levinson-Durbin on autocorrelation R[0..p]. Writes reflection coeffs k[1..p], returns E/R0.
    static double levinson (const double* R, int p, float* kOut, double* aOut = nullptr)
    {
        double a[kMaxOrder + 1] = { 1.0 }, tmp[kMaxOrder + 1];
        double E = R[0];
        if (E <= 0) { for (int i = 1; i <= p; ++i) kOut[i] = 0; return 1.0; }
        for (int i = 1; i <= p; ++i)
        {
            double acc = R[i];
            for (int j = 1; j < i; ++j) acc += a[j] * R[i - j];
            double k = -acc / E;
            k = std::clamp (k, -0.9995, 0.9995);
            for (int j = 1; j < i; ++j) tmp[j] = a[j] + k * a[i - j];
            for (int j = 1; j < i; ++j) a[j] = tmp[j];
            a[i] = k;
            kOut[i] = (float) k;
            E *= (1.0 - k * k);
        }
        if (aOut) for (int i = 0; i <= p; ++i) aOut[i] = a[i];
        return E / R[0];
    }

private:
    void analyseVoice()
    {
        // below the gate floor keep the last mouth shape (release tails keep their vowel)
        if (rms < threshold * 0.12f) return;
        // need a frame that is mostly voice: wait until the level has been up for a moment
        if (++framesAbove < onsetHops) return;

        // window the last `win` samples, oldest first
        for (int i = 0; i < win; ++i)
        {
            int idx = ringPos + i; if (idx >= win) idx -= win;
            frame[(size_t) i] = ring[(size_t) idx] * window[(size_t) i];
        }
        double R[kMaxOrder + 1] = { 0 };
        for (int l = 0; l <= order; ++l)
        {
            double acc = 0;
            const float* x = frame.data();
            for (int n = l; n < win; ++n) acc += (double) x[n] * x[n - l];
            R[l] = acc * lagWin[(size_t) l];
        }
        if (R[0] < 1e-9) return;
        R[0] *= 1.0 + 1e-4;   // -40 dB white-noise floor keeps the poles sane
        float k[kMaxOrder + 1];
        const double ratio = levinson (R, order, k);
        (void) ratio;
        for (int i = 1; i <= order; ++i) kTgt[(size_t) i] = std::clamp (k[i], -0.995f, 0.995f);
        gTgt = 1.0f;
        if (! haveShape) { kSmooth = kTgt; kCur = kTgt; gSmooth = gCur = 1.0f; bd.fill (0); }   // first word: start right on it
        haveShape = true;
    }

    // Formant table: F1..F4 (Hz) and bandwidths, for U O A E I (a funk-singer's open, slightly dark voice)
    void setVowelShape (float v, float openAmt)
    {
        static const float F[5][4] = {
            { 300, 820, 2240, 3250 },   // U  (oo)
            { 520, 880, 2410, 3300 },   // O  (oh)
            { 750, 1180, 2500, 3450 },  // A  (ah)
            { 540, 1800, 2500, 3500 },  // E  (eh)
            { 290, 2250, 3000, 3700 },  // I  (ee)
        };
        static const float B[4] = { 70, 85, 130, 180 };
        static const float closed[4] = { 230, 650, 2200, 3200 };   // lips closed, "mm"
        const int i0 = std::clamp ((int) std::floor (v), 0, 3);
        const float fr = std::clamp (v - (float) i0, 0.0f, 1.0f);

        double a[kMaxOrder + 1] = { 1.0 };
        int deg = 0;
        for (int f = 0; f < 4; ++f)
        {
            float fv = F[i0][f] + fr * (F[i0 + 1][f] - F[i0][f]);
            fv = closed[f] + openAmt * (fv - closed[f]);
            const float bw = B[f] * (1.0f + 0.8f * (1.0f - openAmt));
            const double r = std::exp (-kPi * bw / sr), th = 2.0 * kPi * fv / sr;
            const double c1 = -2.0 * r * std::cos (th), c2 = r * r;
            // multiply polynomial by (1 + c1 z^-1 + c2 z^-2)
            for (int n = deg + 2; n >= 0; --n)
            {
                double acc = (n <= deg ? a[n] : 0.0);
                if (n >= 1 && n - 1 <= deg) acc += c1 * a[n - 1];
                if (n >= 2 && n - 2 <= deg) acc += c2 * a[n - 2];
                a[n] = acc;
            }
            deg += 2;
        }
        // step-down recursion: polynomial -> reflection coefficients
        float k[kMaxOrder + 1] = { 0 };
        double prod = 1.0;
        for (int i = deg; i >= 1; --i)
        {
            const double ki = std::clamp (a[i], -0.9995, 0.9995);
            k[i] = (float) ki;
            prod *= (1.0 - ki * ki);
            const double d = 1.0 - ki * ki;
            double t[kMaxOrder + 1];
            for (int j = 1; j < i; ++j) t[j] = (a[j] - ki * a[i - j]) / d;
            for (int j = 1; j < i; ++j) a[j] = t[j];
        }
        for (int i = 1; i <= order; ++i) kTgt[(size_t) i] = i <= deg ? k[i] : 0.0f;
        // closing the lips also drops the level
        (void) prod;
        gTgt = 0.35f + 0.65f * openAmt;
    }

    double sr = 48000.0;
    MouthParams p;
    int order = 20, win = 1056, hop = 130;
    std::vector<float> ring, frame, window;
    std::vector<double> lagWin;
    int ringPos = 0, hopCount = 0;
    float prevVoice = 0, preEmph = 0.92f;
    std::array<float, kMaxOrder + 1> kCur {}, kTgt {}, kStep {}, kSmooth {}, bd {};
    float gCur = 1, gTgt = 1, gSmooth = 1, gStep = 0, gNorm = 1;
    float rms = 0, threshold = 0.003f, thrDb = -46.0f, gateSmooth = 0;
    float atkC = 0.01f, relC = 0.0003f, gUp = 0.01f, gDn = 0.001f;
    float smoothA = 0.3f, open = 1.0f, openCoef = 0.99f, vowelOffset = 0.0f;
    bool haveShape = false;
    int framesAbove = 0, onsetHops = 6;
};
} // namespace btb

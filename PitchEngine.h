#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

//==============================================================================
// YIN pitch detector (on a decimated copy of the signal -> cheap enough for realtime)
//==============================================================================
class YinDetector
{
public:
    void prepare (double sampleRate)
    {
        factor  = std::max (1, (int) std::lround (sampleRate / 24000.0));
        fsD     = (float) (sampleRate / factor);
        tauMin  = std::max (2, (int) std::floor (fsD / 1000.0f));   // up to ~1 kHz
        tauMax  = (int) std::ceil (fsD / 65.0f);                    // down to ~65 Hz
        bufSize = W + tauMax + 8;

        ring.assign ((size_t) bufSize, 0.f);
        scratch.assign ((size_t) bufSize, 0.f);
        d.assign ((size_t) tauMax + 2, 0.f);
        wr = 0; acc = 0.f; accCount = 0; sinceDetect = 0;
        hz = 0.f; periodSamples = 0.f; voiced = false;
    }

    // feed one sample; returns true whenever a fresh estimate is available
    bool push (float x)
    {
        acc += x;
        if (++accCount < factor) return false;

        ring[(size_t) wr] = acc / (float) factor;
        wr = (wr + 1) % bufSize;
        acc = 0.f; accCount = 0;

        if (++sinceDetect < hop) return false;
        sinceDetect = 0;
        analyse();
        return true;
    }

    float hz = 0.f;             // detected fundamental
    float periodSamples = 0.f;  // period in *host* samples
    bool  voiced = false;

private:
    void analyse()
    {
        for (int k = 0; k < bufSize; ++k)
            scratch[(size_t) k] = ring[(size_t) ((wr + k) % bufSize)];
        const float* x = scratch.data();

        // energy gate (look at the most recent window)
        float e = 0.f;
        for (int j = bufSize - W; j < bufSize; ++j) e += x[j] * x[j];
        const float rms = std::sqrt (e / (float) W);
        if (rms < 0.004f) { voiced = false; return; }

        // difference function
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float s = 0.f;
            for (int j = 0; j < W; ++j) { const float df = x[j] - x[j + tau]; s += df * df; }
            d[(size_t) tau] = s;
        }

        // cumulative mean normalised difference
        d[0] = 1.f;
        float running = 0.f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            running += d[(size_t) tau];
            d[(size_t) tau] = running > 1e-9f ? d[(size_t) tau] * (float) tau / running : 1.f;
        }

        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (d[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < tauMax && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        }
        if (best < 0)
        {
            int am = tauMin;
            for (int tau = tauMin; tau < tauMax; ++tau)
                if (d[(size_t) tau] < d[(size_t) am]) am = tau;
            if (d[(size_t) am] < 0.30f) best = am;
        }
        if (best < 0) { voiced = false; return; }

        float tauF = (float) best;
        if (best > 1 && best < tauMax)
        {
            const float s0 = d[(size_t) best - 1], s1 = d[(size_t) best], s2 = d[(size_t) best + 1];
            const float denom = 2.f * (2.f * s1 - s2 - s0);
            if (std::abs (denom) > 1e-9f) tauF += (s2 - s0) / denom;
        }

        hz = fsD / tauF;
        periodSamples = tauF * (float) factor;
        voiced = true;
    }

    static constexpr int W = 512;
    static constexpr int hop = 192; // decimated samples between estimates
    int factor = 2, tauMin = 20, tauMax = 360, bufSize = 1024;
    float fsD = 24000.f;
    std::vector<float> ring, scratch, d;
    int wr = 0, accCount = 0, sinceDetect = 0;
    float acc = 0.f;
};

//==============================================================================
// Pitch-synchronous two-tap granular shifter.
// Grain length is a multiple of 2 x pitch period, so both taps stay phase-coherent
// (no comb filtering) -> clean hard-tune without phasey artefacts.
//==============================================================================
class GranularShifter
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& b : buf) b.assign ((size_t) len, 0.f);
        w = 0; phase = 0.f;
        grain = 0.012f * sr;
        computeTaps();
    }

    void write (float in0, float in1)
    {
        buf[0][(size_t) w] = in0;
        buf[1][(size_t) w] = in1;
    }

    // wet signal and the latency-matched dry signal for the current sample
    void read (int ch, float& wet, float& dry) const
    {
        wet = w1 * tap (ch, d1) + (1.f - w1) * tap (ch, d2);
        dry = tap (ch, grain * 0.5f + 2.f);
    }

    void advance (float ratio, float targetGrain)
    {
        const float lo = 0.004f * sr, hi = 0.040f * sr;
        targetGrain = std::min (hi, std::max (lo, targetGrain));
        grain += 0.0006f * (targetGrain - grain);

        phase += (1.f - ratio) / grain;
        phase -= std::floor (phase);
        w = (w + 1) & mask;
        computeTaps();
    }

    // grain target for a given source period (multiple of 2T, >= ~9 ms)
    float grainForPeriod (float periodSamples) const
    {
        const float T = std::max (8.f, periodSamples);
        const int k = std::max (1, (int) std::ceil (0.009f * sr / (2.f * T)));
        return 2.f * T * (float) k;
    }

    float defaultGrain() const { return 0.012f * sr; }

private:
    void computeTaps()
    {
        const float p1 = phase;
        float p2 = phase + 0.5f; if (p2 >= 1.f) p2 -= 1.f;
        d1 = p1 * grain + 2.f;
        d2 = p2 * grain + 2.f;
        w1 = 0.5f - 0.5f * std::cos (6.2831853f * p1);
    }

    float tap (int ch, float delay) const
    {
        const float rp = (float) w - delay;
        const int i0 = (int) std::floor (rp);
        const float fr = rp - (float) i0;
        const float a = buf[ch][(size_t) (i0 & mask)];
        const float b = buf[ch][(size_t) ((i0 + 1) & mask)];
        return a + fr * (b - a);
    }

    static constexpr int len = 16384, mask = len - 1;
    std::vector<float> buf[2];
    int w = 0;
    float sr = 44100.f, phase = 0.f, grain = 512.f, d1 = 2.f, d2 = 2.f, w1 = 0.f;
};

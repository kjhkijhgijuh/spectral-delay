// SpectralDelayCore.h
// Framework-independent spectral delay engine.
//
//  - STFT: 2048-point FFT, hop 512 (overlap 4), periodic Hann analysis + synthesis window
//  - Every FFT bin has its own delay time, feedback, level and pan, read from four
//    64-point curves on a log-frequency axis (0 = lowest bin, 1 = highest bin).
//  - Curves are "0 = neutral":
//      delay    -1..1   multiplier (1 + v) on the base time, 0..2x
//      feedback -1..1   added to the base feedback
//      level    -48..6  dB on the wet signal (-48 = off)
//      pan      -1..1   balance of the wet signal (-1 = L, +1 = R)
//  - Shift moves the delayed spectrum by 2 bins per step on every echo. Odd steps
//    flip the sign every other frame so the shifted partials stay phase-coherent
//    across overlapping frames (this works for either FFT sign convention).
//  - Latency is exactly kFFTSize samples.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstring>
#include <vector>

namespace sd
{

constexpr double kPi = 3.14159265358979323846;
constexpr int kFFTSize = 2048;
constexpr int kHop = 512;
constexpr int kBins = kFFTSize / 2 + 1;   // 1025 (DC .. Nyquist)
constexpr int kCurvePoints = 64;
constexpr double kMaxDelaySeconds = 5.0;  // 2 x the longest Time setting (2500 ms)

using cfloat = std::complex<float>;

enum CurveId { kCurveDelay = 0, kCurveFeedback, kCurveLevel, kCurvePan, kNumCurves };

struct CurveRange { float lo, hi; };
constexpr CurveRange kCurveRanges[kNumCurves] = { { -1.0f, 1.0f }, { -1.0f, 1.0f },
                                                  { -48.0f, 6.0f }, { -1.0f, 1.0f } };

//==============================================================================
// Small iterative radix-2 FFT (complex, in place). Forward uses exp(-j...),
// inverse is unscaled.
class FFT
{
public:
    explicit FFT (int size) : n (size)
    {
        int bits = 0;
        while ((1 << bits) < n) ++bits;
        rev.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b)
                if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            rev[(size_t) i] = r;
        }
        tw.resize ((size_t) n / 2);
        for (int k = 0; k < n / 2; ++k)
        {
            const double a = -2.0 * kPi * k / n;
            tw[(size_t) k] = cfloat ((float) std::cos (a), (float) std::sin (a));
        }
    }

    void forward (cfloat* a) const { transform (a, false); }
    void inverse (cfloat* a) const { transform (a, true); }

private:
    void transform (cfloat* a, bool inv) const
    {
        for (int i = 0; i < n; ++i)
            if (i < rev[(size_t) i]) std::swap (a[i], a[rev[(size_t) i]]);

        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    cfloat w = tw[(size_t) (j * step)];
                    if (inv) w = std::conj (w);
                    const cfloat u = a[i + j];
                    const cfloat v = a[i + j + half] * w;
                    a[i + j] = u + v;
                    a[i + j + half] = u - v;
                }
        }
    }

    int n;
    std::vector<int> rev;
    std::vector<cfloat> tw;
};

//==============================================================================
struct Params
{
    float timeMs = 375.0f;    // base delay time
    float feedback = 0.5f;    // 0..1
    float dryWet = 0.4f;      // 0..1
    float shift = 0.0f;       // steps (2 bins each), rounded
    bool freeze = false;
};

//==============================================================================
class SpectralDelayCore
{
public:
    SpectralDelayCore() : fft (kFFTSize)
    {
        for (int i = 0; i < kFFTSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * (float) std::cos (2.0 * kPi * i / kFFTSize);

        double sumSq = 0.0;
        for (float w : window) sumSq += (double) w * w;
        // inverse FFT is unscaled -> 1/N, and Hann^2 overlap-adds to sumSq/hop
        outScale = (float) (1.0 / ((double) kFFTSize * (sumSq / kHop)));

        for (int b = 0; b < kBins; ++b)
            binPos[(size_t) b] = (float) (std::log ((double) std::max (b, 1)) / std::log ((double) (kBins - 1)));

        for (auto& c : curves) c.fill (0.0f);
    }

    static constexpr int getLatencySamples() { return kFFTSize; }

    void prepare (double sampleRate)
    {
        sr = sampleRate > 0 ? sampleRate : 48000.0;
        int frames = (int) std::ceil (kMaxDelaySeconds * sr / kHop) + 4;
        frames += frames & 1;                 // must be even (see shift sign flip)
        maxFrames = frames;
        ring.assign ((size_t) maxFrames * kBins * 2, cfloat (0.0f, 0.0f));
        reset();
    }

    void reset()
    {
        std::fill (ring.begin(), ring.end(), cfloat (0.0f, 0.0f));
        for (int c = 0; c < 2; ++c)
        {
            inBuf[c].fill (0.0f);
            outBuf[c].fill (0.0f);
        }
        hopCount = 0;
        frameCounter = 0;
    }

    // Copy the curve values (called from the audio thread before process()).
    void setCurve (int id, const float* pts)
    {
        std::copy (pts, pts + kCurvePoints, curves[(size_t) id].begin());
    }

    double getSampleRate() const { return sr; }
    int getMaxFrames() const { return maxFrames; }

    // In-place stereo processing.
    void process (float* left, float* right, int numSamples, const Params& p)
    {
        if (ring.empty()) return;
        float* ch[2] = { left, right };

        for (int i = 0; i < numSamples; ++i)
        {
            for (int c = 0; c < 2; ++c)
            {
                inBuf[c][(size_t) (kFFTSize - kHop + hopCount)] = ch[c][i];
                ch[c][i] = outBuf[c][(size_t) hopCount];
            }

            if (++hopCount == kHop)
            {
                hopCount = 0;
                processFrame (p);
            }
        }
    }

private:
    float curveAt (int id, float pos) const
    {
        const auto& c = curves[(size_t) id];
        const float x = std::clamp (pos, 0.0f, 1.0f) * (kCurvePoints - 1);
        const int a = (int) std::floor (x);
        const int b = std::min (a + 1, kCurvePoints - 1);
        return c[(size_t) a] + (c[(size_t) b] - c[(size_t) a]) * (x - (float) a);
    }

    void processFrame (const Params& p)
    {
        // 1) slide the output accumulator by one hop
        for (int c = 0; c < 2; ++c)
        {
            std::memmove (outBuf[c].data(), outBuf[c].data() + kHop, sizeof (float) * (kFFTSize - kHop));
            std::fill (outBuf[c].begin() + (kFFTSize - kHop), outBuf[c].end(), 0.0f);
        }

        // 2) analysis
        for (int c = 0; c < 2; ++c)
        {
            for (int i = 0; i < kFFTSize; ++i)
                work[c][(size_t) i] = cfloat (inBuf[c][(size_t) i] * window[(size_t) i], 0.0f);
            fft.forward (work[c].data());
        }

        // 3) per-bin processing
        frameCounter = (frameCounter + 1) % maxFrames;
        const int cur = frameCounter;

        const float timeFrames = (float) (p.timeMs * 0.001 * sr / kHop);
        const int stp = (int) std::floor (p.shift + 0.5f);
        const float flip = (std::abs ((long) stp * cur) % 2 == 1) ? -1.0f : 1.0f;
        const float dw = std::clamp (p.dryWet, 0.0f, 1.0f);
        const float dry = 1.0f - dw;

        for (int b = 0; b < kBins; ++b)
        {
            const float pos = binPos[(size_t) b];
            const float vdly = curveAt (kCurveDelay, pos);
            const float vfb = curveAt (kCurveFeedback, pos);
            const float vlvl = curveAt (kCurveLevel, pos);
            const float vpan = curveAt (kCurvePan, pos);

            const float mul = std::clamp (1.0f + vdly, 0.0f, 2.0f);
            const int dfr = std::clamp ((int) std::floor (mul * timeFrames + 0.5f), 1, maxFrames - 1);
            const int rfr = ((cur - dfr) % maxFrames + maxFrames) % maxFrames;

            const int src = b - 2 * stp;
            cfloat y[2] = { cfloat (0.0f, 0.0f), cfloat (0.0f, 0.0f) };
            if (src >= 0 && src < kBins)
            {
                const size_t ri = ((size_t) rfr * kBins + (size_t) src) * 2;
                y[0] = ring[ri] * flip;
                y[1] = ring[ri + 1] * flip;
            }

            const float fbk = std::clamp (p.feedback + vfb, 0.0f, 0.99f);
            const size_t wi = ((size_t) cur * kBins + (size_t) b) * 2;
            for (int c = 0; c < 2; ++c)
                ring[wi + (size_t) c] = p.freeze ? y[c] : work[c][(size_t) b] + y[c] * fbk;

            const float lg = vlvl > -47.5f ? std::pow (10.0f, vlvl / 20.0f) : 0.0f;
            const float gl = lg * std::clamp (1.0f - vpan, 0.0f, 1.0f) * dw;
            const float gr = lg * std::clamp (1.0f + vpan, 0.0f, 1.0f) * dw;

            spec[0][(size_t) b] = work[0][(size_t) b] * dry + y[0] * gl;
            spec[1][(size_t) b] = work[1][(size_t) b] * dry + y[1] * gr;
        }

        // 4) synthesis (Hermitian spectrum -> real), window, overlap-add
        for (int c = 0; c < 2; ++c)
        {
            auto& z = work[c];
            for (int b = 0; b < kBins; ++b)
                z[(size_t) b] = spec[c][(size_t) b];
            for (int b = 1; b < kBins - 1; ++b)
                z[(size_t) (kFFTSize - b)] = std::conj (spec[c][(size_t) b]);
            fft.inverse (z.data());
            for (int i = 0; i < kFFTSize; ++i)
                outBuf[c][(size_t) i] += z[(size_t) i].real() * window[(size_t) i] * outScale;
        }

        // 5) slide the input buffer by one hop
        for (int c = 0; c < 2; ++c)
            std::memmove (inBuf[c].data(), inBuf[c].data() + kHop, sizeof (float) * (kFFTSize - kHop));
    }

    FFT fft;
    double sr = 48000.0;
    int maxFrames = 0;
    int hopCount = 0;
    int frameCounter = 0;
    float outScale = 1.0f;

    std::array<float, kFFTSize> window {};
    std::array<float, kBins> binPos {};
    std::array<std::array<float, kCurvePoints>, kNumCurves> curves {};
    std::array<float, kFFTSize> inBuf[2] {};
    std::array<float, kFFTSize> outBuf[2] {};
    std::array<cfloat, kFFTSize> work[2] {};
    std::array<cfloat, kBins> spec[2] {};
    std::vector<cfloat> ring;   // [frame][bin][channel]
};

} // namespace sd

// Self-contained checks for SpectralDelayCore (no framework needed).
#include "../Source/SpectralDelayCore.h"
#include <cstdio>
#include <memory>
#include <random>
#include <vector>

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { std::printf ("FAIL: " __VA_ARGS__); std::printf ("\n"); ++failures; } \
                              else { std::printf ("ok:   " __VA_ARGS__); std::printf ("\n"); } } while (0)

static void run (sd::SpectralDelayCore& core, std::vector<float>& L, std::vector<float>& R, const sd::Params& p)
{
    const int n = (int) L.size();
    for (int pos = 0; pos < n; pos += 300)
        core.process (L.data() + pos, R.data() + pos, std::min (300, n - pos), p);
}

int main()
{
    const double sr = 48000.0;
    const int N = sd::kFFTSize;
    std::mt19937 rng (1);
    std::normal_distribution<float> gauss (0.0f, 0.2f);

    // 1) dry only = exact copy delayed by the reported latency
    {
        auto core = std::make_unique<sd::SpectralDelayCore>();
        core->prepare (sr);
        std::vector<float> L (48000), R (48000);
        for (int i = 0; i < 48000; ++i) { L[(size_t) i] = gauss (rng); R[(size_t) i] = gauss (rng); }
        auto L0 = L, R0 = R;
        sd::Params p; p.dryWet = 0.0f;
        run (*core, L, R, p);
        float err = 0.0f;
        for (int i = N; i < 48000; ++i)
            err = std::max ({ err, std::abs (L[(size_t) i] - L0[(size_t) (i - N)]), std::abs (R[(size_t) i] - R0[(size_t) (i - N)]) });
        CHECK (err < 1e-5f, "dry path is transparent with latency %d (max err %.2e)", N, err);
    }

    // 2) impulse -> echoes at the expected time with feedback 0.5
    {
        auto core = std::make_unique<sd::SpectralDelayCore>();
        core->prepare (sr);
        std::vector<float> L (96000, 0.0f), R (96000, 0.0f);
        const int at = 10000;
        L[at] = R[at] = 1.0f;
        sd::Params p; p.dryWet = 1.0f; p.feedback = 0.5f; p.timeMs = 213.3f;   // 20 frames
        run (*core, L, R, p);
        const int step = 20 * sd::kHop;
        auto peakNear = [&] (int c) { float m = 0; int mi = 0; for (int i = c - 200; i < c + 200; ++i) if (std::abs (L[(size_t) i]) > m) { m = std::abs (L[(size_t) i]); mi = i; } return std::make_pair (m, mi); };
        auto e1 = peakNear (at + N + step), e2 = peakNear (at + N + 2 * step);
        CHECK (e1.second == at + N + step && std::abs (e1.first - 1.0f) < 0.01f, "1st echo at +%d samples, level %.3f", e1.second - at - N, e1.first);
        CHECK (e2.second == at + N + 2 * step && std::abs (e2.first - 0.5f) < 0.01f, "2nd echo at +%d samples, level %.3f", e2.second - at - N, e2.first);
    }

    // 3) torture: extreme settings stay finite and bounded
    {
        auto core = std::make_unique<sd::SpectralDelayCore>();
        core->prepare (96000.0);
        float ext[sd::kCurvePoints];
        for (int c = 0; c < sd::kNumCurves; ++c)
        {
            for (int i = 0; i < sd::kCurvePoints; ++i)
                ext[i] = (i % 2) ? sd::kCurveRanges[c].hi : sd::kCurveRanges[c].lo;
            core->setCurve (c, ext);
        }
        std::vector<float> L (96000 * 4), R (96000 * 4);
        for (size_t i = 0; i < L.size(); ++i) { L[i] = gauss (rng) * 4.0f; R[i] = -L[i]; }
        float peak = 0.0f; bool finite = true;
        sd::Params p; p.dryWet = 1.0f; p.feedback = 1.0f; p.timeMs = 10.0f;
        for (int blk = 0; blk < 8; ++blk)
        {
            p.shift = (blk % 2) ? 24.0f : -24.0f;
            p.freeze = blk == 3 || blk == 4;
            p.timeMs = blk * 300.0f + 10.0f;
            std::vector<float> l (L.begin() + blk * 48000, L.begin() + blk * 48000 + 48000), r (R.begin() + blk * 48000, R.begin() + blk * 48000 + 48000);
            run (*core, l, r, p);
            for (size_t i = 0; i < l.size(); ++i)
            {
                finite = finite && std::isfinite (l[i]) && std::isfinite (r[i]);
                peak = std::max ({ peak, std::abs (l[i]), std::abs (r[i]) });
            }
        }
        CHECK (finite && peak < 1000.0f, "extreme settings stay finite (peak %.1f)", peak);
    }

    std::printf (failures ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", failures);
    return failures ? 1 : 0;
}

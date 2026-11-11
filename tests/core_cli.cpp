// Offline runner for the core: reads raw float32 stereo (planar L then R) and
// a curves file (4 x 64 floats), writes processed raw float32 planar output.
// usage: core_cli in.raw out.raw curves.raw sr timeMs feedback dryWet shift freezeFrom freezeTo
#include "../Source/SpectralDelayCore.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

int main (int argc, char** argv)
{
    if (argc < 11) { std::fprintf (stderr, "bad args\n"); return 1; }
    auto readAll = [] (const char* path) {
        std::vector<float> v; FILE* f = std::fopen (path, "rb"); float x;
        while (std::fread (&x, sizeof (float), 1, f) == 1) v.push_back (x);
        std::fclose (f); return v; };
    auto in = readAll (argv[1]);
    auto curves = readAll (argv[3]);
    const double sr = std::atof (argv[4]);
    sd::Params p;
    p.timeMs = (float) std::atof (argv[5]);
    p.feedback = (float) std::atof (argv[6]);
    p.dryWet = (float) std::atof (argv[7]);
    p.shift = (float) std::atof (argv[8]);
    const int frzFrom = std::atoi (argv[9]), frzTo = std::atoi (argv[10]);
    const int n = (int) in.size() / 2;
    std::vector<float> L (in.begin(), in.begin() + n), R (in.begin() + n, in.end());

    auto core = std::make_unique<sd::SpectralDelayCore>();
    core->prepare (sr);
    for (int c = 0; c < sd::kNumCurves; ++c) core->setCurve (c, curves.data() + c * sd::kCurvePoints);

    // process in odd-sized blocks to exercise the buffering
    int pos = 0, blk = 0;
    const int sizes[] = { 64, 313, 1, 997, 128 };
    while (pos < n)
    {
        int len = std::min (sizes[blk++ % 5], n - pos);
        const int frame = (pos + sd::kHop) / sd::kHop;  // approx. frame index for freeze window
        p.freeze = frame > frzFrom && frame < frzTo;
        core->process (L.data() + pos, R.data() + pos, len, p);
        pos += len;
    }
    FILE* f = std::fopen (argv[2], "wb");
    std::fwrite (L.data(), sizeof (float), (size_t) n, f);
    std::fwrite (R.data(), sizeof (float), (size_t) n, f);
    std::fclose (f);
    return 0;
}

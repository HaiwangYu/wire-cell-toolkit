#include <iostream>
#include "WireCellUtil/Waveform.h"

using namespace std;

using namespace WireCell::Waveform;

int main()
{
    int nsamples = 10;
    while (nsamples >= 0) {
        realseq_t wave(nsamples, 0);
        median(wave);
        --nsamples;
    }

    realseq_t wave;
    assert(-9999 == median(wave));
    wave.push_back(6.9);
    wave.push_back(9.6);
    assert(-9999 == percentile(wave, -0.1));
    assert(-9999 == percentile(wave, 1.1));
    cerr << median(wave) << endl;
    assert(std::abs(9.6 - median(wave)) < 0.001);
    wave.push_back(0.0);
    cerr << median(wave) << endl;
    assert(std::abs(6.9 - median(wave)) < 0.001);
    wave.push_back(10.0);
    cerr << median(wave) << endl;
    assert(std::abs(9.6 - median(wave)) < 0.001);

    return 0;
}

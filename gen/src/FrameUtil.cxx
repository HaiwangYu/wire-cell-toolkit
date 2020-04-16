#include "WireCellGen/FrameUtil.h"
#include "WireCellIface/SimpleFrame.h"
#include "WireCellIface/SimpleTrace.h"

#include <algorithm>
#include <iostream>

using namespace std;
using namespace WireCell;

typedef std::pair<int, const std::vector<float> *> tbin_charge_t;
typedef std::map<int, std::vector<tbin_charge_t>> channel_index_t;

IFrame::pointer Gen::sum(std::vector<IFrame::pointer> frames, int ident)
{
    // Extract starting times and ticks of all frames
    const int nframes = frames.size();
    std::vector<double> times(nframes), ticks(nframes);
    for (int ind = 0; ind < nframes; ++ind)
    {
        times[ind] = frames[ind]->time();
        ticks[ind] = frames[ind]->tick();
    }
    auto tick_mm = std::minmax_element(ticks.begin(), ticks.end());
    if (*tick_mm.first != *tick_mm.second)
    {
        return nullptr;
    }
    const double tick = ticks[0];
    auto times_mm = std::minmax_element(times.begin(), times.end());
    const double start_time = *times_mm.first;

    // Make an temporary index mapping channel to all the trace data,
    // applying time offsets as we fill.
    channel_index_t index;
    for (int ind = 0; ind < nframes; ++ind)
    {
        IFrame::pointer frame = frames[ind];
        const double dt = frame->time() - start_time;
        const int tbinoff = dt / tick;
        for (auto trace : *frame->traces())
        {
            const int ch = trace->channel();
            const int new_tbin = tbinoff + trace->tbin();
            index[ch].push_back(make_pair(new_tbin, &(trace->charge())));
        }
    }

    // Process each channel to make flattened trace.
    ITrace::vector out_traces;
    for (auto ctc : index)
    {
        auto ch = ctc.first;
        auto tcv = ctc.second;  // vector of tbin_charge_t
        vector<int> tbins;
        for (auto tc : tcv)
        {  // go through once to find tbin bounds
            const int tbin = tc.first;
            tbins.push_back(tbin);
            tbins.push_back(tbin + tc.second->size());
        }
        auto tbin_mm = std::minmax_element(tbins.begin(), tbins.end());
        const int trace_tbin = *tbin_mm.first;
        const int trace_nbins = *tbin_mm.second - trace_tbin;
        vector<float> charge(trace_nbins, 0.0);
        for (auto tc : tcv)
        {
            int ind = tc.first - trace_tbin;
            for (auto q : *tc.second)
            {
                charge[ind] += q;
                ++ind;
            }
        }
        ITrace::pointer trace = make_shared<SimpleTrace>(ch, trace_tbin, charge);
        out_traces.push_back(trace);
    }

    return make_shared<SimpleFrame>(ident, start_time, out_traces, tick);
}

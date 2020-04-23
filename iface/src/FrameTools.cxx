#include "WireCellIface/FrameTools.h"
#include "WireCellIface/SimpleFrame.h"
#include "WireCellIface/SimpleTrace.h"

#include <algorithm>
#include <iterator>
#include <numeric>
#include <unordered_map>
#include <unordered_set>

using namespace WireCell;

int FrameTools::frmtcmp(IFrame::pointer frame, double time)
{
    auto traces = frame->traces();
    auto tbin_mm = FrameTools::tbin_range(*traces.get());

    const double tref = frame->time();
    const double tick = frame->tick();
    const double tmin = tref + tbin_mm.first * tick;   // low edge
    const double tmax = tref + tbin_mm.second * tick;  // high edge

    if (tmax <= time) {  // high edge may be exactly equal to target time and frame
        // does not span.
        return -1;
    }
    if (tmin >= time) {  // low edge may be exactly equal to target time and frame
        // does not span.
        return +1;
    }
    return 0;
}
std::pair<IFrame::pointer, IFrame::pointer> FrameTools::split(IFrame::pointer frame, double time)
{
    int cmp = frmtcmp(frame, time);
    if (cmp < 0) {
        return std::pair<IFrame::pointer, IFrame::pointer>(frame, nullptr);
    }
    if (cmp > 0) {
        return std::pair<IFrame::pointer, IFrame::pointer>(nullptr, frame);
    }
    // now gotta do the hard work.

    const double tref = frame->time();
    const double tick = frame->tick();
    const int ident = frame->ident();

    // Every tick equal or larger than this is in the second frame.
    const double fnticks = (time - tref) / tick;
    const int tbin_split = 0.5 + fnticks;

    ITrace::vector mtraces, ptraces;
    for (auto trace : (*frame->traces())) {
        const int tbin = trace->tbin();

        if (tbin >= tbin_split) {
            ptraces.push_back(trace);
            continue;
        }

        const ITrace::ChargeSequence &wave = trace->charge();
        const int lbin = tbin + wave.size();

        if (lbin <= tbin_split) {
            mtraces.push_back(trace);
            continue;
        }
        const int ind_split = tbin_split - tbin;
        ITrace::ChargeSequence mcharge(wave.begin(), wave.begin() + ind_split);
        ITrace::ChargeSequence pcharge(wave.begin() + ind_split, wave.end());

        const int chid = trace->channel();

        auto mtrace = std::make_shared<SimpleTrace>(chid, tbin, mcharge);
        mtraces.push_back(mtrace);
        auto ptrace = std::make_shared<SimpleTrace>(chid, tbin_split, pcharge);
        ptraces.push_back(ptrace);
    }
    // fixme: what about ident, cmm, tags....
    IFrame::pointer mframe = std::make_shared<SimpleFrame>(ident, tref, mtraces, tick);
    IFrame::pointer pframe = std::make_shared<SimpleFrame>(ident, tref, ptraces, tick);
    return std::pair<IFrame::pointer, IFrame::pointer>(mframe, pframe);
}

ITrace::vector FrameTools::untagged_traces(IFrame::pointer frame)
{
    auto traces = frame->traces();
    size_t ntraces = traces->size();

    std::unordered_set<size_t> tagged;
    for (auto tag : frame->trace_tags()) {
        const auto &taglist = frame->tagged_traces(tag);
        tagged.insert(taglist.begin(), taglist.end());
    }
    std::vector<size_t> all(ntraces), untagged;
    std::iota(all.begin(), all.end(), 0);
    std::set_difference(all.begin(), all.end(), tagged.begin(), tagged.end(),
                        std::inserter(untagged, untagged.begin()));
    ITrace::vector ret;
    for (size_t ind : untagged) {
        ret.push_back(traces->at(ind));
    }
    return ret;
}

ITrace::vector FrameTools::tagged_traces(IFrame::pointer frame, IFrame::tag_t tag)
{
    if (tag == "") {
        return untagged_traces(frame);
    }
    ITrace::vector ret;
    auto const &all_traces = frame->traces();
    for (size_t index : frame->tagged_traces(tag)) {
        ret.push_back(all_traces->at(index));
    }
    if (!ret.empty()) {
        return ret;
    }
    auto ftags = frame->frame_tags();
    if (std::find(ftags.begin(), ftags.end(), tag) == ftags.end()) {
        return ret;
    }
    return *all_traces;  // must make copy of shared pointers
}

FrameTools::channel_list FrameTools::channels(const ITrace::vector &traces)
{
    const auto nchans = traces.size();
    FrameTools::channel_list ret(nchans, 0);
    for (size_t ind = 0; ind != nchans; ++ind) {
        ret[ind] = traces[ind]->channel();
    }
    return ret;
}

std::pair<int, int> FrameTools::tbin_range(const ITrace::vector &traces)
{
    const auto siz = traces.size();
    std::vector<int> tbins(siz), tlens(siz);
    for (size_t ind = 0; ind != siz; ++ind) {
        const auto trace = traces[ind];
        const int tbin = trace->tbin();
        tbins[ind] = tbin;
        tlens[ind] = tbin + trace->charge().size();
    }
    return std::pair<int, int>(*std::min_element(tbins.begin(), tbins.end()),
                               *std::max_element(tlens.begin(), tlens.end()));
}

void FrameTools::fill(Array::array_xxf &array, const ITrace::vector &traces, channel_list::iterator chit,
                      channel_list::iterator chend, int tbin)
{
    std::unordered_map<int, int> index;
    // one col is one tick
    // one row is one channel
    // array is indexed in order: (irow, icol)
    const int ncols = array.cols();
    const int nrows = std::min((int) array.rows(), (int) std::distance(chit, chend));
    for (int ind = 0; ind != nrows and chit != chend; ++ind, ++chit) {
        index[*chit] = ind;
    }
    for (const auto trace : traces) {
        // resolve which row a the channel is at
        const int ch = trace->channel();
        auto it = index.find(ch);
        if (it == index.end()) {
            continue;
        }
        const int irow = it->second;

        const auto &charge = trace->charge();
        const int nticks = charge.size();
        const int dtbin = trace->tbin() - tbin;
        int icol0 = 0, itick0 = 0;
        if (dtbin < 0) {
            itick0 = -dtbin;
        }
        if (dtbin > 0) {
            icol0 = dtbin;
        }

        const int ncols_left = ncols - icol0;
        const int nticks_left = nticks - itick0;
        if (ncols_left <= 0 or nticks_left <= 0) {
            continue;
        }
        const int nleft = std::min(ncols_left, nticks_left);
        for (int ind = 0; ind != nleft; ++ind) {
            array(irow, icol0 + ind) += charge.at(itick0 + ind);
        }
    }
}

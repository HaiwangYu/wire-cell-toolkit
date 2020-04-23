#include "FrameUtils.h"
#include "WireCellUtil/Waveform.h"

#include <algorithm>
#include <iostream>
#include <unordered_map>

using namespace WireCell;

void wct::sigproc::dump_frame(WireCell::IFrame::pointer frame)
{
    auto traces = frame->traces();
    const size_t ntraces = traces->size();
    std::vector<double> means, rmses, lengths, tbins;
    for (auto trace : *traces) {
        auto const &charge = trace->charge();

        auto mr = Waveform::mean_rms(charge);
        means.push_back(mr.first);
        rmses.push_back(mr.second * mr.second);
        const int nsamps = charge.size();
        lengths.push_back(nsamps);
        tbins.push_back(trace->tbin());

        if (std::isnan(mr.second)) {
            std::cerr << "Frame: channel " << trace->channel() << " rms is NaN\n";
        }

        for (int ind = 0; ind < nsamps; ++ind) {
            float val = charge[ind];
            if (std::isnan(val)) {
                std::cerr << "Frame: channel " << trace->channel() << " sample " << ind << " is NaN\n";
            }
            if (std::isinf(val)) {
                std::cerr << "Frame: channel " << trace->channel() << " sample " << ind << " is INF\n";
            }
        }
    }
    double meanmean = Waveform::sum(means) / ntraces;
    double totrms = sqrt(Waveform::sum(rmses));
    double meanlen = Waveform::sum(lengths) / ntraces;
    double meantbin = Waveform::sum(tbins) / ntraces;
    std::cerr << "Frame: " << ntraces << " traces,"
              << " <mean>=" << meanmean << " TotRMS=" << totrms << " <tbin>=" << meantbin << " <len>=" << meanlen
              << std::endl;
    for (auto it : frame->masks()) {
        std::cerr << "\t" << it.first << " : " << it.second.size() << std::endl;
    }
}

void wct::sigproc::raster(WireCell::Array::array_xxf &block, WireCell::ITrace::vector traces,
                          const std::vector<int> &channels)
{
    const size_t nchannels = channels.size();
    std::unordered_map<int, size_t> ch2col;
    for (size_t ind = 0; ind < nchannels; ++ind) {
        ch2col[channels[ind]] = ind;
    }

    const size_t ncols = block.cols();
    for (auto trace : traces) {
        const auto &samples = trace->charge();
        const size_t nsamples = samples.size();
        const size_t tbin = trace->tbin();

        if (tbin >= ncols) {  // underflow impossible as they are unsigned.
            continue;         // trace is off screen
        }

        // find the row to fill
        const int ch = trace->channel();
        const auto chit = ch2col.find(ch);
        if (chit == ch2col.end()) {
            continue;
        }
        const size_t irow = chit->second;
        WireCell::Array::array_xf tofill(ncols - tbin);
        const size_t maxbin = std::min(nsamples, ncols - tbin);
        for (size_t bin = 0; bin < maxbin; ++bin) {
            tofill[bin] = samples[bin];
        }
        block.block(irow, tbin, 1, ncols - tbin).row(0) += tofill;
    }
}

int wct::sigproc::maxcount_baseline(const ITrace::vector &traces, const WireCell::Binning &binning)
{
    std::vector<int> hist(binning.nbins(), 0);
    for (auto trace : traces) {
        auto const &samples = trace->charge();
        for (auto sample : samples) {
            hist[binning.bin(sample)] += 1;
        }
    }
    auto mme = std::minmax_element(hist.begin(), hist.end());
    return mme.second - hist.begin();
}

ITrace::vector wct::sigproc::tagged_traces(IFrame::pointer frame, IFrame::tag_t tag)
{
    auto const &all_traces = frame->traces();
    ITrace::vector ret;
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
    return *all_traces;  // must make copy
}

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

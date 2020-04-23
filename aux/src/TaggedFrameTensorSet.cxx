#include "WireCellAux/TaggedFrameTensorSet.h"
#include "WireCellAux/SimpleTensor.h"
#include "WireCellAux/SimpleTensorSet.h"
#include "WireCellIface/FrameTools.h"
#include "WireCellUtil/NamedFactory.h"

#include <iostream>

WIRECELL_FACTORY(TaggedFrameTensorSet, WireCell::Aux::TaggedFrameTensorSet, WireCell::IFrameTensorSet,
                 WireCell::IConfigurable)

using namespace WireCell;

WireCell::Configuration Aux::TaggedFrameTensorSet::default_configuration() const
{
    // Each tag will produce a tensor.
    Configuration cfg;
    cfg["tensors"] = Json::arrayValue;
    return cfg;
}

void Aux::TaggedFrameTensorSet::configure(const WireCell::Configuration &config) { m_cfg = config; }

bool Aux::TaggedFrameTensorSet::operator()(const input_pointer &in, output_pointer &out)
{
    out = nullptr;
    if (!in) {  // pass on EOS
        return true;
    }

    ITensor::vector *itv = new ITensor::vector;

    Configuration md;
    md["time"] = in->time();
    md["tick"] = in->tick();

    for (auto jten : m_cfg["tensors"]) {
        auto tag = get<std::string>(jten, "tag", "");
        jten["tag"] = tag;  // assure it is there for output
        auto traces = FrameTools::tagged_traces(in, tag);
        auto mm_tbin = FrameTools::tbin_range(traces);
        int tbin0 = mm_tbin.first;
        jten["tbin"] = tbin0;
        size_t ntraces = traces.size();

        // traces may be degenerate/overlapping in channels and ticks.
        // Do a little dance to map chid to an index sorted by chid.
        std::vector<int> uchid;
        for (const auto trace : traces) {
            uchid.push_back(trace->channel());
        }
        std::sort(uchid.begin(), uchid.end());
        std::vector<int>::iterator it = std::unique(uchid.begin(), uchid.end());
        const size_t nchans = std::distance(uchid.begin(), it);
        uchid.resize(nchans);
        std::map<int, size_t> chid2ind;
        for (auto chid : uchid) {
            size_t ind = chid2ind.size();
            chid2ind[chid] = ind;
            jten["channels"].append(chid);
        }

        const auto &ts = in->trace_summary(tag);
        for (int ind = 0; ind < (int) ts.size(); ++ind) {
            jten["summary"][ind] = ts[ind];
        }

        ///

        size_t nticks = mm_tbin.second - mm_tbin.first;
        const std::vector<size_t> shape = {nchans, nticks};

        SimpleTensor<float> *st = new SimpleTensor<float>(shape);
        Eigen::Map<Eigen::ArrayXXf> arr((float *) st->data(), nchans, nticks);
        arr.setZero();

        for (size_t itr = 0; itr < ntraces; ++itr) {
            auto trace = traces[itr];
            const auto &qarr = trace->charge();
            size_t nq = qarr.size();

            int chid = trace->channel();
            size_t chind = chid2ind[chid];

            size_t tbini = trace->tbin() - tbin0;  // where in output to lay-down charge.
            size_t tbinf = std::min(nticks, tbini + nq);
            size_t tbinn = tbinf - tbini;

            Eigen::Map<const Eigen::ArrayXf> wave(qarr.data(), nq);
            arr.row(chind).segment(tbini, tbinn) += wave;
        }
        md["tensors"].append(jten);

        itv->push_back(ITensor::pointer(st));
    }

    out = std::make_shared<SimpleTensorSet>(in->ident(), md, ITensor::shared_vector(itv));
    return true;
}

Aux::TaggedFrameTensorSet::TaggedFrameTensorSet() {}
Aux::TaggedFrameTensorSet::~TaggedFrameTensorSet() {}

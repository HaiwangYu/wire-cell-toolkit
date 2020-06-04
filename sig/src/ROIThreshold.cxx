#include "WireCellSig/ROIThreshold.h"
#include "WireCellSig/Util.h"

#include "WireCellUtil/NamedFactory.h"
#include "WireCellUtil/String.h"
#include "WireCellUtil/Array.h"
#include "WireCellUtil/Response.h"
#include "WireCellUtil/FFTBestLength.h"
#include "WireCellUtil/Exceptions.h"

#include "WireCellIface/ITensorSet.h"
#include "WireCellIface/IFilterWaveform.h"

#include "WireCellAux/SimpleTensorSet.h"
#include "WireCellAux/SimpleTensor.h"
#include "WireCellAux/Util.h"
#include "WireCellAux/TensUtil.h"

WIRECELL_FACTORY(ROIThreshold, WireCell::Sig::ROIThreshold, WireCell::ITensorSetFilter, WireCell::IConfigurable)

using namespace WireCell;

Sig::ROIThreshold::ROIThreshold()
  : log(Log::logger("ROIThreshold"))
{
}

Configuration Sig::ROIThreshold::default_configuration() const
{
    Configuration cfg;
    cfg["tag"] = "tag";

    // FIXME: get tensor by type or tag?
    cfg["intypes"] = Json::arrayValue;
    cfg["outtypes"] = Json::arrayValue;

    cfg["intypes"].append("bad:cmm_range");
    cfg["intypes"].append("bad:cmm_channel");
    cfg["intypes"].append("decon_tight");
    cfg["intypes"].append("decon_tighter");

    cfg["outtypes"].append("roi_tight");
    cfg["outtypes"].append("rms");

    return cfg;
}

void Sig::ROIThreshold::configure(const WireCell::Configuration &cfg)
{
    m_cfg = cfg;
    assert(m_cfg["intypes"].size() == 4);
}

bool Sig::ROIThreshold::operator()(const ITensorSet::pointer &in, ITensorSet::pointer &out)
{
    log->trace("start");

    int iplane = get<int>(m_cfg, "iplane", 0);
    log->trace("iplane {}", iplane);

    if (!in) {
        out = nullptr;
        return true;
    }

    auto tag = get<std::string>(m_cfg, "tag", "tag");

    std::vector<ITensor::pointer> in_tensors;
    for (auto j : m_cfg["intypes"]) {
        auto type = j.asString();
        auto t = Aux::get_tens(in, tag, type);
        if (!t) {
            THROW(ValueError() << errmsg{"Failed to get ITensor: " + tag + ", " + type});
        }
        in_tensors.push_back(t);
    }

    assert(in_tensors.size() == 4);
    auto cmm_range = in_tensors[0];
    auto cmm_channel = in_tensors[1];
    auto in_ten0 = in_tensors[2];
    auto in_ten1 = in_tensors[3];

    // TensorSet to Eigen
    WireCell::Array::array_xxf r_data = Aux::itensor_to_eigen_array<float>(in_ten0);
    WireCell::Array::array_xxf r_data_tight = Aux::itensor_to_eigen_array<float>(in_ten1);
    log->trace("r_data: {} {}", r_data.rows(), r_data.cols());

    // apply cmm
    {
        auto nranges = cmm_channel->shape()[0];
        assert(nranges == cmm_range->shape()[0]);
        assert(2 == cmm_range->shape()[1]);
        Eigen::Map<Eigen::ArrayXXd> ranges_arr((double *) cmm_range->data(), nranges, 2);
        Eigen::Map<Eigen::ArrayXd> channels_arr((double *) cmm_channel->data(), nranges);
        for (size_t ind = 0; ind < nranges; ++ind) {
            auto ch = channels_arr(ind);
            if (!(ch < r_data.rows())) {
                continue;
            }
            auto tmin = ranges_arr(ind, 0);
            auto tmax = ranges_arr(ind, 1);
            log->trace("ch: {}, tmin: {}, tmax: {}", ch, tmin, tmax);
            assert(tmin < tmax);
            assert(tmax <= r_data.cols());
            auto nbin = tmax - tmin;
            r_data.row(ch).segment(tmin, nbin) = 0.;
        }
    }

    // save back to ITensorSet
    ITensor::vector *itv = new ITensor::vector;

    auto out_ten0 = Aux::eigen_array_to_simple_tensor<float>(r_data);
    {
        auto &md = out_ten0->metadata();
        md["tag"] = tag;
        md["type"] = m_cfg["outtypes"][0].asString();
    }
    itv->push_back(ITensor::pointer(out_ten0));

    if (m_cfg["outtypes"].size() > 1) {
        auto out_ten1 = Aux::eigen_array_to_simple_tensor<float>(r_data);
        {
            auto &md = out_ten1->metadata();
            md["tag"] = tag;
            md["type"] = m_cfg["outtypes"][1].asString();
        }
        itv->push_back(ITensor::pointer(out_ten1));
    }

    Configuration oset_md(in->metadata());
    oset_md["tags"] = Json::arrayValue;
    oset_md["tags"].append(tag);

    out = std::make_shared<Aux::SimpleTensorSet>(in->ident(), oset_md, ITensor::shared_vector(itv));

    log->trace("end");

    return true;
}
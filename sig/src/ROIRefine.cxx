#include "WireCellSig/ROIRefine.h"
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

WIRECELL_FACTORY(ROIRefine, WireCell::Sig::ROIRefine, WireCell::ITensorSetFilter, WireCell::IConfigurable)

using namespace WireCell;

Sig::ROIRefine::ROIRefine()
  : log(Log::logger("sig"))
{
}

Configuration Sig::ROIRefine::default_configuration() const
{
    Configuration cfg;
    cfg["tag"] = "tag";

    return cfg;
}

void Sig::ROIRefine::configure(const WireCell::Configuration &cfg) { m_cfg = cfg; }

bool Sig::ROIRefine::operator()(const ITensorSet::pointer &in, ITensorSet::pointer &out)
{
    log->debug("start");

    if (!in) {
        out = nullptr;
        return true;
    }
    auto set_md = in->metadata();

    auto tag = get<std::string>(m_cfg, "tag", "tag");

    auto iten0 = Aux::get_tens(in, tag, m_cfg["input0"].asString());
    auto iten1 = Aux::get_tens(in, tag, m_cfg["input1"].asString());
    if (!iten0 or !iten1) {
        log->error("Tensor->Frame: failed to get waveform tensor for tag {} type {} or type {}", tag, m_cfg["input0"].asString(), m_cfg["input1"].asString());
        THROW(ValueError() << errmsg{"Sig::ROIRefine::operator() !iten0 or !iten1"});
    }
    auto wf_shape = iten0->shape();
    size_t nchans = wf_shape[0];
    size_t nticks = wf_shape[1];
    log->debug("iwf_ten->shape: {} {}", nchans, nticks);

    bool have_cmm = true;
    auto cmm_range = Aux::get_tens(in, tag, "bad:cmm_range");
    auto cmm_channel = Aux::get_tens(in, tag, "bad:cmm_channel");
    if (!cmm_range or !cmm_channel) {
        log->debug("Tensor->Frame: no optional cmm_range tensor for tag {}", tag);
        have_cmm = false;
    }

    int iplane = get<int>(m_cfg, "iplane", 0);
    log->debug("iplane {}", iplane);

    int m_nwires = nchans;
    int m_nticks = nticks;

    // TensorSet to Eigen
    WireCell::Array::array_xxf r_data = Aux::itensor_to_eigen_array<float>(iten0);
    WireCell::Array::array_xxf r_data_tight = Aux::itensor_to_eigen_array<float>(iten1);
    log->debug("r_data: {} {}", r_data.rows(), r_data.cols());

    // apply cmm
    if (have_cmm) {
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

    // Eigen to TensorSet
    auto owf_ten = Aux::eigen_array_to_simple_tensor<float>(r_data);

    // save back to ITensorSet
    ITensor::vector *itv = new ITensor::vector;
    auto iwf_md = iten0->metadata();
    auto &owf_md = owf_ten->metadata();
    owf_md["tag"] = tag;
    owf_md["type"] = "roi_th";
    itv->push_back(ITensor::pointer(owf_ten));

    // FIXME need to change ch_ten tag to outtag

    Configuration oset_md(in->metadata());
    oset_md["tags"] = Json::arrayValue;
    oset_md["tags"].append(tag);

    out = std::make_shared<Aux::SimpleTensorSet>(in->ident(), oset_md, ITensor::shared_vector(itv));

    log->debug("end");

    return true;
}
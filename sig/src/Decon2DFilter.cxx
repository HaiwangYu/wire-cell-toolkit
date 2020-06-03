#include "WireCellSig/Decon2DFilter.h"
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

WIRECELL_FACTORY(Decon2DFilter, WireCell::Sig::Decon2DFilter, WireCell::ITensorSetFilter, WireCell::IConfigurable)

using namespace WireCell;

Sig::Decon2DFilter::Decon2DFilter()
  : log(Log::logger("sig"))
{
}

Configuration Sig::Decon2DFilter::default_configuration() const
{
    Configuration cfg;
    cfg["out_type"] = "Decon2DFilter";

    return cfg;
}

void Sig::Decon2DFilter::configure(const WireCell::Configuration &cfg) { m_cfg = cfg; }

bool Sig::Decon2DFilter::operator()(const ITensorSet::pointer &in, ITensorSet::pointer &out)
{
    log->debug("Decon2DFilter: start");

    if (!in) {
        out = nullptr;
        return true;
    }
    auto set_md = in->metadata();

    auto tag = get<std::string>(m_cfg, "tag", "trace_tag");

    auto iwf_ten = Aux::get_tens(in, tag, "waveform");
    if (!iwf_ten) {
        log->error("Tensor->Frame: failed to get waveform tensor for tag {}", tag);
        THROW(ValueError() << errmsg{"Sig::Decon2DFilter::operator() !iwf_ten"});
    }
    auto wf_shape = iwf_ten->shape();
    size_t nchans = wf_shape[0];
    size_t nticks = wf_shape[1];
    log->debug("iwf_ten->shape: {} {}", nchans, nticks);

    int iplane = get<int>(m_cfg, "iplane", 0);
    log->debug("iplane {}", iplane);

    // bins
    int m_pad_nwires = 0;

    // FIXME: figure this out
    int m_nwires = nchans;
    int m_nticks = nticks;

    // TensorSet to Eigen
    WireCell::Array::array_xxc c_data = Aux::itensor_to_eigen_array<std::complex<float>>(iwf_ten);
    log->debug("c_data: {} {}", c_data.rows(), c_data.cols());

    // make software filter on time
    WireCell::Waveform::realseq_t filter(c_data.cols(), 1.);
    for (auto icfg : m_cfg["filters"]) {
        const std::string filter_tn = icfg.asString();
        log->trace("filter_tn: {}", filter_tn);
        auto fw = Factory::find_tn<IFilterWaveform>(filter_tn);
        if (!fw) {
            THROW(ValueError() << errmsg{"!fw"});
        }
        auto wave = fw->filter_waveform(c_data.cols());
        for (size_t i = 0; i != wave.size(); i++) {
            filter.at(i) *= wave.at(i);
        }
    }

    // apply filter
    Array::array_xxc c_data_afterfilter(c_data.rows(), c_data.cols());
    for (int irow = 0; irow < c_data.rows(); ++irow) {
        for (int icol = 0; icol < c_data.cols(); ++icol) {
            c_data_afterfilter(irow, icol) = c_data(irow, icol) * filter.at(icol);
        }
    }

    // do the second round of inverse FFT on wire
    Array::array_xxf tm_r_data = Array::idft_cr(c_data_afterfilter, 0);
    Array::array_xxf r_data = tm_r_data.block(m_pad_nwires, 0, m_nwires, m_nticks);
    Sig::restore_baseline(r_data);

    // Eigen to TensorSet
    auto owf_ten = Aux::eigen_array_to_simple_tensor<float>(r_data);

    // save back to ITensorSet
    ITensor::vector *itv = new ITensor::vector;
    auto iwf_md = iwf_ten->metadata();
    auto &owf_md = owf_ten->metadata();
    owf_md["tag"] = tag;
    owf_md["type"] = get<std::string>(m_cfg, "out_type", "Decon2DFilter");
    itv->push_back(ITensor::pointer(owf_ten));

    Configuration oset_md(in->metadata());
    oset_md["tags"] = Json::arrayValue;
    oset_md["tags"].append(tag);

    out = std::make_shared<Aux::SimpleTensorSet>(in->ident(), oset_md, ITensor::shared_vector(itv));

    log->debug("Decon2DFilter: end");

    return true;
}
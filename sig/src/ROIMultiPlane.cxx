#include "WireCellSig/ROIMultiPlane.h"
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

WIRECELL_FACTORY(ROIMultiPlane, WireCell::Sig::ROIMultiPlane, WireCell::ITensorSetFilter, WireCell::IConfigurable)

using namespace WireCell;

Sig::ROIMultiPlane::ROIMultiPlane()
  : log(Log::logger("sig"))
{
}

Configuration Sig::ROIMultiPlane::default_configuration() const
{
    Configuration cfg;
    cfg["tag"] = "tag";

    // FIXME: get tensor by type or tag?
    cfg["intypes"] = Json::arrayValue;
    cfg["outtypes"] = Json::arrayValue;

    return cfg;
}

void Sig::ROIMultiPlane::configure(const WireCell::Configuration &cfg)
{
    m_cfg = cfg;
}

bool Sig::ROIMultiPlane::operator()(const ITensorSet::pointer &in, ITensorSet::pointer &out)
{
    log->trace("ROIMultiPlane: start");

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

    // FIXME: Place holder
    WireCell::Array::array_xxf r_data = Aux::itensor_to_eigen_array<float>(in_tensors[0]);
    
    // save back to ITensorSet
    ITensor::vector *itv = new ITensor::vector;

    auto out_ten0 = Aux::eigen_array_to_simple_tensor<float>(r_data);
    {
        auto &md = out_ten0->metadata();
        md["tag"] = tag;
        md["type"] = m_cfg["outtypes"][0].asString();
    }
    itv->push_back(ITensor::pointer(out_ten0));

    auto out_ten1 = Aux::eigen_array_to_simple_tensor<float>(r_data);
    {
        auto &md = out_ten1->metadata();
        md["tag"] = tag;
        md["type"] = m_cfg["outtypes"][1].asString();
    }
    itv->push_back(ITensor::pointer(out_ten1));

    Configuration oset_md(in->metadata());
    oset_md["tags"] = Json::arrayValue;
    oset_md["tags"].append(tag);

    out = std::make_shared<Aux::SimpleTensorSet>(in->ident(), oset_md, ITensor::shared_vector(itv));

    log->trace("ROIMultiPlane: end");

    return true;
}
#include "WireCellSig/Decon2DResponse.h"
#include "WireCellUtil/Array.h"
#include "WireCellUtil/NamedFactory.h"
#include "WireCellUtil/String.h"

#include "WireCellAux/SimpleTensor.h"
#include "WireCellAux/SimpleTensorSet.h"
#include "WireCellAux/Util.h"
#include "WireCellIface/ITensorSet.h"
#include "WireCellUtil/Exceptions.h"

WIRECELL_FACTORY(Decon2DResponse, WireCell::Sig::Decon2DResponse,
                 WireCell::ITensorSetFilter, WireCell::IConfigurable)

using namespace WireCell;

Sig::Decon2DResponse::Decon2DResponse()
  : l(Log::logger("pytorch"))
{
}

Configuration Sig::Decon2DResponse::default_configuration() const
{
    Configuration cfg;

    return cfg;
}

void Sig::Decon2DResponse::configure(const WireCell::Configuration &cfg)
{
    m_cfg = cfg;
}

namespace
{
    std::string dump(const ITensorSet::pointer &itens)
    {
        std::stringstream ss;
        ss << "ITensorSet: ";
        Json::FastWriter jwriter;
        ss << itens->ident() << ", " << jwriter.write(itens->metadata());
        for (auto iten : *itens->tensors())
        {
            ss << "shape: [";
            for (auto l : iten->shape())
            {
                ss << l << " ";
            }
            ss << "]\n";
        }
        return ss.str();
    }
}  // namespace

bool Sig::Decon2DResponse::operator()(const ITensorSet::pointer &in,
                                      ITensorSet::pointer &out)
{
    l->debug("Decon2DResponse: start");

    if (!in)
    {
        out = nullptr;
        return true;
    }

    if (in->tensors()->size() != 1)
    {
        THROW(ValueError() << errmsg{"in->tensors()->size()!=1"});
    }

    // TensorSet to Eigen
    auto iiten = in->tensors()->front();
    WireCell::Array::array_xxf r_data = Aux::itensor_to_eigen_array<float>(iiten);

    // first round of FFT on time
    auto c_data = Array::dft_rc(r_data, 0);

    // ch-by-ch response

    // second round of FFT on wire
    c_data = Array::dft_cc(c_data, 1);

    // other processing

    // Eigen to TensorSet
    auto oiten = Aux::eigen_array_to_itensor<std::complex<float>>(c_data);
    ITensor::vector *itv = new ITensor::vector;
    itv->push_back(ITensor::pointer(oiten));
    // FIXME use a correct seqno
    int seqno = 0;
    Configuration md;
    out = std::make_shared<Aux::SimpleTensorSet>(seqno, md,
                                                 ITensor::shared_vector(itv));

    l->debug("Decon2DResponse: end");

    return true;
}
#include "WireCellAux/TensorFanout.h"

#include "WireCellUtil/NamedFactory.h"
#include "WireCellUtil/Exceptions.h"

WIRECELL_FACTORY(TensorFanout, WireCell::Aux::TensorFanout, WireCell::ITensorFanout, WireCell::IConfigurable)

using namespace WireCell;

Aux::TensorFanout::TensorFanout(size_t multiplicity)
  : m_multiplicity(multiplicity)
  , log(Log::logger("glue"))
{
}
Aux::TensorFanout::~TensorFanout() {}

WireCell::Configuration Aux::TensorFanout::default_configuration() const
{
    Configuration cfg;
    // How many output ports
    cfg["multiplicity"] = (int) m_multiplicity;
    return cfg;
}
void Aux::TensorFanout::configure(const WireCell::Configuration& cfg)
{
    int m = get<int>(cfg, "multiplicity", (int) m_multiplicity);
    if (m <= 0) {
        THROW(ValueError() << errmsg{"TensorFanout multiplicity must be positive"});
    }
    m_multiplicity = m;
}

std::vector<std::string> Aux::TensorFanout::output_types()
{
    const std::string tname = std::string(typeid(output_type).name());
    std::vector<std::string> ret(m_multiplicity, tname);
    return ret;
}

bool Aux::TensorFanout::operator()(const input_pointer& in, output_vector& outv)
{
    outv.resize(m_multiplicity);

    if (!in) {  //  pass on EOS
        for (size_t ind = 0; ind < m_multiplicity; ++ind) {
            outv[ind] = in;
        }
        log->debug("TensorFanout: see EOS");
        return true;
    }

    for (size_t ind = 0; ind < m_multiplicity; ++ind) {
        outv[ind] = in;
    }

    return true;
}

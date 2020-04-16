#include "WireCellGen/DepoSetFanout.h"

#include "WireCellUtil/Exceptions.h"
#include "WireCellUtil/NamedFactory.h"

#include <iostream>

WIRECELL_FACTORY(DepoSetFanout, WireCell::Gen::DepoSetFanout,
                 WireCell::IDepoSetFanout, WireCell::IConfigurable)

using namespace WireCell;
using namespace std;

Gen::DepoSetFanout::DepoSetFanout(size_t multiplicity)
  : m_multiplicity(multiplicity)
  , log(Log::logger("glue"))
{
}

Gen::DepoSetFanout::~DepoSetFanout() {}

WireCell::Configuration Gen::DepoSetFanout::default_configuration() const
{
    Configuration cfg;
    cfg["multiplicity"] = (int) m_multiplicity;
    return cfg;
}

void Gen::DepoSetFanout::configure(const WireCell::Configuration &cfg)
{
    int m = get<int>(cfg, "multiplicity", (int) m_multiplicity);
    if (m <= 0)
    {
        log->critical("DepoSetFanout multiplicity must be positive");
        THROW(
            ValueError() << errmsg{"DepoSetFanout multiplicity must be positive"});
    }
    m_multiplicity = m;
}

std::vector<std::string> Gen::DepoSetFanout::output_types()
{
    const std::string tname = std::string(typeid(output_type).name());
    std::vector<std::string> ret(m_multiplicity, tname);
    return ret;
}

bool Gen::DepoSetFanout::operator()(const input_pointer &in,
                                    output_vector &outv)
{
    // Note: if "in" indicates EOS, just pass it on
    if (in)
    {
        log->debug("DepoSetFanout ({}) fanout data", in->ident());
    }
    else
    {
        log->debug("DepoSetFanout fanout EOS");
    }

    outv.resize(m_multiplicity);
    for (size_t ind = 0; ind < m_multiplicity; ++ind)
    {
        outv[ind] = in;
    }
    return true;
}

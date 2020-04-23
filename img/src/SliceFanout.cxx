#include "WireCellImg/SliceFanout.h"

#include "WireCellUtil/Exceptions.h"
#include "WireCellUtil/NamedFactory.h"

WIRECELL_FACTORY(SliceFanout, WireCell::Img::SliceFanout, WireCell::ISliceFanout, WireCell::IConfigurable)

using namespace WireCell;

Img::SliceFanout::SliceFanout(size_t multiplicity)
  : m_multiplicity(multiplicity)
  , l(Log::logger("glue"))
{
}
Img::SliceFanout::~SliceFanout() {}

WireCell::Configuration Img::SliceFanout::default_configuration() const
{
    Configuration cfg;
    // How many output ports
    cfg["multiplicity"] = (int) m_multiplicity;
    return cfg;
}
void Img::SliceFanout::configure(const WireCell::Configuration &cfg)
{
    int m = get<int>(cfg, "multiplicity", (int) m_multiplicity);
    if (m <= 0) {
        l->critical("SliceFanout multiplicity must be positive");
        THROW(ValueError() << errmsg{"SliceFanout multiplicity must be positive"});
    }
    m_multiplicity = m;
}

std::vector<std::string> Img::SliceFanout::output_types()
{
    const std::string tname = std::string(typeid(output_type).name());
    std::vector<std::string> ret(m_multiplicity, tname);
    return ret;
}

bool Img::SliceFanout::operator()(const input_pointer &in, output_vector &outv)
{
    outv.resize(m_multiplicity);

    if (!in) {
        SPDLOG_LOGGER_TRACE(l, "SliceFanout: sending out {} EOSes", m_multiplicity);
        for (size_t ind = 0; ind < m_multiplicity; ++ind) {
            outv[ind] = nullptr;
        }
        return true;
    }

    SPDLOG_LOGGER_TRACE(l, "SliceFanout: {}x of #{} t={} + {} in nchan={}", m_multiplicity, in->ident(), in->start(),
                        in->span(), in->activity().size());

    for (size_t ind = 0; ind < m_multiplicity; ++ind) {
        outv[ind] = in;
    }
    return true;
}

#include "WireCellImg/BlobSetSync.h"
#include "WireCellIface/SimpleBlob.h"

#include "WireCellUtil/NamedFactory.h"

WIRECELL_FACTORY(BlobSetSync, WireCell::Img::BlobSetSync, WireCell::IBlobSetFanin, WireCell::IConfigurable)
using namespace WireCell;

Img::BlobSetSync::BlobSetSync()
  : m_multiplicity(0)
  , l(Log::logger("glue"))
{
}

Img::BlobSetSync::~BlobSetSync() {}

WireCell::Configuration Img::BlobSetSync::default_configuration() const
{
    Configuration cfg;
    cfg["multiplicity"] = (int) m_multiplicity;
    return cfg;
}

void Img::BlobSetSync::configure(const WireCell::Configuration &cfg)
{
    int m = get<int>(cfg, "multiplicity", (int) m_multiplicity);
    if (m <= 0) {
        THROW(ValueError() << errmsg{"BlobSync multiplicity must be positive"});
    }
    m_multiplicity = m;
}

std::vector<std::string> Img::BlobSetSync::input_types()
{
    const std::string tname = std::string(typeid(input_type).name());
    std::vector<std::string> ret(m_multiplicity, tname);
    return ret;
}

bool Img::BlobSetSync::operator()(const input_vector &invec, output_pointer &out)
{
    SimpleBlobSet *sbs = new SimpleBlobSet(0, nullptr);
    out = IBlobSet::pointer(sbs);

    int neos = 0;
    for (const auto &ibs : invec) {
        if (!ibs) {
            ++neos;
            break;
        }
        ISlice::pointer newslice = ibs->slice();
        if (!sbs->slice() or sbs->slice()->start() > newslice->start()) {
            sbs->m_slice = newslice;
            sbs->m_ident = newslice->ident();
        }
        for (const auto &iblob : ibs->blobs()) {
            sbs->m_blobs.push_back(iblob);
        }
    }
    if (neos) {
        out = nullptr;
        SPDLOG_LOGGER_TRACE(l, "BlobSetSink: EOS");
        return true;
    }
    SPDLOG_LOGGER_TRACE(l, "BlobSetSink: sync'ed {} blobs", sbs->m_blobs.size());
    return true;
}

#include "WireCellZio/ZioTensorSetSource.h"
#include "WireCellUtil/NamedFactory.h"
#include "WireCellZio/FlowConfigurable.h"

WIRECELL_FACTORY(ZioTensorSetSource, WireCell::Zio::ZioTensorSetSource,
                 WireCell::ITensorSetSource, WireCell::IConfigurable)

using namespace WireCell;

Zio::ZioTensorSetSource::ZioTensorSetSource()
  : FlowConfigurable("extract")
  , l(Log::logger("zio"))
  , m_had_eos(false)
{
}

Zio::ZioTensorSetSource::~ZioTensorSetSource() {}

bool Zio::ZioTensorSetSource::operator()(ITensorSet::pointer &out)
{
    // if buffer not empty
    if (m_tensors.size() > 0)
    {
        out = m_tensors.back();
        m_tensors.pop_back();
        return true;
    }

    // fill m_tensors
    pre_flow();
    if (!m_flow)
    {
        return false;
    }

    zio::Message msg;
    bool ok = m_flow->get(msg, m_timeout);
    if (!ok)
    {
        zio::Message eot;
        m_flow->send_eot(eot);
        m_flow = nullptr;
        return false;  // timeout, eot or other error
    }

    const zio::multipart_t &pls = msg.payload();
    if (!pls.size())
    {  // EOS
        if (m_had_eos)
        {  // 2nd
            finalize();
            return false;
        }
        m_had_eos = true;
        return true;
    }
    m_had_eos = false;

    m_tensors.push_back(Zio::FlowConfigurable::unpack(msg));

    // issue quit if buffer empty after filling
    if (m_tensors.empty())
    {
        return false;
    }

    // pop one from buffer
    out = m_tensors.back();
    m_tensors.pop_back();
    return true;
}
#include "WireCellGen/FrameFanout.h"

#include "WireCellIface/SimpleFrame.h"
#include "WireCellUtil/Exceptions.h"
#include "WireCellUtil/NamedFactory.h"

WIRECELL_FACTORY(FrameFanout, WireCell::Gen::FrameFanout,
                 WireCell::IFrameFanout, WireCell::IConfigurable)

using namespace WireCell;

Gen::FrameFanout::FrameFanout(size_t multiplicity)
  : m_multiplicity(multiplicity)
  , log(Log::logger("glue"))
{
}
Gen::FrameFanout::~FrameFanout() {}

WireCell::Configuration Gen::FrameFanout::default_configuration() const
{
    Configuration cfg;
    // How many output ports
    cfg["multiplicity"] = (int) m_multiplicity;
    // Tag rules are an array, one element per output port.  Each
    // element is an object keyed with "frame" or "trace".  Each of
    // their values are an object keyed by a regular expression
    // (regex) and with values that are a single tag or an array of
    // tags.  See util/test_tagrules for examples.
    cfg["tag_rules"] = Json::arrayValue;
    return cfg;
}
void Gen::FrameFanout::configure(const WireCell::Configuration &cfg)
{
    int m = get<int>(cfg, "multiplicity", (int) m_multiplicity);
    if (m <= 0)
    {
        THROW(ValueError() << errmsg{"FrameFanout multiplicity must be positive"});
    }
    m_multiplicity = m;

    m_ft.configure(cfg["tag_rules"]);
}

std::vector<std::string> Gen::FrameFanout::output_types()
{
    const std::string tname = std::string(typeid(output_type).name());
    std::vector<std::string> ret(m_multiplicity, tname);
    return ret;
}

bool Gen::FrameFanout::operator()(const input_pointer &in,
                                  output_vector &outv)
{
    outv.resize(m_multiplicity);

    if (!in)
    {  //  pass on EOS
        for (size_t ind = 0; ind < m_multiplicity; ++ind)
        {
            outv[ind] = in;
        }
        log->debug("FrameFanout: see EOS");
        return true;
    }

    auto fintags = in->frame_tags();
    // Add empty tag to allow rules to create an output tag even if none exist.
    // No equivalent should be made for trace tags.
    fintags.push_back("");

    std::stringstream taginfo;

    for (size_t ind = 0; ind < m_multiplicity; ++ind)
    {
        // Basic frame stays the same.
        auto sfout =
            new SimpleFrame(in->ident(), in->time(), *in->traces(), in->tick());

        // Transform any frame tags based on a per output port ruleset
        auto fouttags = m_ft.transform(ind, "frame", fintags);

        for (auto ftag : fouttags)
        {
            sfout->tag_frame(ftag);
            taginfo << " ftag:" << ftag;
        }

        for (auto inttag : in->trace_tags())
        {
            tagrules::tagset_t touttags = m_ft.transform(ind, "trace", inttag);
            if (touttags.empty())
            {
                continue;
            }
            const auto &traces = in->tagged_traces(inttag);
            const auto &summary = in->trace_summary(inttag);
            for (auto otag : touttags)
            {
                sfout->tag_traces(otag, traces, summary);
                taginfo << " " << inttag << "->" << otag;
            }
        };

        outv[ind] = IFrame::pointer(sfout);
    }

    std::string tagmsg = taginfo.str();
    if (!tagmsg.empty())
    {
        log->debug("FrameFanout: tagnifo:{}", taginfo.str());
    }
    return true;
}

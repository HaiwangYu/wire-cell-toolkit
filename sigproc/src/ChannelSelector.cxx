#include "WireCellSigProc/ChannelSelector.h"
#include "WireCellIface/FrameTools.h"
#include "WireCellIface/SimpleFrame.h"

#include "WireCellUtil/NamedFactory.h"

#include <sstream>

WIRECELL_FACTORY(ChannelSelector, WireCell::SigProc::ChannelSelector,
                 WireCell::IFrameFilter, WireCell::IConfigurable)

using namespace WireCell;
using namespace WireCell::SigProc;

ChannelSelector::ChannelSelector()
  : log(Log::logger("glue"))
{
}

ChannelSelector::~ChannelSelector() {}

WireCell::Configuration ChannelSelector::default_configuration() const
{
    Configuration cfg;

    /// Only traces with channels in this array will be in the output.
    cfg["channels"] = Json::arrayValue;

    /// Only traces with these tags will be in the output.  If no tags
    /// are given then tags are not considered.
    cfg["tags"] = Json::arrayValue;

    return cfg;
}

void ChannelSelector::configure(const WireCell::Configuration &cfg)
{
    // tags need some order
    auto jtags = cfg["tags"];
    int ntags = jtags.size();
    m_tags.clear();
    m_tags.resize(ntags);
    for (int ind = 0; ind < ntags; ++ind)
    {
        m_tags[ind] = jtags[ind].asString();
    }

    // channels are just a bag
    for (auto jchan : cfg["channels"])
    {
        m_channels.insert(jchan.asInt());
    }
}

void ChannelSelector::set_channels(const std::vector<int> &channels)
{
    m_channels.clear();
    for (int ch : channels)
    {
        m_channels.insert(ch);
    }
}

bool ChannelSelector::operator()(const input_pointer &in, output_pointer &out)
{
    out = nullptr;
    if (!in)
    {
        log->debug("ChannelSelector: sees EOS");
        return true;  // eos
    }

    std::vector<ITrace::vector> tracesvin;

    size_t ntraces = 0;

    size_t ntags = m_tags.size();
    if (!ntags)
    {
        tracesvin.push_back(FrameTools::untagged_traces(in));
        log->debug(
            "ChannelSelector: see frame: {} no tags, whole frame ({} traces "
            "out of {})",
            in->ident(), tracesvin.back().size(), in->traces()->size());
        ntraces += tracesvin[0].size();
    }
    else
    {
        tracesvin.resize(ntags);
        std::stringstream ss;
        ss << "ChannelSelector: see frame: " << in->ident() << " looking for "
           << ntags << " tags:";
        for (size_t ind = 0; ind < ntags; ++ind)
        {
            std::string tag = m_tags[ind];
            tracesvin[ind] = FrameTools::tagged_traces(in, tag);
            ss << " " << tag << ":[" << tracesvin[ind].size() << " traces]";
            ntraces += tracesvin[ind].size();
        }
        log->debug(ss.str());
    }
    if (!ntraces)
    {
        log->warn("ChannelSelector: see no traces from frame {}", in->ident());
    }

    ITrace::vector out_traces;
    std::vector<IFrame::trace_list_t> tagged_trace_indices;

    for (size_t ind = 0; ind < tracesvin.size(); ++ind)
    {
        auto &traces = tracesvin[ind];

        IFrame::trace_list_t tl;
        for (size_t trind = 0; trind < traces.size(); ++trind)
        {
            auto &trace = traces[trind];
            if (m_channels.find(trace->channel()) == m_channels.end())
            {
                continue;
            }
            tl.push_back(out_traces.size());
            out_traces.push_back(trace);
        }
        tagged_trace_indices.push_back(tl);
    }

    std::stringstream taginfo;

    auto sf = new SimpleFrame(in->ident(), in->time(), out_traces, in->tick());
    if (ntags)
    {
        for (size_t ind = 0; ind < ntags; ++ind)
        {
            std::string tag = m_tags[ind];
            sf->tag_traces(tag, tagged_trace_indices[ind]);
            taginfo << tag << " ";
        }
    }
    for (auto ftag : in->frame_tags())
    {
        sf->tag_frame(ftag);
        taginfo << "frame tag: " << ftag;
    }

    out = IFrame::pointer(sf);
    log->debug("ChannelSelector: producing {} traces, tags: {}",
               out->traces()->size(), taginfo.str());

    return true;
}

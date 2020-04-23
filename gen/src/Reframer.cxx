#include "WireCellGen/Reframer.h"
#include "WireCellUtil/Units.h"

#include "WireCellIface/SimpleFrame.h"
#include "WireCellIface/SimpleTrace.h"

#include "WireCellUtil/NamedFactory.h"

#include <map>
#include <unordered_set>

WIRECELL_FACTORY(Reframer, WireCell::Gen::Reframer, WireCell::IFrameFilter, WireCell::IConfigurable)

using namespace std;
using namespace WireCell;

Gen::Reframer::Reframer()
  : m_toffset(0.0)
  , m_fill(0.0)
  , m_tbin(0)
  , m_nticks(0)
  , log(Log::logger("sim"))
{
}

Gen::Reframer::~Reframer() {}

WireCell::Configuration Gen::Reframer::default_configuration() const
{
    Configuration cfg;

    cfg["anode"] = "";

    cfg["tags"] = Json::arrayValue;
    cfg["tbin"] = m_tbin;
    cfg["nticks"] = m_nticks;
    cfg["toffset"] = m_toffset;
    cfg["fill"] = m_fill;

    return cfg;
}

void Gen::Reframer::configure(const WireCell::Configuration &cfg)
{
    const std::string anode_tn = cfg["anode"].asString();
    log->debug("Gen::Reframer: using anode: \"{}\"", anode_tn);
    m_anode = Factory::find_tn<IAnodePlane>(anode_tn);

    m_input_tags.clear();
    for (auto jtag : cfg["tags"]) {
        m_input_tags.push_back(jtag.asString());
    }
    m_toffset = get(cfg, "toffset", m_toffset);
    m_tbin = get(cfg, "tbin", m_tbin);
    m_fill = get(cfg, "fill", m_fill);
    m_nticks = get(cfg, "nticks", m_nticks);
}

bool Gen::Reframer::operator()(const input_pointer &inframe, output_pointer &outframe)
{
    if (!inframe) {
        outframe = nullptr;
        return true;
    }

    // Storage for samples indexed by channel ident.
    std::map<int, std::vector<float>> waves;

    // initialize a "rectangular" 2D array of samples
    for (auto chid : m_anode->channels()) {
        waves[chid].resize(m_nticks, m_fill);
    }

    // Get traces to consider
    std::vector<ITrace::pointer> traces;
    auto all_traces = inframe->traces();
    if (m_input_tags.empty()) {  // all traces
        traces.insert(traces.begin(), all_traces->begin(), all_traces->end());
    }
    else {
        // get tagged traces, but don't double count
        std::unordered_set<int> trace_indices;
        for (auto tag : m_input_tags) {
            auto indices = inframe->tagged_traces(tag);
            trace_indices.insert(indices.begin(), indices.end());
        }
        for (int ind : trace_indices) {
            traces.push_back(all_traces->at(ind));
        }
    }

    // Lay down input traces over output waves
    for (auto trace : traces) {
        const int chid = trace->channel();

        const int tbin_in = trace->tbin();
        auto in_it = trace->charge().begin();

        auto &wave = waves[chid];  // reference
        auto out_it = wave.begin();

        const int delta_tbin = m_tbin - tbin_in;
        if (delta_tbin > 0) {  // must truncate input
            in_it += delta_tbin;
        }
        else {  // must pad output
            out_it += -delta_tbin;
        }

        // Go as far as possible but don't walk of the end of either
        const auto in_end = trace->charge().end();
        const auto out_end = wave.end();
        while (in_it < in_end and out_it < out_end) {
            *out_it += *in_it;  // accumulate
            ++in_it;
            ++out_it;
        }
    }

    // Transfer waves into traces.
    ITrace::vector out_traces;
    for (auto &it : waves) {
        const int chid = it.first;
        auto &wave = it.second;
        auto out_trace = make_shared<SimpleTrace>(chid, 0, wave);
        out_traces.push_back(out_trace);
    }

    outframe = make_shared<SimpleFrame>(inframe->ident(), inframe->time() + m_toffset + m_tbin * inframe->tick(),
                                        out_traces, inframe->tick());
    log->debug("Gen::Reframer: frame {} {} traces, {} ticks", inframe->ident(), out_traces.size(), m_nticks);
    return true;
}

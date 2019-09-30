

#include "WireCellTbb/DataFlowGraph.h"

#include "WireCellUtil/Type.h"
#include "WireCellUtil/NamedFactory.h"

<<<<<<< HEAD
WIRECELL_FACTORY(TbbDataFlowGraph, WireCellTbb::DataFlowGraph, WireCell::IDataFlowGraph, WireCell::IConfigurable)
=======
#include <iostream>

WIRECELL_FACTORY(TbbDataFlowGraph, WireCellTbb::DataFlowGraph,
                 WireCell::IDataFlowGraph, WireCell::IConfigurable)
>>>>>>> 4e01cf770602cf83886437e4773b2c722643c5d0

using namespace std;
using namespace WireCell;
using namespace WireCellTbb;




DataFlowGraph::DataFlowGraph(int max_threads)
    : m_sched(max_threads > 0 ? max_threads : tbb::task_scheduler_init::automatic)
    , m_graph()
    , m_factory(m_graph)
    , l(Log::logger("tbb"))
{
}

DataFlowGraph::~DataFlowGraph()
{
}

Configuration DataFlowGraph::default_configuration() const
{
    Configuration cfg;
    cfg["max_threads"] = 1;
    return cfg;
}


void DataFlowGraph::configure(const Configuration& cfg)
{
    // int maxthreads = get<int>(cfg,"max_threads");
    // fixme: now what?
}


bool DataFlowGraph::connect(INode::pointer tail, INode::pointer head,
			    size_t sport, size_t rport)
{
    using namespace WireCellTbb;

    Node mytail = m_factory(tail);
    if (!mytail) {
    l->critical("DFP: failed to get tail node wrapper for {}", demangle(tail->signature()));
	return false;
    }

    Node myhead = m_factory(head);
    if (!myhead) {
    l->critical("DFP: failed to get head node wrapper for {}", demangle(head->signature()));
	return false;
    }

    auto sports = mytail->sender_ports();
    if (sport < 0 || sports.size() <= sport) {
    l->critical("DFP: bad sender port number: {}", sport);
	return false;
    }

    auto rports = myhead->receiver_ports();
    if (rport < 0 || rports.size() <= rport) {
    l->critical("DFP: bad receiver port number: {}", rport);
	return false;
    }

    
    sender_type* s = sports[sport];
    if (!s) {
    l->critical("DFP: failed to get sender port {}", sport);
	return false;
    }

    receiver_type* r = rports[rport];
    if (!s) {
    l->critical("DFP: failed to get receiver port {}", rport);
	return false;
    }

    make_edge(*s, *r);
    return true;
}
    

bool DataFlowGraph::run()
{
    for (auto it : m_factory.seen()) {
    l->debug("Initialize node of type: {}", demangle(it.first->signature()));
	it.second->initialize();
    }
    m_graph.wait_for_all();
    return true;
}


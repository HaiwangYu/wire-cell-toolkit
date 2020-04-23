#ifndef WIRECELLTBB_SINKCAT
#define WIRECELLTBB_SINKCAT

#include "WireCellIface/ISinkNode.h"
#include "WireCellTbb/NodeWrapper.h"

namespace WireCellTbb {
    // adapter to convert from WC sink node to TBB sink node body.
    class SinkBody {
        WireCell::ISinkNodeBase::pointer m_wcnode;

       public:
        ~SinkBody() {}

        SinkBody(WireCell::INode::pointer wcnode)
        {
            m_wcnode = std::dynamic_pointer_cast<WireCell::ISinkNodeBase>(wcnode);
        }
        boost::any operator()(const boost::any &in)
        {
            // bool ok = (*m_wcnode)(in);
            (*m_wcnode)(in);  // fixme: don't ignore the return code
            return in;
        }
    };

    // implement facade to access ports for sink nodes
    class SinkNodeWrapper : public NodeWrapper {
        sink_node *m_tbbnode;

       public:
        SinkNodeWrapper(tbb::flow::graph &graph, WireCell::INode::pointer wcnode)
          : m_tbbnode(new sink_node(graph, wcnode->concurrency(), SinkBody(wcnode)))
        {
        }
        ~SinkNodeWrapper() { delete m_tbbnode; }
        virtual receiver_port_vector receiver_ports()
        {
            auto ptr = dynamic_cast<receiver_type *>(m_tbbnode);
            return receiver_port_vector{ptr};
        }
    };
}  // namespace WireCellTbb
#endif

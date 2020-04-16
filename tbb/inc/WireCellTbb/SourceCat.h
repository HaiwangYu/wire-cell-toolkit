/** Facade/adapter to TBB for source node category. */
#ifndef WIRECELLTBB_SOURCECAT
#define WIRECELLTBB_SOURCECAT

#include "WireCellIface/ISourceNode.h"
#include "WireCellTbb/NodeWrapper.h"

namespace WireCellTbb
{
    // adapter to convert from WC source node to TBB source node body.
    class SourceBody
    {
        WireCell::ISourceNodeBase::pointer m_wcnode;

       public:
        ~SourceBody() {}

        SourceBody(WireCell::INode::pointer wcnode)
        {
            m_wcnode = std::dynamic_pointer_cast<WireCell::ISourceNodeBase>(wcnode);
        }
        bool operator()(boost::any &out) { return (*m_wcnode)(out); }
    };

    // implement facade to access ports for source nodes
    class SourceNodeWrapper : public NodeWrapper
    {
        source_node *m_tbbnode;

       public:
        SourceNodeWrapper(tbb::flow::graph &graph, WireCell::INode::pointer wcnode)
          : m_tbbnode(new source_node(graph, SourceBody(wcnode), false))
        {
        }
        ~SourceNodeWrapper() { delete m_tbbnode; }
        virtual void initialize() { m_tbbnode->activate(); }
        virtual sender_port_vector sender_ports()
        {
            auto ptr = dynamic_cast<sender_type *>(m_tbbnode);
            return sender_port_vector{ptr};
        }
    };
}  // namespace WireCellTbb
#endif

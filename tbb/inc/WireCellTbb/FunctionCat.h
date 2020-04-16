#ifndef WIRECELLTBB_FUNCTIONCAT
#define WIRECELLTBB_FUNCTIONCAT

#include "WireCellIface/IFunctionNode.h"
#include "WireCellTbb/NodeWrapper.h"

#include <iostream>  // temporary, don't ignore the error code, chump!

namespace WireCellTbb
{
    // Body for a TBB function node.
    class FunctionBody
    {
        WireCell::IFunctionNodeBase::pointer m_wcnode;

       public:
        FunctionBody(WireCell::INode::pointer wcnode)
        {
            m_wcnode = std::dynamic_pointer_cast<WireCell::IFunctionNodeBase>(wcnode);
        }
        virtual ~FunctionBody() {}

        boost::any operator()(const boost::any &in) const
        {
            boost::any ret;
            bool ok = (*m_wcnode)(in, ret);  // fixme: don't ignore the error code!
            if (!ok)
            {
                std::cerr << "I'm ignoring the error code!\n";
            }
            return ret;
        }
    };

    // Wrap the TBB (compound) node
    class FunctionWrapper : public NodeWrapper
    {
        tbb::flow::graph_node *m_tbbnode;

       public:
        FunctionWrapper(tbb::flow::graph &graph, WireCell::INode::pointer wcnode)
          : m_tbbnode(new function_node(graph, wcnode->concurrency(),
                                        FunctionBody(wcnode)))
        {
        }

        virtual receiver_port_vector receiver_ports()
        {
            return {dynamic_cast<receiver_type *>(m_tbbnode)};
        }

        virtual sender_port_vector sender_ports()
        {
            return {dynamic_cast<sender_type *>(m_tbbnode)};
        }
    };

}  // namespace WireCellTbb

#endif

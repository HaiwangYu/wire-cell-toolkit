/** This adapts the queued out node category to a TBB multifunction node.
 */

#ifndef WIRECELLTBB_QUEUEDOUT
#define WIRECELLTBB_QUEUEDOUT

#include "WireCellIface/IQueuedoutNode.h"
#include "WireCellTbb/NodeWrapper.h"

namespace WireCellTbb
{
    class QueuedoutBody
    {
        WireCell::IQueuedoutNodeBase::pointer m_wcnode;

       public:
        ~QueuedoutBody() {}
        QueuedoutBody(WireCell::INode::pointer wcnode)
        {
            m_wcnode = std::dynamic_pointer_cast<WireCell::IQueuedoutNodeBase>(wcnode);
        }
        void operator()(const boost::any &in, queuedout_port &out)
        {
            WireCell::IQueuedoutNodeBase::queuedany outq;
            bool ok = (*m_wcnode)(in, outq);
            if (!ok)
            {
                return;
            }  // fixme: do something better here!
            for (auto a : outq)
            {
                std::get<0>(out).try_put(a);
            }
        }
    };
    class QueuedoutWrapper : public NodeWrapper
    {
       public:
        QueuedoutWrapper(tbb::flow::graph &graph, WireCell::INode::pointer wcnode)
          : m_tbbnode(new queuedout_node(graph, wcnode->concurrency(),
                                         QueuedoutBody(wcnode)))
        {
        }
        virtual ~QueuedoutWrapper()
        {
            delete m_tbbnode;
            m_tbbnode = nullptr;
        }

        virtual receiver_port_vector receiver_ports()
        {
            auto ptr = dynamic_cast<receiver_type *>(m_tbbnode);
            return receiver_port_vector{ptr};
        }

        virtual sender_port_vector sender_ports()
        {
            auto ptr = &tbb::flow::output_port<0>(*m_tbbnode);
            return sender_port_vector{ptr};
        }

       private:
        queuedout_node *m_tbbnode;
    };

}  // namespace WireCellTbb
#endif

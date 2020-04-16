#ifndef WIRECELLTBB_JOINCAT
#define WIRECELLTBB_JOINCAT

#include "WireCellIface/IJoinNode.h"
#include "WireCellTbb/NodeWrapper.h"
#include "WireCellUtil/Testing.h"

namespace WireCellTbb
{
    // Body for a TBB join node.
    template <typename TupleType>
    class JoinBody
    {
        WireCell::IJoinNodeBase::pointer m_wcnode;

       public:
        typedef typename WireCell::IJoinNodeBase::any_vector any_vector;
        typedef typename WireCell::tuple_helper<TupleType> helper_type;

        JoinBody(WireCell::INode::pointer wcnode)
        {
            m_wcnode = std::dynamic_pointer_cast<WireCell::IJoinNodeBase>(wcnode);
            Assert(m_wcnode);
        }

        boost::any operator()(const TupleType &tup) const
        {
            helper_type ih;
            any_vector in = ih.as_any(tup);
            boost::any ret;
            // bool ok = (*m_wcnode)(in, ret);
            (*m_wcnode)(in, ret);  // fixme: don't ignore the return code
            return ret;
        }
    };

    template <std::size_t N>
    receiver_port_vector
    build_joiner(tbb::flow::graph &graph, WireCell::INode::pointer wcnode,
                 tbb::flow::graph_node *&joiner, tbb::flow::graph_node *&caller)
    {
        typedef typename WireCell::type_repeater<N, boost::any>::type TupleType;

        // this node is fully TBB and joins N receiver ports into a tuple
        typedef tbb::flow::join_node<TupleType> tbb_join_node_type;
        tbb_join_node_type *jn = new tbb_join_node_type(graph);
        joiner = jn;

        // this node takes user WC body and runs it after converting input tuple to
        // vector
        typedef tbb::flow::function_node<TupleType, boost::any> joining_node;
        joining_node *fn = new joining_node(graph, wcnode->concurrency(),
                                            JoinBody<TupleType>(wcnode));
        caller = fn;

        tbb::flow::make_edge(*jn, *fn);

        // JoinNodeInputPorts<TupleType,N> ports;
        // return ports(*jn);
        return receiver_ports(*jn);
    }

    // Wrap the TBB (compound) node
    class JoinWrapper : public NodeWrapper
    {
        tbb::flow::graph_node *m_joiner, *m_caller;
        receiver_port_vector m_receiver_ports;

       public:
        JoinWrapper(tbb::flow::graph &graph, WireCell::INode::pointer wcnode)
          : m_joiner(0)
          , m_caller(0)
        {
            int nin = wcnode->input_types().size();
            // an exhaustive switch to convert from run-time to compile-time types and
            // enumerations.
            Assert(nin > 0 && nin <= 3);  // fixme: exception instead?
            if (1 == nin)
                m_receiver_ports = build_joiner<1>(graph, wcnode, m_joiner, m_caller);
            if (2 == nin)
                m_receiver_ports = build_joiner<2>(graph, wcnode, m_joiner, m_caller);
            if (3 == nin)
                m_receiver_ports = build_joiner<3>(graph, wcnode, m_joiner, m_caller);
        }

        virtual receiver_port_vector receiver_ports() { return m_receiver_ports; }

        virtual sender_port_vector sender_ports()
        {
            auto ptr = dynamic_cast<sender_type *>(m_caller);
            return sender_port_vector{ptr};
        }
    };

}  // namespace WireCellTbb

#endif

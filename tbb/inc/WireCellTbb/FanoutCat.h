#ifndef WIRECELLTBB_FANOUTCAT
#define WIRECELLTBB_FANOUTCAT

#include "WireCellIface/IFanoutNode.h"
#include "WireCellTbb/NodeWrapper.h"
#include "WireCellUtil/Testing.h"
#include "WireCellUtil/TupleHelpers.h"

#include <iostream>

namespace WireCellTbb {
    // Body for a TBB split node.
    template <typename std::size_t N>
    class FanoutBody {
        WireCell::IFanoutNodeBase::pointer m_wcnode;

       public:
        typedef typename WireCell::IFanoutNodeBase::any_vector any_vector;
        typedef typename WireCell::type_repeater<N, boost::any>::type TupleType;
        // typedef typename WireCell::tuple_helper<TupleType> helper_type;

        FanoutBody(WireCell::INode::pointer wcnode)
        {
            m_wcnode = std::dynamic_pointer_cast<WireCell::IFanoutNodeBase>(wcnode);
            Assert(m_wcnode);
            // helper_type ih;
        }

        TupleType operator()(boost::any in) const
        {
            // helper_type ih;
            any_vector ret;
            // bool ok = (*m_wcnode)(in, ret);
            (*m_wcnode)(in, ret);  // fixme: don't ignore return code
            // return ih.from_any(vectorToTuple<N>(ret));
            return WireCell::vectorToTuple<N>(ret);
        }
    };

    template <std::size_t N>
    sender_port_vector build_fanouter(tbb::flow::graph &graph, WireCell::INode::pointer wcnode,
                                      tbb::flow::graph_node *&spliter, tbb::flow::graph_node *&caller)
    {
        typedef typename WireCell::type_repeater<N, boost::any>::type TupleType;

        // this node is fully TBB and splits N sender ports into a tuple
        typedef tbb::flow::split_node<TupleType> tbb_split_node_type;
        tbb_split_node_type *sp = new tbb_split_node_type(graph);
        spliter = sp;

        // this node takes user WC body and runs it after converting input verctor to
        // tuple
        typedef tbb::flow::function_node<boost::any, TupleType> spliting_node;
        spliting_node *fn = new spliting_node(graph, wcnode->concurrency(), FanoutBody<N>(wcnode));
        caller = fn;

        tbb::flow::make_edge(*fn, *sp);

        // FanoutNodeInputPorts<TupleType,N> ports;
        // return ports(*sp);
        return sender_ports(*sp);
    }

    // Wrap the TBB (compound) node
    class FanoutWrapper : public NodeWrapper {
        tbb::flow::graph_node *m_spliter, *m_caller;
        sender_port_vector m_sender_ports;

       public:
        FanoutWrapper(tbb::flow::graph &graph, WireCell::INode::pointer wcnode)
          : m_spliter(0)
          , m_caller(0)
        {
            int nout = wcnode->output_types().size();
            // an exhaustive switch to convert from run-time to compile-time types and
            // enumerations.
            Assert(nout > 0 && nout <= 6);  // fixme: exception instead?
            if (1 == nout) m_sender_ports = build_fanouter<1>(graph, wcnode, m_spliter, m_caller);
            if (2 == nout) m_sender_ports = build_fanouter<2>(graph, wcnode, m_spliter, m_caller);
            if (3 == nout) m_sender_ports = build_fanouter<3>(graph, wcnode, m_spliter, m_caller);
            if (4 == nout) m_sender_ports = build_fanouter<4>(graph, wcnode, m_spliter, m_caller);
            if (5 == nout) m_sender_ports = build_fanouter<5>(graph, wcnode, m_spliter, m_caller);
            if (6 == nout) m_sender_ports = build_fanouter<6>(graph, wcnode, m_spliter, m_caller);
        }

        virtual sender_port_vector sender_ports() { return m_sender_ports; }

        virtual receiver_port_vector receiver_ports()
        {
            auto ptr = dynamic_cast<receiver_type *>(m_caller);
            return receiver_port_vector{ptr};
        }
    };

}  // namespace WireCellTbb

#endif

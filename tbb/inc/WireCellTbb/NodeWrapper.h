#ifndef WIRECELLTBB_NODEWRAPPER
#define WIRECELLTBB_NODEWRAPPER

#include "WireCellIface/INode.h"
#include "WireCellUtil/TupleHelpers.h"

#include <tbb/flow_graph.h>
#include <boost/any.hpp>
#include <map>
#include <memory>

namespace WireCellTbb
{
    // Broken out sender/receiver types, vectors of their pointers
    typedef tbb::flow::sender<boost::any> sender_type;
    typedef tbb::flow::receiver<boost::any> receiver_type;

    typedef std::vector<sender_type *> sender_port_vector;
    typedef std::vector<receiver_type *> receiver_port_vector;

    typedef std::vector<boost::any> any_vector;

    typedef std::tuple<boost::any> any_single;
    typedef std::tuple<boost::any, boost::any> any_double;
    typedef std::tuple<boost::any, boost::any, boost::any> any_triple;

    /// Types for TBB nodes
    typedef tbb::flow::source_node<boost::any> source_node;
    typedef tbb::flow::function_node<boost::any> sink_node;
    typedef tbb::flow::function_node<boost::any, boost::any> function_node;
    typedef tbb::flow::multifunction_node<boost::any, any_single> queuedout_node;
    typedef queuedout_node::output_ports_type queuedout_port;

    // A base facade which expose sender/receiver ports and provide
    // initialize hook.  There is one NodeWrapper for each node
    // category.
    class NodeWrapper
    {
       public:
        virtual ~NodeWrapper() {}

        virtual sender_port_vector sender_ports() { return sender_port_vector(); }
        virtual receiver_port_vector receiver_ports()
        {
            return receiver_port_vector();
        }

        // call before running graph
        virtual void initialize() {}
    };

    // expose the wrappers only as a shared pointer
    typedef std::shared_ptr<NodeWrapper> Node;

    // internal
    template <typename Tuple, std::size_t... Is>
    receiver_port_vector receiver_ports(tbb::flow::join_node<Tuple> &jn,
                                        std::index_sequence<Is...>)
    {
        return {dynamic_cast<receiver_type *>(&tbb::flow::input_port<Is>(jn))...};
    }
    /// Return receiver ports of a join node as a vector.
    template <typename Tuple>
    receiver_port_vector receiver_ports(tbb::flow::join_node<Tuple> &jn)
    {
        return receiver_ports(
            jn, std::make_index_sequence<std::tuple_size<Tuple>::value>{});
    }

    template <typename Tuple, std::size_t... Is>
    sender_port_vector sender_ports(tbb::flow::split_node<Tuple> &sp,
                                    std::index_sequence<Is...>)
    {
        return {dynamic_cast<sender_type *>(&tbb::flow::output_port<Is>(sp))...};
    }
    /// Return sender ports of a split node as a vector.
    template <typename Tuple>
    sender_port_vector sender_ports(tbb::flow::split_node<Tuple> &sp)
    {
        return sender_ports(
            sp, std::make_index_sequence<std::tuple_size<Tuple>::value>{});
    }

}  // namespace WireCellTbb

#endif

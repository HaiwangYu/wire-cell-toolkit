#ifndef WIRECELL_IFACE_ITENSORFANOUT
#define WIRECELL_IFACE_ITENSORFANOUT

#include "WireCellIface/IFanoutNode.h"
#include "WireCellIface/ITensor.h"

namespace WireCell {
    /** A Tensor fan-out component takes 1 input Tensor and produces one
     * Tensor on each of its output ports.  What each of those N Tensor
     * are depends on implementation.
     */
    class ITensorFanout : public IFanoutNode<ITensor, ITensor, 0> {
       public:
        virtual ~ITensorFanout();

        virtual std::string signature() { return typeid(ITensorFanout).name(); }

        // Subclass must implement:
        virtual std::vector<std::string> output_types() = 0;
        // and the already abstract:
        // virtual bool operator()(const input_pointer& in, output_vector& outv);
    };
}  // namespace WireCell

#endif
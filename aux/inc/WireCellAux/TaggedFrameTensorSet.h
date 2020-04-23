#ifndef WIRECELL_AUX_TAGGEDFRAMETENSORSET
#define WIRECELL_AUX_TAGGEDFRAMETENSORSET

#include "WireCellIface/IConfigurable.h"
#include "WireCellIface/IFrameTensorSet.h"

namespace WireCell {
    namespace Aux {
        /*! Produce a set of 2D tensors and metadata from a frame.
         *
         * The inverse converter is @ref TaggedTensorSetFrame.
         *
         * The converter configuration consists of an object
         * containing an attribute "tensors" that holds an array of
         * "tensor config objects".  Each object has the attributes:
         *
         * - tag :: the frame trace tag supplying the traces
         * - pad :: number to pad sparse tensors (def=0.0)
         *
         * There will be one output tensor in the set per tensor
         * config object in the "tensors" array.  If no matching
         * tensor can be made the output tensor (pointer) will be null
         * (and NOT a fully padded tensor).
         *
         * Tensors are produced row-major (C-style) order with a row
         * holding one waveform from one channel.  Sparse frames
         * result in dense, padded tensors (padded with "pad" value,
         * if given).
         *
         * The ITensorSet also provides a metadata object with these
         * attributes:
         *
         * time - float, the frame's reference time  (in WCT s.o.u.)
         * tick - float, the sample period (in WCT s.o.u.)
         * tensors - a per-output tensor metadata object
         *
         * Each element of the latter forwards the attributes of the
         * input tensor config object and sets these additional attributes:
         *
         * tbin - the time bin of the first column of the tensor
         * channels - an array holding channel IDs associated with each row of the
         * tensor. summary - an optional array, corresponding 1-to-2 with channels
         * holding "channel summary" values.
         *
         */
        class TaggedFrameTensorSet : public WireCell::IConfigurable, public WireCell::IFrameTensorSet {
           public:
            TaggedFrameTensorSet();
            virtual ~TaggedFrameTensorSet();

            virtual bool operator()(const input_pointer &in, output_pointer &out);

            virtual WireCell::Configuration default_configuration() const;
            virtual void configure(const WireCell::Configuration &config);

           private:
            WireCell::Configuration m_cfg;
        };
    }  // namespace Aux
}  // namespace WireCell

#endif

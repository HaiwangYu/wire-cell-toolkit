#ifndef WIRECELL_AUX_TENSORFANOUT
#define WIRECELL_AUX_TENSORFANOUT

#include "WireCellIface/ITensorFanout.h"
#include "WireCellIface/IConfigurable.h"
#include "WireCellUtil/Logging.h"

namespace WireCell {
    namespace Aux {

        // Fan out 1 Tensor to N set at construction or configuration time.
        class TensorFanout : public ITensorFanout, public IConfigurable {
           public:
            TensorFanout(size_t multiplicity = 0);
            virtual ~TensorFanout();

            // INode, override because we get multiplicity at run time.
            virtual std::vector<std::string> output_types();

            // IFanout
            virtual bool operator()(const input_pointer& in, output_vector& outv);

            // IConfigurable
            virtual void configure(const WireCell::Configuration& cfg);
            virtual WireCell::Configuration default_configuration() const;

           private:
            size_t m_multiplicity;

            Log::logptr_t log;
        };
    }  // namespace Gen
}  // namespace WireCell

#endif
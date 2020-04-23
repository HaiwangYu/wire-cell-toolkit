#ifndef WIRECELL_IMG_SLICEFANOUT
#define WIRECELL_IMG_SLICEFANOUT

#include "WireCellIface/IConfigurable.h"
#include "WireCellIface/ISliceFanout.h"
#include "WireCellUtil/Logging.h"

namespace WireCell {
    namespace Img {
        // Fan out 1 slice to N set at construction or configuration time.
        class SliceFanout : public ISliceFanout, public IConfigurable {
           public:
            SliceFanout(size_t multiplicity = 0);
            virtual ~SliceFanout();

            // INode, override because we get multiplicity at run time.
            virtual std::vector<std::string> output_types();

            // IFanout
            virtual bool operator()(const input_pointer &in, output_vector &outv);

            // IConfigurable
            virtual void configure(const WireCell::Configuration &cfg);
            virtual WireCell::Configuration default_configuration() const;

           private:
            size_t m_multiplicity;
            Log::logptr_t l;
        };
    }  // namespace Img
}  // namespace WireCell

#endif

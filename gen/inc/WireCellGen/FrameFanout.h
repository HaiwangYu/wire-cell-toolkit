#ifndef WIRECELL_GEN_FRAMEFANOUT
#define WIRECELL_GEN_FRAMEFANOUT

#include "WireCellIface/IConfigurable.h"
#include "WireCellIface/IFrameFanout.h"
#include "WireCellUtil/Logging.h"
#include "WireCellUtil/TagRules.h"

namespace WireCell {
    namespace Gen {
        // Fan out 1 frame to N set at construction or configuration time.
        class FrameFanout : public IFrameFanout, public IConfigurable {
           public:
            FrameFanout(size_t multiplicity = 0);
            virtual ~FrameFanout();

            // INode, override because we get multiplicity at run time.
            virtual std::vector<std::string> output_types();

            // IFanout
            virtual bool operator()(const input_pointer &in, output_vector &outv);

            // IConfigurable
            virtual void configure(const WireCell::Configuration &cfg);
            virtual WireCell::Configuration default_configuration() const;

           private:
            size_t m_multiplicity;

            tagrules::Context m_ft;
            Log::logptr_t log;
        };
    }  // namespace Gen
}  // namespace WireCell

#endif

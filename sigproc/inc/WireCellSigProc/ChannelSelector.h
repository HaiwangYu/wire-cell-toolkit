/**
   Make a new output frame with a set of traces selected from the
   input based on being in a set of channels and possibly tags.
 */

#ifndef WIRECELLSIGPROC_CHANNELSELECTOR
#define WIRECELLSIGPROC_CHANNELSELECTOR

#include "WireCellIface/IConfigurable.h"
#include "WireCellIface/IFrameFilter.h"
#include "WireCellUtil/Logging.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace WireCell {
    namespace SigProc {
        class ChannelSelector : public IFrameFilter, public IConfigurable {
           public:
            ChannelSelector();
            virtual ~ChannelSelector();

            /// IFrameFilter interface.
            virtual bool operator()(const input_pointer &in, output_pointer &out);

            /// IConfigurable interface.
            virtual void configure(const WireCell::Configuration &config);
            virtual WireCell::Configuration default_configuration() const;

           protected:
            virtual void set_channels(const std::vector<int> &channels);

           private:
            std::vector<std::string> m_tags;
            std::unordered_set<int> m_channels;

            Log::logptr_t log;
        };
    }  // namespace SigProc
}  // namespace WireCell

#endif

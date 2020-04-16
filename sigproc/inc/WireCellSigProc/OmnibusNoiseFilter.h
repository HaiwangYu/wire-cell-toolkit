/** Remove all possible noise from a microboone-like detector.
 *
 * This filter is a kitchen sink class and is a candidate for
 * factoring.
 */

#ifndef WIRECELLSIGPROC_OMNIBUSNOISEFILTER
#define WIRECELLSIGPROC_OMNIBUSNOISEFILTER

#include "WireCellIface/IChannelFilter.h"
#include "WireCellIface/IChannelNoiseDatabase.h"
#include "WireCellIface/IConfigurable.h"
#include "WireCellIface/IFrameFilter.h"

#include "WireCellUtil/Logging.h"
#include "WireCellUtil/Waveform.h"

#include <map>
#include <string>
#include <vector>

namespace WireCell
{
    namespace SigProc
    {
        /**
   The Omnibus Noise Filter applies two series of IChannelFilter
   objects. The first series is applied on a per-channel basis and
   the second is applied on groups of channels as determined by
   its channel grouping.
*/
        class OmnibusNoiseFilter : public WireCell::IFrameFilter,
                                   public WireCell::IConfigurable
        {
           public:
            typedef std::vector<std::vector<int>> grouped_channels_t;

            /// Create an OmnibusNoiseFilter.
            OmnibusNoiseFilter(std::string intag = "orig", std::string outtag = "raw");
            virtual ~OmnibusNoiseFilter();

            /// IFrameFilter interface.
            virtual bool operator()(const input_pointer &in, output_pointer &out);

            /// IConfigurable interface.
            virtual void configure(const WireCell::Configuration &config);
            virtual WireCell::Configuration default_configuration() const;

            // Local methods. Real code should not use these.  Expose for unit tests

            void
            set_channel_filters(std::vector<WireCell::IChannelFilter::pointer> filters)
            {
                m_perchan = filters;
            }
            void set_channel_status_filters(
                std::vector<WireCell::IChannelFilter::pointer> filters)
            {
                m_perchan_status = filters;
            }
            void
            set_grouped_filters(std::vector<WireCell::IChannelFilter::pointer> filters)
            {
                m_grouped = filters;
            }
            void set_channel_noisedb(WireCell::IChannelNoiseDatabase::pointer ndb)
            {
                m_noisedb = ndb;
            }

           private:
            // number of time ticks in the waveforms processed.  Set to 0 and first input
            // trace sets it.
            size_t m_nticks;
            std::string m_intag, m_outtag;
            std::vector<WireCell::IChannelFilter::pointer> m_perchan, m_grouped,
                m_perchan_status;
            WireCell::IChannelNoiseDatabase::pointer m_noisedb;

            std::map<std::string, std::string> m_maskmap;

            Log::logptr_t log;
        };

    }  // namespace SigProc
}  // namespace WireCell
#endif

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

/** Init Decon2D for signal processing
 */

#ifndef WIRECELLSIG_DECON2DRESPONSE
#define WIRECELLSIG_DECON2DRESPONSE

#include "WireCellIface/IConfigurable.h"
#include "WireCellIface/ITensorSetFilter.h"
#include "WireCellUtil/Logging.h"

namespace WireCell {
    namespace Sig {
        class Decon2DResponse : public ITensorSetFilter, public IConfigurable {
           public:
            Decon2DResponse();
            virtual ~Decon2DResponse() {}

            // IConfigurable interface
            virtual void configure(const WireCell::Configuration &config);
            virtual WireCell::Configuration default_configuration() const;

            // ITensorSetFilter interface
            virtual bool operator()(const ITensorSet::pointer &in, ITensorSet::pointer &out);

           private:
            Log::logptr_t l;
            Configuration m_cfg;  /// copy of configuration

            std::unordered_map<std::string, float> m_timers;
        };
    }  // namespace Sig
}  // namespace WireCell

#endif  // WIRECELLSIG_DECON2DRESPONSE
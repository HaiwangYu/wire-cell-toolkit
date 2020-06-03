/** ROI refinement
 */

#ifndef WIRECELLSIG_ROIREFINE
#define WIRECELLSIG_ROIREFINE

#include "WireCellIface/IConfigurable.h"
#include "WireCellIface/ITensorSetFilter.h"
#include "WireCellUtil/Logging.h"

namespace WireCell {
    namespace Sig {
        class ROIRefine : public ITensorSetFilter, public IConfigurable {
           public:
            ROIRefine();
            virtual ~ROIRefine() {}

            // IConfigurable interface
            virtual void configure(const WireCell::Configuration& config);
            virtual WireCell::Configuration default_configuration() const;

            // ITensorSetFilter interface
            virtual bool operator()(const ITensorSet::pointer& in, ITensorSet::pointer& out);

           private:
            Log::logptr_t log;
            Configuration m_cfg;  /// copy of configuration
        };
    }  // namespace Sig
}  // namespace WireCell

#endif  // WIRECELLSIG_ROIREFINE
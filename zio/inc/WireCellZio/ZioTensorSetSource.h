/**
 * zio::Message -> WireCell::ITensorSet
 */

#ifndef WIRECELLZIO_ZIOTENSORSETSOURCE
#define WIRECELLZIO_ZIOTENSORSETSOURCE

#include "WireCellIface/ITensorSetSource.h"
#include "WireCellUtil/Logging.h"
#include "WireCellZio/FlowConfigurable.h"

namespace WireCell
{
    namespace Zio
    {
        class ZioTensorSetSource : public ITensorSetSource, public FlowConfigurable
        {
           public:
            ZioTensorSetSource();
            virtual ~ZioTensorSetSource();

            /// IFrameSource
            virtual bool operator()(ITensorSet::pointer &out);

           private:
            Log::logptr_t l;
            bool m_had_eos;
            ITensorSet::vector m_tensors;  // current set of depos
        };
    }  // namespace Zio
}  // namespace WireCell
#endif  // WIRECELLZIO_ZIOTENSORSETSOURCE
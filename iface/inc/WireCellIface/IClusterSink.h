#ifndef WIRECELL_ICLUSTERSINK
#define WIRECELL_ICLUSTERSINK

#include "WireCellIface/ICluster.h"
#include "WireCellIface/ISinkNode.h"

namespace WireCell {
    class IClusterSink : public ISinkNode<ICluster> {
       public:
        typedef std::shared_ptr<IClusterSink> pointer;

        virtual ~IClusterSink();

        virtual std::string signature() { return typeid(IClusterSink).name(); }

        // supply:
        // virtual bool operator()(const ICluster::pointer& cluster);
    };
}  // namespace WireCell

#endif

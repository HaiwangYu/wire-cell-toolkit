#ifndef WIRECELL_IBLOBSETSINK
#define WIRECELL_IBLOBSETSINK

#include "WireCellIface/IBlobSet.h"
#include "WireCellIface/ISinkNode.h"

namespace WireCell {
    /** A blob set sink is a node that consumes IBlobSet objects.
     */
    class IBlobSetSink : public ISinkNode<IBlobSet> {
       public:
        virtual ~IBlobSetSink();

        virtual std::string signature() { return typeid(IBlobSetSink).name(); }

        // supply:
        // virtual bool operator()(const IBlobSet::pointer& bs);
    };

}  // namespace WireCell

#endif

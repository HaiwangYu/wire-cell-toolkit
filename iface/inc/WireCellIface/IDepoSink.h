#ifndef WIRECELL_IDEPOSINK
#define WIRECELL_IDEPOSINK

#include "WireCellIface/IDepo.h"
#include "WireCellIface/ISinkNode.h"

namespace WireCell {
    /** A depo sink is a node that consumes IDepo objects.
     */
    class IDepoSink : public ISinkNode<IDepo> {
       public:
        virtual ~IDepoSink();

        virtual std::string signature() { return typeid(IDepoSink).name(); }

        // supply:
        // virtual bool operator()(const IDepo::pointer& depo);
    };

}  // namespace WireCell

#endif

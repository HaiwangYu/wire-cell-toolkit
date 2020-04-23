#ifndef WIRECELL_IDEPOFILTER
#define WIRECELL_IDEPOFILTER

#include "WireCellIface/IDepo.h"
#include "WireCellIface/IFunctionNode.h"

namespace WireCell {
    /** Depos go in, depos go out.
     */
    class IDepoFilter : public IFunctionNode<IDepo, IDepo> {
       public:
        virtual ~IDepoFilter();

        typedef std::shared_ptr<IDepoFilter> pointer;

        virtual std::string signature() { return typeid(IDepoFilter).name(); }

        // supply:
        // virtual bool operator()(const input_pointer& in, output_pointer& out);
    };
}  // namespace WireCell

#endif

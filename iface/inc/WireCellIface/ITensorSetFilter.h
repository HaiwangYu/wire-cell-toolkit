/** A wrapper for pytorch torchscript
 */

#ifndef WIRECELL_ITENSORSETFILTER
#define WIRECELL_ITENSORSETFILTER

#include "WireCellIface/IFunctionNode.h"
#include "WireCellIface/ITensorSet.h"
#include "WireCellUtil/IComponent.h"

namespace WireCell
{
    class ITensorSetFilter : public IFunctionNode<ITensorSet, ITensorSet>
    {
       public:
        typedef std::shared_ptr<ITensorSetFilter> pointer;

        virtual ~ITensorSetFilter();

        virtual std::string signature() { return typeid(ITensorSetFilter).name(); }

        // supply:
        // virtual bool operator()(const input_pointer& in, output_pointer& out);
    };
}  // namespace WireCell

#endif  // WIRECELL_ITENSORSETFILTER
/** A wrapper for pytorch torchscript
 */

#ifndef WIRECELLPYTORCH_TORCHSCRIPT
#define WIRECELLPYTORCH_TORCHSCRIPT

#include "WireCellIface/IConfigurable.h"
#include "WireCellIface/ITensorSetFilter.h"
#include "WireCellUtil/Logging.h"

#include <torch/script.h>  // One-stop header.

namespace WireCell {
    namespace Pytorch {
        class TorchScript : public ITensorSetFilter, public IConfigurable {
           public:
            TorchScript();
            virtual ~TorchScript() {}

            // IConfigurable interface
            virtual void configure(const WireCell::Configuration &config);
            virtual WireCell::Configuration default_configuration() const;

            // ITensorSetFilter interface
            virtual bool operator()(const ITensorSet::pointer &in, ITensorSet::pointer &out);

           private:
            Log::logptr_t l;
            Configuration m_cfg;  /// copy of configuration

            torch::jit::script::Module m_module;

            std::unordered_map<std::string, float> m_timers;
        };
    }  // namespace Pytorch
}  // namespace WireCell

#endif  // WIRECELLPYTORCH_TORCHSCRIPT
#include "WireCellPytorch/TorchScript.h"
#include "WireCellPytorch/Util.h"
#include "WireCellUtil/NamedFactory.h"
#include "WireCellUtil/String.h"

WIRECELL_FACTORY(TorchScript, WireCell::Pytorch::TorchScript,
                 WireCell::ITensorSetFilter, WireCell::IConfigurable)

using namespace WireCell;

Pytorch::TorchScript::TorchScript()
  : l(Log::logger("pytorch"))
{
}

Configuration Pytorch::TorchScript::default_configuration() const
{
    Configuration cfg;

    // TorchScript model
    cfg["model"] = "model.ts";
    cfg["gpu"] = true;

    // if failed, wait this time and try again
    cfg["wait_time"] = 500;  // ms

    // for debug
    cfg["nloop"] = 10;

    return cfg;
}

void Pytorch::TorchScript::configure(const WireCell::Configuration &cfg)
{
    m_cfg = cfg;
    const bool gpu = get<bool>(m_cfg, "gpu", false);

    auto model_path = m_cfg["model"].asString();
    if (model_path.empty())
    {
        THROW(ValueError() << errmsg{"Must provide output model to TorchScript"});
    }

    // load Torch Script Model
    try
    {
        // Deserialize the ScriptModule from a file using torch::jit::load().
        m_module = torch::jit::load(m_cfg["model"].asString());
        if (gpu)
            m_module.to(at::kCUDA);
        else
            m_module.to(at::kCPU);
        l->info("Model: {} loaded", m_cfg["model"].asString());
    }
    catch (const c10::Error &e)
    {
        l->critical("error loading model: {}", m_cfg["model"].asString());
        exit(0);
    }
}

bool Pytorch::TorchScript::operator()(const ITensorSet::pointer &in,
                                      ITensorSet::pointer &out)
{
    if (!in)
    {
        out = nullptr;
        return true;
    }

    int wait_time = m_cfg["wait_time"].asInt();
    const bool gpu = get<bool>(m_cfg, "gpu", false);
    int thread_wait_time = 0;

    // for(int iloop=0; iloop<m_cfg["nloop"].asInt();++iloop) {
    bool success = false;
    while (!success)
    {
        try
        {
            auto iival = Pytorch::from_itensor(in, gpu);
            auto oival = m_module.forward(iival);
            out = Pytorch::to_itensor({oival});

            success = true;
        }
        catch (...)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
            thread_wait_time += wait_time;
        }
    }
    // }

    l->info("thread_wait_time: {} sec", thread_wait_time / 1000.);

    return true;
}
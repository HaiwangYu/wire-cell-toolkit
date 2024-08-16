#include <torch/script.h>  // One-stop header.
#include <chrono>
#include "WireCellUtil/ExecMon.h"
#include "WireCellUtil/Logging.h"
using namespace WireCell;
using spdlog::info;
/**
 * to use single CPU thread, export OMP_NUM_THREADS=1 when running the program
*/
int main(int argc, const char* argv[])
{
    if(argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <nchunks>\n";
        return 1;
    }
    const size_t nchunks = atoi(argv[1]);
    size_t nsecsleep = 0;
    if (argc > 2) {
        nsecsleep = atoi(argv[2]);
    }

    // Disable gradient computation, use like a mutex lock
    torch::NoGradGuard no_grad;

    std::cout << "WireCell::pytorch : test loading TorchScript Model\n";

    const std::string mname = "model.ts";
    auto dtype = torch::kFloat32;

    ExecMon em("initialize");
    torch::jit::script::Module module;
    // Deserialize the ScriptModule from a file using torch::jit::load().
    auto start = std::chrono::high_resolution_clock::now();
    module = torch::jit::load(mname);
    std::cout << em("load model") << std::endl;
    module.to(at::kCPU, dtype);
    info(em("to device"));
    std::cout << em("to device") << std::endl;
    torch::TensorOptions options = torch::TensorOptions().dtype(dtype);
    torch::Tensor iten = torch::rand({1, 3, 2000, 340}, options);
    std::cout << em("make input tensor") << std::endl;
    auto chunks = iten.chunk(nchunks, 2);
    std::cout << em("chunking") << std::endl;
    std::vector<torch::Tensor> outputs;
    // use these sleeps to identify the places where the program is at
    std::this_thread::sleep_for(std::chrono::seconds(nsecsleep));
    for (auto chunk : chunks) {
        std::cout << "chunk size: " << chunk.sizes() << std::endl;
        std::vector<torch::IValue> itens {chunk};
        auto otens = module.forward(itens).toTensor().cpu();
        std::this_thread::sleep_for(std::chrono::seconds(nsecsleep));
        std::cout << "otens size: " << chunk.sizes() << std::endl;
        outputs.push_back(otens);
    }
    std::this_thread::sleep_for(std::chrono::seconds(nsecsleep));
    torch::Tensor combined = torch::cat(outputs, 2);
    std::cout << "Combined size: " << combined.sizes() << std::endl;
    std::cout << em("inference") << std::endl;
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "timing: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms\n";

    return 0;
}
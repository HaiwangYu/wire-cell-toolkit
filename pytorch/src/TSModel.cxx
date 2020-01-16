#include "WireCellPytorch/TSModel.h"
#include "WireCellIface/ITrace.h"
#include "WireCellIface/SimpleFrame.h"
#include "WireCellIface/SimpleTrace.h"

#include "WireCellIface/FrameTools.h"
#include "WireCellUtil/NamedFactory.h"
#include "WireCellUtil/Array.h"

#include <h5cpp/all>
#include <torch/script.h> // One-stop header.

#include <string>
#include <vector>

/// macro to register name - concrete pair in the NamedFactory
/// @param NAME - used to configure node in JSON/Jsonnet
/// @parame CONCRETE - C++ concrete type
/// @parame ... - interfaces
WIRECELL_FACTORY(TSModel, WireCell::Pytorch::TSModel,
                 WireCell::IFrameSink, WireCell::IConfigurable)

using namespace WireCell;

Pytorch::TSModel::TSModel() : m_save_count(0), l(Log::logger("pytorch")) {}

Pytorch::TSModel::~TSModel() {}

void Pytorch::TSModel::configure(const WireCell::Configuration &cfg) {

  auto anode_tn = cfg["anode"].asString();
  m_anode = Factory::find_tn<IAnodePlane>(anode_tn);

  auto model_path = cfg["model"].asString();
  if (model_path.empty()) {
    THROW(ValueError() << errmsg{
              "Must provide output filename to HDF5FrameTap"});
  }

  m_cfg = cfg;
}

WireCell::Configuration Pytorch::TSModel::default_configuration() const {
  Configuration cfg;

  // This number is set to the waveform sample array before any
  // charge is added.
  cfg["baseline"] = 0.0;

  // This number will be multiplied to each waveform sample before
  // casting to dtype.
  cfg["scale"] = 1.0;

  // This number will be added to each scaled waveform sample before
  // casting to dtype.
  cfg["offset"] = 0.0;

  cfg["model"] = "model.pt";

  cfg["anode"] = "AnodePlane";
  cfg["nticks"] = 6000;
  cfg["tick0"]  = 0;

  // taces used as input
  cfg["trace_tags"] = Json::arrayValue;

  return cfg;
}

namespace {
  Array::array_xxf rebin(const Array::array_xxf &in, const unsigned int k) {
    Array::array_xxf out = Array::array_xxf::Zero(in.rows()/k, in.cols());
    for(unsigned int i=0; i<in.rows(); ++i) {
      out.row(i/k) = out.row(i/k) + in.row(i);
    }
    return out/k;
  }
}


Array::array_xxf Pytorch::TSModel::frame_to_eigen(const IFrame::pointer &inframe, const std::string & tag) const {
  const unsigned int tick_rebin = 10;
  
  const float baseline = m_cfg["baseline"].asFloat();
  const float scale = m_cfg["scale"].asFloat();
  const float offset = m_cfg["offset"].asFloat();

  const int tick0 = m_cfg["tick0"].asInt();
  const int nticks = m_cfg["nticks"].asInt();
  const int tbeg = tick0;
  const int tend = tick0+nticks-1;
  auto channels = m_anode->channels();
  const int cbeg = channels.front();
  const int cend = channels.back();
  l->debug("{}: t: {} - {}; c: {} - {}",
            m_cfg["anode"].asString(),
            tbeg, tend, cbeg, cend);        

  const size_t ncols = nticks;
  const size_t nrows = cend-cbeg+1;

  auto traces = FrameTools::tagged_traces(inframe, tag);
  l->debug("TSModel: save {} tagged as {}", traces.size(), tag);
  if (traces.empty()) {
      l->warn("TSModel: no traces for tag: \"{}\"", tag);
      return Array::array_xxf::Zero(nrows/tick_rebin, ncols);
  }

  Array::array_xxf arr = Array::array_xxf::Zero(nrows, ncols) + baseline;
  FrameTools::fill(arr, traces, channels.begin(), channels.end(), tick0);
  arr = arr * scale + offset;

  return rebin(arr,tick_rebin);
}

bool Pytorch::TSModel::operator()(const IFrame::pointer &inframe,
                                      IFrame::pointer &outframe) {

  outframe = inframe;
  if (!inframe) {
      l->debug("TSModel: EOS");
      outframe = nullptr;
      return true;
  }

  if (m_cfg["trace_tags"].size() != 3) {
    return true;
  }

  // frame to eigen
  std::vector<Array::array_xxf> ch_eigen;
  for (auto jtag : m_cfg["trace_tags"]) {
    const std::string tag = jtag.asString();
    ch_eigen.push_back(frame_to_eigen(inframe, tag));
  }

  // eigen to tensor
  torch::Tensor ch[3];
  for(unsigned int i=0; i<3; ++i) {
    ch[i] = torch::from_blob(ch_eigen[i].data(), {ch_eigen[i].cols(),ch_eigen[i].rows()});
  }
  auto img = torch::stack({ch[0], ch[1], ch[2]}, 0);
  auto batch = torch::stack({img}, 0);


  // load Torch Script Model
  torch::jit::script::Module module;
  try {
    // Deserialize the ScriptModule from a file using torch::jit::load().
    module = torch::jit::load(m_cfg["model"].asString());
  }
  catch (const c10::Error& e) {
    std::cerr << "error loading the model\n";
    return -1;
  }

  // Create a vector of inputs.
  std::vector<torch::jit::IValue> inputs;
  inputs.push_back(batch.cuda());

  // Execute the model and turn its output into a tensor.
  torch::Tensor output = module.forward(inputs).toTensor().cpu();

  // tensor to eigen
  Eigen::Map<Eigen::ArrayXXf> out_e(output[0][0].data_ptr<float>(), output.size(3), output.size(2));

  // eigen to frame

  const int ncols = out_e.cols();
  const int nrows = out_e.rows();
  const std::string aname = String::format("/frame_%s", "dlsp");
  h5::fd_t fd = h5::open("tsmodel-eval.h5", H5F_ACC_RDWR);
  h5::write<float>(fd, aname, out_e.data(), h5::count{ncols, nrows}, h5::chunk{ncols, nrows} | h5::gzip{2}, h5::high_throughput);

  return true;
}

// Local Variables:
// mode: c++
// c-basic-offset: 2
// End:
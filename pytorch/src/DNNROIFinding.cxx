#include "WireCellPytorch/DNNROIFinding.h"
#include "WireCellIface/ITrace.h"
#include "WireCellIface/SimpleFrame.h"
#include "WireCellIface/SimpleTrace.h"

#include "WireCellIface/FrameTools.h"
#include "WireCellUtil/NamedFactory.h"
#include "WireCellUtil/Array.h"

#include <h5cpp/all>

#include <string>
#include <vector>

/// macro to register name - concrete pair in the NamedFactory
/// @param NAME - used to configure node in JSON/Jsonnet
/// @parame CONCRETE - C++ concrete type
/// @parame ... - interfaces
WIRECELL_FACTORY(DNNROIFinding, WireCell::Pytorch::DNNROIFinding,
                 WireCell::IFrameFilter, WireCell::IConfigurable)

using namespace WireCell;

Pytorch::DNNROIFinding::DNNROIFinding() : m_save_count(0), l(Log::logger("pytorch")) {}

Pytorch::DNNROIFinding::~DNNROIFinding() {}

void Pytorch::DNNROIFinding::configure(const WireCell::Configuration &cfg) {

  auto anode_tn = cfg["anode"].asString();
  m_anode = Factory::find_tn<IAnodePlane>(anode_tn);

  auto model_path = cfg["model"].asString();
  if (model_path.empty()) {
    THROW(ValueError() << errmsg{
              "Must provide output model to DNNROIFinding"});
  }

  
  std::string fn = cfg["evalfile"].asString();
  if (fn.empty()) {
    THROW(ValueError() << errmsg{
              "Must provide output evalfile to DNNROIFinding"});
  }

  h5::create(fn, H5F_ACC_TRUNC);

  m_cfg = cfg;

  // load Torch Script Model
  try {
    // Deserialize the ScriptModule from a file using torch::jit::load().
    module = torch::jit::load(m_cfg["model"].asString());
    if(m_cfg["gpu"].asBool()) module.to(at::kCUDA);
    else module.to(at::kCPU);
    l->info("Model: {} loaded",m_cfg["model"].asString());
  }
  catch (const c10::Error& e) {
    l->critical( "error loading model: {}", m_cfg["model"].asString());
    exit(0);
  }

  m_timers.insert({"frame2eigen",0});
  m_timers.insert({"eigen2tensor",0});
  m_timers.insert({"forward",0});
  m_timers.insert({"tensor2eigen",0});
  m_timers.insert({"h5eval",0});
  m_timers.insert({"eigen2frame",0});
}

WireCell::Configuration Pytorch::DNNROIFinding::default_configuration() const {
  Configuration cfg;

  // anode
  cfg["anode"] = "AnodePlane";

  // DNN needs consistent scaling with trained model
  cfg["scale"] = 1.0/4000;

  // offset for frame to eigen for DNN input
  cfg["offset"] = 0.0;

  cfg["cbeg"] = 800;
  cfg["cend"]  = 1600;
  cfg["tick0"]  = 0;
  cfg["nticks"] = 6000;

  // TorchScript model
  cfg["model"] = "model.pt";
  cfg["gpu"] = true;

  // taces used as input
  cfg["intags"] = Json::arrayValue;

  // tick/slice
  cfg["tick_per_slice"] = 10;
  
  // decon charge to fill in ROIs
  cfg["decon_charge_tag"] = "decon_charge0";

  // evaluation output
  cfg["evalfile"] = "tsmodel-eval.h5";

  // output trace tag
  cfg["outtag"] = "dnn_sp";

  return cfg;
}

namespace {
  Array::array_xxf downsample(const Array::array_xxf &in, const unsigned int k) {
    Array::array_xxf out = Array::array_xxf::Zero(in.rows(), in.cols()/k);
    for(unsigned int i=0; i<in.cols(); ++i) {
      out.col(i/k) = out.col(i/k) + in.col(i);
    }
    return out/k;
  }

  Array::array_xxf upsample(
    const Array::array_xxf &in,
    const unsigned int k,
    const int dim = 0) {
    if(dim==0) {
      Array::array_xxf out = Array::array_xxf::Zero(in.rows()*k, in.cols());
      for(unsigned int i=0; i<in.rows()*k; ++i) {
        out.row(i) = in.row(i/k);
      }
      return out;
    }
    if(dim==1) {
      Array::array_xxf out = Array::array_xxf::Zero(in.rows(), in.cols()*k);
      for(unsigned int i=0; i<in.cols()*k; ++i) {
        out.col(i) = in.col(i/k);
      }
      return out;
    }
  }

  Array::array_xxf mask(const Array::array_xxf &in, const Array::array_xxf &mask, const float th = 0.5) {
    Array::array_xxf ret = Eigen::ArrayXXf::Zero(in.rows(),in.cols());
    if(in.rows()!=mask.rows() || in.cols()!=mask.cols()) {
      std::cout << "error: in.rows()!=mask.rows() || in.cols()!=mask.cols()\n";
      return ret;
    }
    return (mask>th).select(in, ret);
  }

  Array::array_xxf baseline_subtraction(
    const Array::array_xxf &in
  ) {
    Array::array_xxf ret = Eigen::ArrayXXf::Zero(in.rows(),in.cols());
    for(int ich=0; ich<in.cols(); ++ich) {
      int sta = 0;
      int end = 0;
      for(int it=0; it<in.rows(); ++it) {
        if(in(it,ich) == 0) {
          if(sta < end){
            for(int i=sta;i<end+1;++i) {
              ret(i,ich) = in(i,ich)-(in(sta,ich)+(i-sta)*(in(end,ich)-in(sta,ich))/(end-sta));
            }
          }
          sta = it+1; // first tick in ROI
        } else {
          end = it; // last tick in ROI
        }
      }
    }
    return ret;
  }

  // used in sparsifying below.  Could use C++17 lambdas....
  bool ispositive(float x) { return x > 0.0; }
  bool isZero(float x) { return x == 0.0; }

  void eigen_to_traces(
    const Array::array_xxf& data
    , ITrace::vector& itraces
    , IFrame::trace_list_t& indices
    , const int cbeg
    , const int nticks
    , const bool sparse = false
    )
  {
    // reuse this temporary vector to hold charge for a channel.
    ITrace::ChargeSequence charge(nticks, 0.0);

    for (int ch = cbeg; ch < cbeg+data.cols(); ++ch) {
      for (int itick=0;itick!=nticks;itick++){
        const float q = data(itick,ch-cbeg);
        charge.at(itick) = q;
      }

      // actually save out
      if (sparse) {
        // Save waveform sparsely by finding contiguous, positive samples.
        std::vector<float>::const_iterator beg=charge.begin(), end=charge.end();
        auto i1 = std::find_if(beg, end, ispositive); // first start
        while (i1 != end) {
          // stop at next zero or end and make little temp vector
          auto i2 = std::find_if(i1, end, isZero);
          const std::vector<float> q(i1,i2);

          // save out
          const int tbin = i1 - beg;
          SimpleTrace *trace = new SimpleTrace(ch, tbin, q);
          const size_t trace_index = itraces.size();
          indices.push_back(trace_index);
          itraces.push_back(ITrace::pointer(trace));

          // find start for next loop
          i1 = std::find_if(i2, end, ispositive);
        }
      }
      else {
        // Save the waveform densely, including zeros.
        SimpleTrace *trace = new SimpleTrace(ch, 0, charge);
        const size_t trace_index = itraces.size();
        indices.push_back(trace_index);
        itraces.push_back(ITrace::pointer(trace));
      }
    }
  }

  Array::array_xxf frame_to_eigen(
    const IFrame::pointer & inframe
    , const std::string & tag
    , const IAnodePlane::pointer anode
    , const float scale = 1.0
    , const float offset = 0
    , const int win_cbeg = 0
    , const int win_cend = 800
    , const int tick0 = 0
    , const int nticks = 6000
  ) {
    const int tbeg = tick0;
    const int tend = tick0+nticks-1;
    auto channels = anode->channels();
    const int cbeg = channels.front()+win_cbeg;
    const int cend = channels.front()+win_cend-1;      

    const size_t ncols = nticks;
    const size_t nrows = cend-cbeg+1;
    Array::array_xxf arr = Array::array_xxf::Zero(nrows, ncols);

    auto traces = FrameTools::tagged_traces(inframe, tag);
    if (traces.empty()) {
      return arr;
    }

    FrameTools::fill(arr, traces, channels.begin()+win_cbeg, channels.begin()+win_cend, tick0);
    arr = arr * scale + offset;

    return arr;
  }
}

bool Pytorch::DNNROIFinding::operator()(const IFrame::pointer &inframe,
                                      IFrame::pointer &outframe) {

  outframe = inframe;
  if (!inframe) {
      l->debug("DNNROIFinding: EOS");
      outframe = nullptr;
      return true;
  }

  if (m_cfg["intags"].size() != 3) {
    return true;
  }

  h5::fd_t fd = h5::open(m_cfg["evalfile"].asString(), H5F_ACC_RDWR);
  std::clock_t start;
  double duration = 0;

  // frame to eigen
  const unsigned int tick_per_slice = m_cfg["tick_per_slice"].asInt();
  start = std::clock();
  duration = 0;
  std::vector<Array::array_xxf> ch_eigen;
  for (auto jtag : m_cfg["intags"]) {
    const std::string tag = jtag.asString();
    ch_eigen.push_back(
      downsample(
        frame_to_eigen(inframe, tag, m_anode,
        m_cfg["scale"].asFloat(), m_cfg["offset"].asFloat(),
        m_cfg["cbeg"].asInt(), m_cfg["cend"].asInt(),
        m_cfg["tick0"].asInt(), m_cfg["nticks"].asInt())
        , tick_per_slice));
  }
  duration += (std::clock() - start) / (double)CLOCKS_PER_SEC;
  l->info("frame2eigen: {}", duration);
  m_timers["frame2eigen"] += duration;

  start = std::clock();
  duration = 0;
  // eigen to tensor
  torch::Tensor ch[3];
  for(unsigned int i=0; i<3; ++i) {
    ch[i] = torch::from_blob(ch_eigen[i].data(), {ch_eigen[i].cols(),ch_eigen[i].rows()});
    // const int ncols = ch_eigen[i].cols();
    // const int nrows = ch_eigen[i].rows();
    // std::cout << "ncols: " << ncols << "nrows: " << nrows << std::endl;
    // h5::write<float>(fd, String::format("/%d/frame_%s%d%d", m_save_count, "ch", i, 0), ch_eigen[i].data(), h5::count{ncols, nrows}, h5::chunk{ncols, nrows} | h5::gzip{2});
  }
  auto img = torch::stack({ch[0], ch[1], ch[2]}, 0);
  auto batch = torch::stack({torch::transpose(img,1,2)}, 0);

  // std::cout << "batch.shape: {"
  // << batch.size(0) << ", "
  // << batch.size(1) << ", "
  // << batch.size(2) << ", "
  // << batch.size(3) << "} "
  // << std::endl;

  // Create a vector of inputs.
  std::vector<torch::jit::IValue> inputs;
  if(m_cfg["gpu"].asBool()) inputs.push_back(batch.cuda());
  else inputs.push_back(batch);

  duration += (std::clock() - start) / (double)CLOCKS_PER_SEC;
  l->info("eigen2tensor: {}", duration);
  m_timers["eigen2tensor"] += duration;
  
  start = std::clock();
  duration = 0;
  // Execute the model and turn its output into a tensor.
  torch::Tensor output = module.forward(inputs).toTensor().cpu();

  duration += (std::clock() - start) / (double)CLOCKS_PER_SEC;
  l->info("forward: {}", duration);
  m_timers["forward"] += duration;

  // std::cout << "output.shape: {"
  // << output.size(0) << ", "
  // << output.size(1) << ", "
  // << output.size(2) << ", "
  // << output.size(3) << "} "
  // << std::endl;
  
  start = std::clock();
  duration = 0;
  // tensor to eigen
  Eigen::Map<Eigen::ArrayXXf> out_e(output[0][0].data<float>(), output.size(3), output.size(2));
  auto mask_e = upsample(out_e, tick_per_slice);

  duration += (std::clock() - start) / (double)CLOCKS_PER_SEC;
  l->info("tensor2eigen: {}", duration);
  m_timers["tensor2eigen"] += duration;

  // decon charge frame to eigen
  Array::array_xxf decon_charge_eigen = frame_to_eigen(inframe, m_cfg["decon_charge_tag"].asString(), m_anode,
  1., 0.,
  m_cfg["cbeg"].asInt(), m_cfg["cend"].asInt(),
  m_cfg["tick0"].asInt(), m_cfg["nticks"].asInt());
  l->info("decon_charge_eigen: ncols: {} nrows: {}", decon_charge_eigen.cols(), decon_charge_eigen.rows()); // c600 x r800

  // apply ROI
  auto sp_charge = mask(decon_charge_eigen.transpose(), mask_e, 0.7);
  sp_charge = baseline_subtraction(sp_charge);
  
  start = std::clock();
  duration = 0;
  // hdf5 eval
  {
    const int ncols = mask_e.cols();
    const int nrows = mask_e.rows();
    l->info("ncols: {} nrows: {}", ncols, nrows);
    std::string aname = String::format("/%d/frame_%s%d", m_save_count, "dlroi", 0);
    h5::write<float>(fd, aname, mask_e.data(), h5::count{ncols, nrows}, h5::chunk{ncols, nrows} | h5::gzip{2});

    aname = String::format("/%d/frame_%s%d", m_save_count, "dlcharge", 0);
    h5::write<float>(fd, aname, sp_charge.data(), h5::count{ncols, nrows}, h5::chunk{ncols, nrows} | h5::gzip{2});
  }
  duration += (std::clock() - start) / (double)CLOCKS_PER_SEC;
  l->info("h5eval: {}", duration);
  m_timers["h5eval"] += duration;

  //eigen to frame
  start = std::clock();
  duration = 0;
  
  ITrace::vector* itraces = new ITrace::vector;
  IFrame::trace_list_t trace_index;
  eigen_to_traces(sp_charge, *itraces, trace_index, m_cfg["cbeg"].asInt(), m_cfg["nticks"].asInt(), false);

  SimpleFrame* sframe = new SimpleFrame(inframe->ident(), inframe->time(),
                                        ITrace::shared_vector(itraces),
                                        inframe->tick(), inframe->masks());
  sframe->tag_frame("DNNROIFinding");
  sframe->tag_traces(m_cfg["outtag"].asString(), trace_index);

  l->info("DNNROIFinding: produce {} traces: {}",
             itraces->size(), trace_index.size());

  outframe = IFrame::pointer(sframe);
  
  duration += (std::clock() - start) / (double)CLOCKS_PER_SEC;
  l->info("eigen2frame: {}", duration);
  m_timers["eigen2frame"] += duration;
  
  l->info("timer summary:");
  for (auto pair : m_timers) {
    l->info("{} : {}",pair.first, pair.second);
  }

  ++m_save_count;
  return true;
}

// Local Variables:
// mode: c++
// c-basic-offset: 2
// End:

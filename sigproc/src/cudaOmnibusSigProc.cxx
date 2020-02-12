#include "WireCellSigProc/cudaOmnibusSigProc.h"

#include "WireCellUtil/NamedFactory.h"
#include "WireCellUtil/Exceptions.h"
#include "WireCellUtil/String.h"
#include "WireCellUtil/FFTBestLength.h"

#include "WireCellIface/SimpleFrame.h"
#include "WireCellIface/SimpleTrace.h"

#include "WireCellIface/IFieldResponse.h"
#include "WireCellIface/IFilterWaveform.h"
#include "WireCellIface/IChannelResponse.h"

#include "ROI_formation.h"
#include "ROI_refinement.h"

#include "WireCellUtil/NamedFactory.h"

//#include "../../cuda/inc/WireCellCuda/cudaOmnibusSigProc.h"

#include <Eigen/Dense>
#include <iostream>
#include <omp.h>
#include <complex>

using namespace std;

WIRECELL_FACTORY(cudaOmnibusSigProc, WireCell::SigProc::cudaOmnibusSigProc,
                 WireCell::IFrameFilter, WireCell::IConfigurable)


using namespace WireCell;

using namespace WireCell::SigProc;


double g_restore_baseline = 0.0;
double g_decon_2D_tightROI = 0.0;
//double g_decon_2D_charge = 0.0;

cudaOmnibusSigProc::cudaOmnibusSigProc(const std::string& anode_tn,
                               const std::string& per_chan_resp_tn,
                               const std::string& field_response,
                               double fine_time_offset,
                               double coarse_time_offset,
                               double gain, 
                               double shaping_time,
                               double inter_gain , 
                               double ADC_mV,
                               float th_factor_ind,
                               float th_factor_col,
                               int pad,
                               float asy,
                               int rebin,
                               double l_factor,
                               double l_max_th,
                               double l_factor1,
                               int l_short_length,
			       int l_jump_one_bin,
                               double r_th_factor ,
                               double r_fake_signal_low_th ,
                               double r_fake_signal_high_th,
                               double r_fake_signal_low_th_ind_factor,
                               double r_fake_signal_high_th_ind_factor,
                               int r_pad ,
                               int r_break_roi_loop ,
                               double r_th_peak ,
                               double r_sep_peak,
                               double r_low_peak_sep_threshold_pre ,
                               int r_max_npeaks ,
                               double r_sigma ,
                               double r_th_percent ,
                               int charge_ch_offset,
                               const std::string& wiener_tag,
                               const std::string& wiener_threshold_tag,
                               const std::string& gauss_tag,
                               bool use_roi_debug_mode,
                               const std::string& tight_lf_tag,
                               const std::string& loose_lf_tag,
                               const std::string& cleanup_roi_tag,
                               const std::string& break_roi_loop1_tag,
                               const std::string& break_roi_loop2_tag,
                               const std::string& shrink_roi_tag,
                               const std::string& extend_roi_tag )
  : m_anode_tn (anode_tn)
  , m_per_chan_resp(per_chan_resp_tn)
  , m_field_response(field_response)
  , m_fine_time_offset(fine_time_offset)
  , m_coarse_time_offset(coarse_time_offset)
  , m_period(0)
  , m_nticks(0)
  , m_fft_flag(0)
  , m_gain(gain)
  , m_shaping_time(shaping_time)
  , m_inter_gain(inter_gain)
  , m_ADC_mV(ADC_mV)
  , m_th_factor_ind(th_factor_ind)
  , m_th_factor_col(th_factor_col)
  , m_pad(pad)
  , m_asy(asy)
  , m_rebin(rebin)
  , m_l_factor(l_factor)
  , m_l_max_th(l_max_th)
  , m_l_factor1(l_factor1)
  , m_l_short_length(l_short_length)
  , m_l_jump_one_bin(l_jump_one_bin)
  , m_r_th_factor(r_th_factor)
  , m_r_fake_signal_low_th(r_fake_signal_low_th)
  , m_r_fake_signal_high_th(r_fake_signal_high_th)
  , m_r_fake_signal_low_th_ind_factor(r_fake_signal_low_th_ind_factor)
  , m_r_fake_signal_high_th_ind_factor(r_fake_signal_high_th_ind_factor)
  , m_r_pad(r_pad)
  , m_r_break_roi_loop(r_break_roi_loop)
  , m_r_th_peak(r_th_peak)
  , m_r_sep_peak(r_sep_peak)
  , m_r_low_peak_sep_threshold_pre(r_low_peak_sep_threshold_pre)
  , m_r_max_npeaks(r_max_npeaks)
  , m_r_sigma(r_sigma)
  , m_r_th_percent(r_th_percent)
  , m_charge_ch_offset(charge_ch_offset)
  , m_wiener_tag(wiener_tag)
  , m_wiener_threshold_tag(wiener_threshold_tag)
  , m_gauss_tag(gauss_tag) 
  , m_frame_tag("sigproc")
  , m_use_roi_debug_mode(use_roi_debug_mode)
  , m_tight_lf_tag(tight_lf_tag)
  , m_loose_lf_tag(loose_lf_tag)
  , m_cleanup_roi_tag(cleanup_roi_tag)
  , m_break_roi_loop1_tag(break_roi_loop1_tag)
  , m_break_roi_loop2_tag(break_roi_loop2_tag)
  , m_shrink_roi_tag(shrink_roi_tag)
  , m_extend_roi_tag(extend_roi_tag)
  , m_sparse(false)
  , log(Log::logger("sigproc"))
{
  // get wires for each plane

 
  //std::cout << m_anode->channels().size() << " " << nwire_u << " " << nwire_v << " " << nwire_w << std::endl;

#ifdef HAVE_CUDA_H
  init_cudaOmnibusSigProc_CUDA();
#endif
  
}

cudaOmnibusSigProc::~cudaOmnibusSigProc()
{
#ifdef HAVE_CUDA_H
  clean_cudaOmnibusSigProc_CUDA();
#endif
}

std::string WireCell::SigProc::cudaOmnibusSigProc::OspChan::str() const
{
  std::stringstream ss;
  ss<<"OspChan<c:"<<channel<<",w:"<<wire<<",p:"<<plane<<",i:"<<ident<<">";
  return ss.str();
}


void cudaOmnibusSigProc::configure(const WireCell::Configuration& config)
{
  m_sparse = get(config, "sparse", false);

  m_fine_time_offset = get(config,"ftoffset",m_fine_time_offset);
  m_coarse_time_offset = get(config,"ctoffset",m_coarse_time_offset);
  m_anode_tn = get(config, "anode", m_anode_tn);

  //m_nticks = get(config,"nticks",m_nticks);
  if (! config["nticks"].isNull() ) {
    log->warn("cudaOmnibusSigProc has not setting \"nticks\", ignoring value {}", config["nticks"].asInt());
  }
  //m_period = get(config,"period",m_period);
  if (! config["period"].isNull() ) {
    log->warn("cudaOmnibusSigProc has not setting \"period\", ignoring value {}", config["period"].asDouble());
  }

  m_fft_flag = get(config,"fft_flag",m_fft_flag);
  
  m_gain = get(config,"gain",m_gain);
  m_shaping_time = get(config,"shaping",m_shaping_time);
  m_inter_gain = get(config,"postgain", m_inter_gain);
  m_ADC_mV = get(config,"ADC_mV", m_ADC_mV);

  m_per_chan_resp = get(config, "per_chan_resp", m_per_chan_resp);
  m_field_response = get(config, "field_response", m_field_response);

  m_th_factor_ind = get(config,"troi_ind_th_factor",m_th_factor_ind);
  m_th_factor_col = get(config,"troi_col_th_factor",m_th_factor_col);
  m_pad = get(config,"troi_pad",m_pad);
  m_asy = get(config,"troi_asy",m_asy);
  m_rebin = get(config,"lroi_rebin",m_rebin);
  m_l_factor = get(config,"lroi_th_factor",m_l_factor);
  m_l_max_th = get(config,"lroi_max_th",m_l_max_th);
  m_l_factor1 = get(config,"lroi_th_factor1",m_l_factor1);
  m_l_short_length = get(config,"lroi_short_length",m_l_short_length);
  m_l_jump_one_bin = get(config,"lroi_jump_one_bin",m_l_jump_one_bin);

  m_r_th_factor = get(config,"r_th_factor",m_r_th_factor);
  m_r_fake_signal_low_th = get(config,"r_fake_signal_low_th",m_r_fake_signal_low_th);
  m_r_fake_signal_high_th = get(config,"r_fake_signal_high_th",m_r_fake_signal_high_th);
  m_r_fake_signal_low_th_ind_factor = get(config,"r_fake_signal_low_th_ind_factor",m_r_fake_signal_low_th_ind_factor);
  m_r_fake_signal_high_th_ind_factor = get(config,"r_fake_signal_high_th_ind_factor",m_r_fake_signal_high_th_ind_factor);
  m_r_pad = get(config,"r_pad",m_r_pad);
  m_r_break_roi_loop = get(config,"r_break_roi_loop",m_r_break_roi_loop);
  m_r_th_peak = get(config,"r_th_peak",m_r_th_peak);
  m_r_sep_peak = get(config,"r_sep_peak",m_r_sep_peak);
  m_r_low_peak_sep_threshold_pre = get(config,"r_low_peak_sep_threshold_pre",m_r_low_peak_sep_threshold_pre);
  m_r_max_npeaks = get(config,"r_max_npeaks",m_r_max_npeaks);
  m_r_sigma = get(config,"r_sigma",m_r_sigma);
  m_r_th_percent = get(config,"r_th_percent",m_r_th_percent);

  m_charge_ch_offset = get(config,"charge_ch_offset",m_charge_ch_offset);
  
  m_wiener_tag = get(config,"wiener_tag",m_wiener_tag);
  m_wiener_threshold_tag = get(config,"wiener_threshold_tag",m_wiener_threshold_tag);
  m_gauss_tag = get(config,"gauss_tag",m_gauss_tag);  
  m_frame_tag = get(config,"frame_tag",m_frame_tag);  

  m_use_roi_debug_mode = get(config,"use_roi_debug_mode",m_use_roi_debug_mode);
  m_tight_lf_tag = get(config,"tight_lf_tag",m_tight_lf_tag);
  m_loose_lf_tag = get(config,"loose_lf_tag",m_loose_lf_tag);
  m_cleanup_roi_tag = get(config,"cleanup_roi_tag",m_cleanup_roi_tag);
  m_break_roi_loop1_tag = get(config,"break_roi_loop1_tag",m_break_roi_loop1_tag);
  m_break_roi_loop2_tag = get(config,"break_roi_loop2_tag",m_break_roi_loop2_tag);
  m_shrink_roi_tag = get(config,"shrink_roi_tag",m_shrink_roi_tag);
  m_extend_roi_tag = get(config,"extend_roi_tag",m_extend_roi_tag);

  // this throws if not found
  m_anode = Factory::find_tn<IAnodePlane>(m_anode_tn);

  // Build up the channel map.  The OSP channel must run contiguously
  // first up the U, then V, then W "wires".  Ie, face-major order,
  // but we have plane-major order so make a temporary collection.
  IChannel::vector plane_channels[3];
  std::stringstream ss;
  ss << "OmnibusSigproc: internal channel map for tags: gauss:\""
     << m_gauss_tag << "\", wiener:\"" << m_wiener_tag << "\", frame:\"" << m_frame_tag << "\"\n";

  for (auto face : m_anode->faces()) {
    if (!face) { // A null face means one sided AnodePlane.  
      continue;  // Can be "back" or "front" face.
    }
    for (auto plane: face->planes()) {
      int plane_index = plane->planeid().index();
      auto& pchans = plane_channels[plane_index];
      // These IChannel vectors are ordered in same order as wire-in-plane.
      const auto& ichans = plane->channels();
      // Append
      pchans.reserve(pchans.size() + ichans.size());
      pchans.insert(pchans.end(), ichans.begin(), ichans.end());
      ss << "\tpind" << plane_index << " "
         << "aid" << m_anode->ident() << " "
         << "fid" << face->ident() << " "
         << "pid" << plane->ident() << " "
         << "cid" << ichans.front()->ident() << " -> cid" << ichans.back()->ident() << ", "
         << "cind" << ichans.front()->index() << " -> cind" << ichans.back()->index() << ", "
         << "(n="<<pchans.size()<<")\n";
    }
  }
  log->debug(ss.str());
  
  int osp_channel_number = 0;
  for (int iplane = 0; iplane < 3; ++iplane) {
    m_nwires[iplane] = plane_channels[iplane].size();
    int osp_wire_number = 0;
    for (auto ichan : plane_channels[iplane]) {
      const int wct_chan_ident = ichan->ident();
      OspChan och(osp_channel_number, osp_wire_number, iplane, wct_chan_ident);
      m_channel_map[wct_chan_ident] = och; // we could save some space by storing
      m_channel_range[iplane].push_back(och);// wct ident here instead of a whole och.
      ++osp_wire_number;
      ++osp_channel_number;
    }
  }
  m_timers.insert({"restore_baseline",0});
  m_timers.insert({"restore_baseline_cuda",0});
}

WireCell::Configuration cudaOmnibusSigProc::default_configuration() const
{
  Configuration cfg;
  cfg["anode"] = m_anode_tn;
  cfg["ftoffset"] = m_fine_time_offset;
  cfg["ctoffset"] = m_coarse_time_offset;
  //cfg["nticks"] = m_nticks;
  //cfg["period"] = m_period;

  cfg["fft_flag"] = m_fft_flag;
  
  cfg["gain"] = m_gain;
  cfg["shaping"] = m_shaping_time;
  cfg["inter_gain"] = m_inter_gain;
  cfg["ADC_mV"] = m_ADC_mV;

  cfg["per_chan_resp"] = m_per_chan_resp;
  cfg["field_response"] = m_field_response;

  cfg["troi_ind_th_factor"] = m_th_factor_ind;
  cfg["troi_col_th_factor"] = m_th_factor_col;
  cfg["troi_pad"] = m_pad;
  cfg["troi_asy"] = m_asy;
  cfg["lroi_rebin"] = m_rebin; 
  cfg["lroi_th_factor"] = m_l_factor;
  cfg["lroi_max_th"] = m_l_max_th;
  cfg["lroi_th_factor1"] = m_l_factor1;
  cfg["lroi_short_length"] = m_l_short_length; 
  cfg["lroi_jump_one_bin"] = m_l_jump_one_bin;
  
  cfg["r_th_factor"] = m_r_th_factor;
  cfg["r_fake_signal_low_th"] = m_r_fake_signal_low_th;
  cfg["r_fake_signal_high_th"] = m_r_fake_signal_high_th;
  cfg["r_fake_signal_low_th_ind_factor"] = m_r_fake_signal_low_th_ind_factor;
  cfg["r_fake_signal_high_th_ind_factor"] = m_r_fake_signal_high_th_ind_factor;
  cfg["r_pad"] = m_r_pad;
  cfg["r_break_roi_loop"] = m_r_break_roi_loop;
  cfg["r_th_peak"] = m_r_th_peak;
  cfg["r_sep_peak"] = m_r_sep_peak;
  cfg["r_low_peak_sep_threshold_pre"] = m_r_low_peak_sep_threshold_pre;
  cfg["r_max_npeaks"] = m_r_max_npeaks;
  cfg["r_sigma"] = m_r_sigma;
  cfg["r_th_precent"] = m_r_th_percent;
      
  // fixme: unused?
  cfg["charge_ch_offset"] = m_charge_ch_offset;

  cfg["wiener_tag"] = m_wiener_tag;
  cfg["wiener_threshold_tag"] = m_wiener_threshold_tag;
  cfg["gauss_tag"] = m_gauss_tag;
  cfg["frame_tag"] = m_frame_tag;

  cfg["use_roi_debug_mode"] = m_use_roi_debug_mode; // default false
  cfg["tight_lf_tag"] = m_tight_lf_tag;
  cfg["loose_lf_tag"] = m_loose_lf_tag;
  cfg["cleanup_roi_tag"] = m_cleanup_roi_tag;
  cfg["break_roi_loop1_tag"] = m_break_roi_loop1_tag;
  cfg["break_roi_loop2_tag"] = m_break_roi_loop2_tag;
  cfg["shrink_roi_tag"] = m_shrink_roi_tag;
  cfg["extend_roi_tag"] = m_extend_roi_tag;
  
  cfg["sparse"] = false;

  return cfg;
  
}

void cudaOmnibusSigProc::load_data(const input_pointer& in, int plane){

  m_r_data = Array::array_xxf::Zero(m_fft_nwires[plane],m_fft_nticks);

  auto traces = in->traces();

  auto& bad = m_cmm["bad"];
  int nbad = 0;

  for (auto trace : *traces.get()) {
    int wct_channel_ident = trace->channel();
    OspChan och = m_channel_map[wct_channel_ident];
    if (plane != och.plane) {
      continue;         // we'll catch it in another call to load_data
    }

    // fixme: this code uses tbin() but other places in this file will barf if tbin!=0.
    int tbin = trace->tbin();
    auto const& charges = trace->charge();
    const int ntbins = std::min((int)charges.size(), m_nticks);
    for (int qind = 0; qind < ntbins; ++qind) {
      m_r_data(och.wire + m_pad_nwires[plane], tbin + qind) = charges[qind];
    }

    //ensure dead channels are indeed dead ...
    auto const& badch = bad.find(och.channel);
    if (badch == bad.end()) {
      continue;
    }

    auto const& binranges = badch->second;
    for (auto const& br : binranges) {
      ++nbad;
      for (int i = br.first; i != br.second; ++i) {
        m_r_data(och.wire+m_pad_nwires[plane], i) = 0;
      }
    }

  }
  log->debug("cudaOmnibusSigProc: plane index: {} input data identifies {} bad regions",
             plane, nbad);
  
}

// used in sparsifying below.  Could use C++17 lambdas....
static bool ispositive(float x) { return x > 0.0; }
static bool isZero(float x) { return x == 0.0; }

void cudaOmnibusSigProc::save_data(ITrace::vector& itraces, IFrame::trace_list_t& indices, int plane,
                               const std::vector<float>& perwire_rmses,
                               IFrame::trace_summary_t& threshold)
{
  // reuse this temporary vector to hold charge for a channel.
  ITrace::ChargeSequence charge(m_nticks, 0.0);

  double wstart, wend;

  wstart = omp_get_wtime();

  double qtot = 0.0;
  for (auto och : m_channel_range[plane]) { // ordered by osp channel
    
    // Post process: zero out any negative signal and that from "bad" channels.
    // fixme: better if we move this outside of save_data().
    for (int itick=0;itick!=m_nticks;itick++){
      const float q = m_r_data(och.wire, itick);
      // charge.at(itick) = q > 0.0 ? q : 0.0;
      //charge.at(itick) = q ;
      if (m_use_roi_debug_mode) charge.at(itick) = q ; // debug mode: save all decons
      else charge.at(itick) = q > 0.0 ? q : 0.0; // default mode: only save positive
    }
    {
      auto& bad = m_cmm["bad"];
      auto badit = bad.find(och.channel);
      if (badit != bad.end()) {
        for (auto bad : badit->second) {
          for (int itick=bad.first; itick < bad.second; ++itick) {
            charge.at(itick) = 0.0;
          }
        }
      }
    }
    

    // debug
    for (int j=0;j!=m_nticks;j++){
      qtot += charge.at(j);
    }

    const float thresh = perwire_rmses[och.wire];

    // actually save out
    if (m_sparse) {
      // Save waveform sparsely by finding contiguous, positive samples.
      std::vector<float>::const_iterator beg=charge.begin(), end=charge.end();
      auto i1 = std::find_if(beg, end, ispositive); // first start
      while (i1 != end) {
        // stop at next zero or end and make little temp vector
        auto i2 = std::find_if(i1, end, isZero);
        const std::vector<float> q(i1,i2);

        // save out
        const int tbin = i1 - beg;
        SimpleTrace *trace = new SimpleTrace(och.ident, tbin, q);
        const size_t trace_index = itraces.size();
        indices.push_back(trace_index);
        itraces.push_back(ITrace::pointer(trace));
        threshold.push_back(thresh);

        // find start for next loop
        i1 = std::find_if(i2, end, ispositive);
      }
    }
    else {
      // Save the waveform densely, including zeros.
      SimpleTrace *trace = new SimpleTrace(och.ident, 0, charge);
      const size_t trace_index = itraces.size();
      indices.push_back(trace_index);
      itraces.push_back(ITrace::pointer(trace));
      threshold.push_back(thresh);
    }
  }
  wend = omp_get_wtime();
  if(plane == 1) cout << "cudaOmnibusSigProc::save_data() time : " << wend - wstart << endl;


  // debug
  if (indices.empty()) {
    log->debug("cudaOmnibusSigProc::save_data plane index: {} empty", plane);
  }
  else {
    const int nadded = indices.back() - indices.front() + 1;
    log->debug("cudaOmnibusSigProc::save_data plane index: Qtot={} added {} traces to total {} indices:[{},{}]",
               plane, qtot, nadded, indices.size(), indices.front(), indices.back());
  }  
}

// save ROI into the out frame
void cudaOmnibusSigProc::save_roi(ITrace::vector& itraces, IFrame::trace_list_t& indices,
                              int plane, std::vector<std::list<SignalROI*> >& roi_channel_list)
{
  // reuse this temporary vector to hold charge for a channel.
  ITrace::ChargeSequence charge(m_nticks, 0.0);

   for (auto och : m_channel_range[plane]) { // ordered by osp channel

     // std::cout << "[wgu] wire: " << och.wire << " roi_channel_list.size(): " << roi_channel_list.size() << std::endl;

     std::fill(charge.begin(), charge.end(), 0);

     // for (auto it = roi_channel_list.at(och.wire).begin(); it!= roi_channel_list.at(och.wire).end(); it++){
     //   SignalROI *roi =  *it;
     //   int start = roi->get_start_bin();
     //   int end = roi->get_end_bin();
     //   std::cout << "[wgu] wire: " << och.wire << " ROI: " << start << " " << end << " channel: " << roi->get_chid() << " plane: " << roi->get_plane() << std::endl;      
     // }

     int prev_roi_end = -1;
     for (auto signal_roi: roi_channel_list.at(och.wire) ) {
         int start = signal_roi->get_start_bin();
         int end = signal_roi->get_end_bin();
         // if (och.wire==732) 
        //   std::cout << "[wgu] cudaOmnibusSigProc::save_roi() wire: " << och.wire << " channel: " << och.channel << " ROI: " << start << " " << end << " channel: " << signal_roi->get_chid() << " plane: " << signal_roi->get_plane() << " max height: " << signal_roi->get_max_height() <<  std::endl;
         if (start<0 or end<0) continue;
         for (int i=start; i<=end; i++) {
           if (i-prev_roi_end<2) continue; // skip one bin for visibility of two adjacent ROIs
           charge.at(i) = 10.; // arbitary constant number for ROI display
         }
         prev_roi_end = end;
     }

     {
      auto& bad = m_cmm["bad"];
      auto badit = bad.find(och.channel);
      if (badit != bad.end()) {
        for (auto bad : badit->second) {
          for (int itick=bad.first; itick < bad.second; ++itick) {
            charge.at(itick) = 0.0;
          }
        }
      }
     }


     // actually save out
    if (m_sparse) {
      // Save waveform sparsely by finding contiguous, positive samples.
      std::vector<float>::const_iterator beg=charge.begin(), end=charge.end();
      auto i1 = std::find_if(beg, end, ispositive); // first start
      while (i1 != end) {
        // stop at next zero or end and make little temp vector
        auto i2 = std::find_if(i1, end, isZero);
        const std::vector<float> q(i1,i2);

         // save out
        const int tbin = i1 - beg;
        SimpleTrace *trace = new SimpleTrace(och.ident, tbin, q);
        const size_t trace_index = itraces.size();
        indices.push_back(trace_index);
        itraces.push_back(ITrace::pointer(trace));
        // if (och.wire==67) std::cout << "[wgu] och channel: " << och.channel << " wire: " << och.wire << " plane: " << och.plane << " ident: " << och.ident << " tbin: " << tbin << " len: " << i2-i1 << std::endl;

         // find start for next loop
        i1 = std::find_if(i2, end, ispositive);
      }
    }
    else {
      // Save the waveform densely, including zeros.
      SimpleTrace *trace = new SimpleTrace(och.ident, 0, charge);
      const size_t trace_index = itraces.size();
      indices.push_back(trace_index);
      itraces.push_back(ITrace::pointer(trace));
    }
  }
}

void cudaOmnibusSigProc::init_overall_response(IFrame::pointer frame)
{
  m_period = frame->tick();
  {
    std::vector<int> tbins;
    for (auto trace : *frame->traces()) {
      const int tbin = trace->tbin();
      const int nbins = trace->charge().size();
      tbins.push_back(tbin);
      tbins.push_back(tbin+nbins);
    }
    auto mme = std::minmax_element(tbins.begin(), tbins.end());
    int tbinmin = *mme.first;
    int tbinmax = *mme.second;
    m_nticks = tbinmax-tbinmin;
    log->debug("cudaOmnibusSigProc: nticks={} tbinmin={} tbinmax={}",
               m_nticks, tbinmin, tbinmax);

    if (m_fft_flag==0){
      m_fft_nticks = m_nticks;
    }else{
      m_fft_nticks = fft_best_length(m_nticks);
      log->debug("cudaOmnibusSigProc: enlarge window from {} to {}", m_nticks, m_fft_nticks);
    }
    //
    
    m_pad_nticks = m_fft_nticks - m_nticks;
  }

  auto ifr = Factory::find_tn<IFieldResponse>(m_field_response);
  // Get full, "fine-grained" field responses defined at impact
  // positions.
  Response::Schema::FieldResponse fr = ifr->field_response();
  
  // Make a new data set which is the average FR
  Response::Schema::FieldResponse fravg = Response::wire_region_average(fr);
  
  for (int i=0;i!=3;i++){
    //
    if (m_fft_flag==0){
      m_fft_nwires[i] = m_nwires[i];
    }else{
      m_fft_nwires[i] = fft_best_length(m_nwires[i]+fravg.planes[0].paths.size()-1,1);
      log->debug("cudaOmnibusSigProc: enlarge wire number in plane {} from {} to {}",
                 i, m_nwires[i], m_fft_nwires[i]);
    }
    m_pad_nwires[i] = (m_fft_nwires[i]-m_nwires[i])/2;
  }
  
  // since we only do FFT along time, no need to change dimension for wire ...
  const size_t fine_nticks = fft_best_length(fravg.planes[0].paths[0].current.size());
  int fine_nwires = fravg.planes[0].paths.size();
  
  WireCell::Waveform::compseq_t elec;
  WireCell::Binning tbins(fine_nticks, 0, fine_nticks * fravg.period);
  Response::ColdElec ce(m_gain, m_shaping_time);
  auto ewave = ce.generate(tbins);
  Waveform::scale(ewave, m_inter_gain * m_ADC_mV  * (-1));
  elec = Waveform::dft(ewave);

  std::complex<float> fine_period(fravg.period,0);
  
  Waveform::realseq_t wfs(m_fft_nticks);
  Waveform::realseq_t ctbins(m_fft_nticks);
  for (int i=0;i!=m_fft_nticks;i++){
    ctbins.at(i) = i * m_period;
  }

  
  
  Waveform::realseq_t ftbins(fine_nticks);
  for (size_t i=0;i!=fine_nticks;i++){
    ftbins.at(i) = i * fravg.period;
  }

  // clear the overall response
  for (int i=0;i!=3;i++){
    overall_resp[i].clear();
  }

  m_intrinsic_time_offset = fr.origin/fr.speed;

  
  // Convert each average FR to a 2D array
  for (int iplane=0; iplane<3; ++iplane) {
    auto arr = Response::as_array(fravg.planes[iplane], fine_nwires, fine_nticks);

    // do FFT for response ... 
    Array::array_xxc c_data = Array::dft_rc(arr,0);
    int nrows = c_data.rows();
    int ncols = c_data.cols();

    for (int irow = 0; irow < nrows; ++irow){
      for (int icol = 0; icol < ncols; ++ icol){
	c_data(irow,icol) = c_data(irow,icol) * elec.at(icol) * fine_period;
      }
    }
    
    arr = Array::idft_cr(c_data,0);

	
    // figure out how to do fine ... shift (good ...) 
    int fine_time_shift = m_fine_time_offset / fravg.period;
    if (fine_time_shift>0){
      Array::array_xxf arr1(nrows,ncols - fine_time_shift);
      arr1 = arr.block(0,0,nrows,ncols - fine_time_shift);
      Array::array_xxf arr2(nrows,fine_time_shift);
      arr2 = arr.block(0,ncols-fine_time_shift,nrows,fine_time_shift);
      arr.block(0,0,nrows,fine_time_shift) = arr2;
      arr.block(0,fine_time_shift,nrows,ncols-fine_time_shift) = arr1;
      
      // Array::array_xxf arr1(nrows,fine_time_shift);
      // arr1 = arr.block(0,0,nrows,fine_time_shift);
      // Array::array_xxf arr2(nrows,ncols-fine_time_shift);
      // arr2 = arr.block(0,fine_time_shift,nrows,ncols-fine_time_shift);
      // arr.block(0,0,nrows,ncols-fine_time_shift) = arr2;
      // arr.block(0,ncols-fine_time_shift,nrows,fine_time_shift) = arr1;
    }
	
	
    // redigitize ... 
    for (int irow = 0; irow < fine_nwires; ++ irow){
      // gtemp = new TGraph();
      
      size_t fcount = 1;
      for (int i=0;i!=m_fft_nticks;i++){
	double ctime = ctbins.at(i);
	
	if (fcount < fine_nticks)
	  while(ctime > ftbins.at(fcount)){
	    fcount ++;
	    if (fcount >= fine_nticks) break;
	  }
	
	    
	if(fcount < fine_nticks){
	  wfs.at(i) = ((ctime - ftbins.at(fcount-1)) /fravg.period * arr(irow,fcount-1) + (ftbins.at(fcount)-ctime)/fravg.period * arr(irow,fcount)) ;// / (-1);
	}else{
	  wfs.at(i) = 0;
	}
      }
      
      overall_resp[iplane].push_back(wfs);

      //wfs.clear();
    } // loop inside wire ...

    
    // calculated the wire shift ...     
    m_wire_shift[iplane] = (int(overall_resp[iplane].size())-1)/2;


  }//  loop over plane

  
  
}

#ifdef HAVE_CUDA_H
void cudaOmnibusSigProc::restore_baseline_cuda(Array::array_xxf& arr){

  //double wstart, wend;
  //double wstart_p, wend_p;
  //double part1 = 0.0;
  //double part2 = 0.0;
  //double part3 = 0.0;

  

  //wstart = omp_get_wtime();
  
  //log->debug("cudaOmnibusSigProc::restore_baseline_cuda() : {}, {}", arr.rows(), arr.cols());
  //restore_baseline_CUDA(arr.data(), arr.rows(), arr.cols());
  //wend = omp_get_wtime();
  //g_restore_baseline += wend - wstart;
  //log->debug("cudaOmnibusSigProc::restore_baseline_cuda() : total running time : {}",  g_restore_baseline);
  //return;

  for (int i=0;i!=arr.rows();i++){
    //wstart_p = omp_get_wtime();
    Waveform::realseq_t signal(arr.cols());

    /*
    cout << "signal : ";
    for(int j=0 ; j<arr.cols() ; j++) {
      cout << arr(i,j) << ", ";
    }
    cout << endl;
    */

    int ncount = 0;
    for (int j=0;j!=arr.cols();j++){
      if (arr(i,j)!=0){
	signal.at(ncount) = arr(i,j);
	ncount ++;
      }
    }
    signal.resize(ncount);
    /*
    if(ncount < arr.cols()) {
      cout << "signal : ";
      for(int k=0 ; k<ncount ; k++) {
        cout << signal[k] << ", ";
      }
      cout << endl;
    }
    */
    //if(arr.cols() > ncount) log->debug("cudaOmnibusSigProc::restore_baseline_cuda() : ncount 1 : {}", ncount);
    float baseline = WireCell::Waveform::median(signal);
    //cout << "[i:"<<i<<"] basseline : " << baseline << endl;


    //wend_p = omp_get_wtime();
    //part1 += wend_p - wstart_p;
    //wstart_p = omp_get_wtime();

    Waveform::realseq_t temp_signal(arr.cols());
    ncount = 0;
    for (size_t j =0; j!=signal.size();j++){
      if (fabs(signal.at(j)-baseline) < 500){
	temp_signal.at(ncount) = signal.at(j);
	ncount ++;
      }
    }
    temp_signal.resize(ncount);
    //if(arr.cols() > ncount) log->debug("cudaOmnibusSigProc::restore_baseline_cuda() : ncount 2 : {}", ncount);
    
    baseline = WireCell::Waveform::median(temp_signal);
    //wend_p = omp_get_wtime();
    //part2 += wend_p - wstart_p;
    
    for (int j=0;j!=arr.cols();j++){
      if (arr(i,j)!=0)
	arr(i,j) -= baseline;
    }
    //part3 += omp_get_wtime() - wend_p;
  }

  //wend = omp_get_wtime();
  //g_restore_baseline += wend - wstart;
  //log->debug("cudaOmnibusSigProc::restore_baseline_cuda() : part1 : {}, part2 : {}, part3 : {},  total running time : {}", part1, part2, part3, g_restore_baseline);
}
#endif

void cudaOmnibusSigProc::restore_baseline(Array::array_xxf& arr){

  double wstart, wend;

  wstart = omp_get_wtime();
  
  for (int i=0;i!=arr.rows();i++){
    Waveform::realseq_t signal(arr.cols());
    int ncount = 0;
    for (int j=0;j!=arr.cols();j++){
      if (arr(i,j)!=0){
	signal.at(ncount) = arr(i,j);
	ncount ++;
      }
    }
    signal.resize(ncount);
    float baseline = WireCell::Waveform::median(signal);

    Waveform::realseq_t temp_signal(arr.cols());
    ncount = 0;
    for (size_t j =0; j!=signal.size();j++){
      if (fabs(signal.at(j)-baseline) < 500){
	temp_signal.at(ncount) = signal.at(j);
	ncount ++;
      }
    }
    temp_signal.resize(ncount);
    
    baseline = WireCell::Waveform::median(temp_signal);
    
    for (int j=0;j!=arr.cols();j++){
      if (arr(i,j)!=0)
	arr(i,j) -= baseline;
    }
  }
  wend = omp_get_wtime();
  g_restore_baseline += wend - wstart;
  m_timers["restore_baseline"] += wend - wstart;
  log->debug("cudaOmnibusSigProc::restore_baseline() : {}, {}, total running time : {}", arr.rows(), arr.cols(), g_restore_baseline);
}


void cudaOmnibusSigProc::decon_2D_init(int plane){

  //log->debug("cudaOmnibusSigProc::decon_2D_init()");
  // data part ... 
  // first round of FFT on time
  m_c_data = Array::dft_rc(m_r_data,0);

  
  // now apply the ch-by-ch response ...
  if (! m_per_chan_resp.empty()) {
    log->debug("cudaOmnibusSigProc: applying ch-by-ch electronics response correction");
    auto cr = Factory::find_tn<IChannelResponse>(m_per_chan_resp);
    auto cr_bins = cr->channel_response_binning();
    if (cr_bins.binsize() != m_period) {
      log->critical("cudaOmnibusSigProc::decon_2D_init: channel response size mismatch");
      THROW(ValueError() << errmsg{"cudaOmnibusSigProc::decon_2D_init: channel response size mismatch"});
    }
    //starndard electronics response ... 
    // WireCell::Binning tbins(m_nticks, 0-m_period/2., m_nticks*m_period-m_period/2.);
    // Response::ColdElec ce(m_gain, m_shaping_time);

    // temporary hack ...
    //float scaling = 1./(1e-9*0.5/1.13312);
    //WireCell::Binning tbins(m_nticks, (-5-0.5)*m_period, (m_nticks-5-0.5)*m_period-m_period);
    //Response::ColdElec ce(m_gain*scaling, m_shaping_time);
    //// this is moved into wirecell.sigproc.main production of
    //// microboone-channel-responses-v1.json.bz2
    WireCell::Binning tbins(m_fft_nticks, cr_bins.min(), cr_bins.min() + m_fft_nticks*m_period);
    Response::ColdElec ce(m_gain, m_shaping_time);
    
    const auto ewave = ce.generate(tbins);
    const WireCell::Waveform::compseq_t elec = Waveform::dft(ewave);

    for (auto och : m_channel_range[plane]) {
      //const auto& ch_resp = cr->channel_response(och.ident);
      Waveform::realseq_t tch_resp = cr->channel_response(och.ident);
      tch_resp.resize(m_fft_nticks,0);
      const WireCell::Waveform::compseq_t ch_elec = Waveform::dft(tch_resp);

      const int irow = och.wire+m_pad_nwires[plane];
      for (int icol = 0; icol != m_c_data.cols(); icol++){
        const auto four = ch_elec.at(icol);
	if (std::abs(four) != 0){
	  m_c_data(irow,icol) *= elec.at(icol) / four;
	}else{
	  m_c_data(irow,icol) = 0;
	}
      }
    }
  }


  
  
  //second round of FFT on wire
  m_c_data = Array::dft_cc(m_c_data,1);
  
  //response part ...
  Array::array_xxf r_resp = Array::array_xxf::Zero(m_r_data.rows(),m_fft_nticks);
  for (size_t i=0;i!=overall_resp[plane].size();i++){
    for (int j=0;j!=m_fft_nticks;j++){
      r_resp(i,j) = overall_resp[plane].at(i).at(j);
    }
  }
  
  // do first round FFT on the resposne on time
  Array::array_xxc c_resp = Array::dft_rc(r_resp,0);
  // do second round FFT on the response on wire
  c_resp = Array::dft_cc(c_resp,1);

  
  //make ratio to the response and apply wire filter
  m_c_data = m_c_data/c_resp;

  // apply software filter on wire
  const std::vector<std::string> filter_names{"Wire_ind", "Wire_ind", "Wire_col"};
  Waveform::realseq_t wire_filter_wf;
  auto ncr1 = Factory::find<IFilterWaveform>("HfFilter", filter_names[plane]);
  wire_filter_wf = ncr1->filter_waveform(m_c_data.rows());
  for (int irow=0; irow<m_c_data.rows(); ++irow) {
    for (int icol=0; icol<m_c_data.cols(); ++icol) {
      float val = abs(m_c_data(irow,icol));
      if (std::isnan(val)) {
	m_c_data(irow,icol) = -0.0;
      }
      if (std::isinf(val)) {
	m_c_data(irow,icol) = 0.0;
      }
      m_c_data(irow,icol) *= wire_filter_wf.at(irow);
    }
  }
  
  //do the first round of inverse FFT on wire
  m_c_data = Array::idft_cc(m_c_data,1);

  // do the second round of inverse FFT on time
  m_r_data = Array::idft_cr(m_c_data,0);

  // do the shift in wire 
  const int nrows = m_r_data.rows();
  const int ncols = m_r_data.cols();
  { 
    Array::array_xxf arr1(m_wire_shift[plane], ncols) ;
    arr1 = m_r_data.block(nrows-m_wire_shift[plane] , 0 , m_wire_shift[plane], ncols);
    Array::array_xxf arr2(nrows-m_wire_shift[plane],ncols);
    arr2 = m_r_data.block(0,0,nrows-m_wire_shift[plane],ncols);
    m_r_data.block(0,0,m_wire_shift[plane],ncols) = arr1;
    m_r_data.block(m_wire_shift[plane],0,nrows-m_wire_shift[plane],ncols) = arr2;
  }
  
  //do the shift in time
  int time_shift = (m_coarse_time_offset + m_intrinsic_time_offset)/m_period;
  if (time_shift > 0){
    Array::array_xxf arr1(nrows,ncols - time_shift);
    arr1 = m_r_data.block(0,0,nrows,ncols - time_shift);
    Array::array_xxf arr2(nrows,time_shift);
    arr2 = m_r_data.block(0,ncols-time_shift,nrows,time_shift);
    m_r_data.block(0,0,nrows,time_shift) = arr2;
    m_r_data.block(0,time_shift,nrows,ncols-time_shift) = arr1;
  }
  m_c_data = Array::dft_rc(m_r_data,0);

#ifdef HAVE_CUDA_H
#ifdef CUDA_TEST
  double wstart = omp_get_wtime(); 
  upload_c_data(m_c_data.data(), m_c_data.rows(), m_c_data.cols());
  double wend = omp_get_wtime();
  g_decon_2D_tightROI += wend - wstart;
  log->debug("decon_2D_init() : update_c_data() called : total running time : {}", g_decon_2D_tightROI);

#endif
#endif

}


void cudaOmnibusSigProc::decon_2D_ROI_refine(int plane){
   // apply software filter on time
  double wstart, wend;

  const std::vector<std::string> filter_names{"Wiener_tight_U", "Wiener_tight_V", "Wiener_tight_W"};
  Waveform::realseq_t roi_hf_filter_wf;

  auto ncr1 = Factory::find<IFilterWaveform>("HfFilter", filter_names[plane]);
  roi_hf_filter_wf = ncr1->filter_waveform(m_c_data.cols());


#ifdef HAVE_CUDA_H
#ifdef CUDA_TEST
   
  wstart = omp_get_wtime(); 
  decon_2D_tightROI_CUDA(m_c_data.rows(), m_c_data.cols(), roi_hf_filter_wf.data(), m_r_data.data(), m_pad_nwires[plane],0,m_nwires[plane],m_nticks);
  wend = omp_get_wtime();
  g_decon_2D_tightROI += wend - wstart;
  log->debug("cudaOmnibusSigProc::decon_2D_ROI_refine() : total running time : {}", g_decon_2D_tightROI);

  restore_baseline_cuda(m_r_data);
  return;
  
#endif
#endif


  wstart = omp_get_wtime(); 
  Array::array_xxc c_data_afterfilter(m_c_data.rows(),m_c_data.cols());
  for (int irow=0; irow<m_c_data.rows(); ++irow) {
    for (int icol=0; icol<m_c_data.cols(); ++icol) {
      c_data_afterfilter(irow,icol) = m_c_data(irow,icol) * roi_hf_filter_wf.at(icol);
    }
  }
 
  //do the second round of inverse FFT on wire
  Array::array_xxf tm_r_data = Array::idft_cr(c_data_afterfilter,0);
  m_r_data = tm_r_data.block(m_pad_nwires[plane],0,m_nwires[plane],m_nticks);
  wend = omp_get_wtime();
  g_decon_2D_tightROI += wend - wstart;


  log->debug("cudaOmnibusSigProc::decon_2D_ROI_refine() : total running time : {}", g_decon_2D_tightROI);

#ifdef HAVE_CUDA_H
  restore_baseline_cuda(m_r_data);
#else
  restore_baseline(m_r_data);
#endif
}


void cudaOmnibusSigProc::decon_2D_tightROI(int plane){
  // apply software filter on time

  double wstart, wend;

  wstart = omp_get_wtime(); 
  Waveform::realseq_t roi_hf_filter_wf;
  if (plane ==0){
    auto ncr1 = Factory::find<IFilterWaveform>("HfFilter","Wiener_tight_U");
    roi_hf_filter_wf = ncr1->filter_waveform(m_c_data.cols());
    auto ncr2 = Factory::find<IFilterWaveform>("LfFilter","ROI_tight_lf");
    auto temp_filter = ncr2->filter_waveform(m_c_data.cols());
    for(size_t i=0;i!=roi_hf_filter_wf.size();i++){
      roi_hf_filter_wf.at(i) *= temp_filter.at(i);
    }
  }else if (plane==1){
    auto ncr1 = Factory::find<IFilterWaveform>("HfFilter","Wiener_tight_V");
    roi_hf_filter_wf = ncr1->filter_waveform(m_c_data.cols());
    auto ncr2 = Factory::find<IFilterWaveform>("LfFilter","ROI_tight_lf");
    auto temp_filter = ncr2->filter_waveform(m_c_data.cols());
    for(size_t i=0;i!=roi_hf_filter_wf.size();i++){
      roi_hf_filter_wf.at(i) *= temp_filter.at(i);
    }
  }else{
    auto ncr1 = Factory::find<IFilterWaveform>("HfFilter","Wiener_tight_W");
    roi_hf_filter_wf = ncr1->filter_waveform(m_c_data.cols());
  }

#ifdef HAVE_CUDA_H
#ifdef CUDA_TEST
  
  wstart = omp_get_wtime(); 
  decon_2D_tightROI_CUDA(m_c_data.rows(), m_c_data.cols(), roi_hf_filter_wf.data(), m_r_data.data(), m_pad_nwires[plane],0,m_nwires[plane],m_nticks);
  wend = omp_get_wtime();
  g_decon_2D_tightROI += wend - wstart;
  log->debug("cudaOmnibusSigProc::decon_2D_tightROI() : total running time : {}", g_decon_2D_tightROI);

  restore_baseline_cuda(m_r_data);
  return;
  
#endif
#endif


  wstart = omp_get_wtime(); 
  //log->debug("cudaOmnibusSigProc::decon_2D_tightROI() : m_c_data : real : {},  image : {}, filter : {}", real(m_c_data(0,100)), imag(m_c_data(0,100)), roi_hf_filter_wf.at(100) );
  Array::array_xxc c_data_afterfilter(m_c_data.rows(),m_c_data.cols());
  for (int irow=0; irow<m_c_data.rows(); ++irow) {
    for (int icol=0; icol<m_c_data.cols(); ++icol) {
      c_data_afterfilter(irow,icol) = m_c_data(irow,icol) * roi_hf_filter_wf.at(icol);
    }
  }
  //log->debug("cudaOmnibusSigProc::decon_2D_tightROI() : c_data_afterfilter : real : {},  image : {}", real(c_data_afterfilter(0,100)), imag(c_data_afterfilter(0,100)) );
 
  //do the second round of inverse FFT on wire
  Array::array_xxf tm_r_data = Array::idft_cr(c_data_afterfilter,0);



  m_r_data = tm_r_data.block(m_pad_nwires[plane],0,m_nwires[plane],m_nticks);
  wend = omp_get_wtime();
  g_decon_2D_tightROI += wend - wstart;

#ifdef HAVE_CUDA_H
  restore_baseline_cuda(m_r_data);
#else
  restore_baseline(m_r_data);
#endif

  //log->debug("cudaOmnibusSigProc::decon_2D_tightROI() : m_c_data.rows() : {}, m_c_data.cols() : {}, m_pad_nwires[{}] : {}, ,m_nwires[{}] : {}, m_nticks : {}", m_c_data.rows(), m_c_data.cols(), plane, m_pad_nwires[plane], plane, m_nwires[plane], m_nticks);
  log->debug("cudaOmnibusSigProc::decon_2D_tightROI() : total running time : {}", g_decon_2D_tightROI);

}
 
// same as above but with "tight" -> "tighter" for ROI filterss.
void cudaOmnibusSigProc::decon_2D_tighterROI(int plane){
  // apply software filter on time
  double wstart, wend;


  Waveform::realseq_t roi_hf_filter_wf;
  if (plane ==0){
    auto ncr1 = Factory::find<IFilterWaveform>("HfFilter","Wiener_tight_U");
    roi_hf_filter_wf = ncr1->filter_waveform(m_c_data.cols());
    auto ncr2 = Factory::find<IFilterWaveform>("LfFilter","ROI_tighter_lf");
    auto temp_filter = ncr2->filter_waveform(m_c_data.cols());
    for(size_t i=0;i!=roi_hf_filter_wf.size();i++){
      roi_hf_filter_wf.at(i) *= temp_filter.at(i);
    }
  }else if (plane==1){
    auto ncr1 = Factory::find<IFilterWaveform>("HfFilter","Wiener_tight_V");
    roi_hf_filter_wf = ncr1->filter_waveform(m_c_data.cols());
    auto ncr2 = Factory::find<IFilterWaveform>("LfFilter","ROI_tighter_lf");
    auto temp_filter = ncr2->filter_waveform(m_c_data.cols());
    for(size_t i=0;i!=roi_hf_filter_wf.size();i++){
      roi_hf_filter_wf.at(i) *= temp_filter.at(i);
    }
  }else{
    auto ncr1 = Factory::find<IFilterWaveform>("HfFilter","Wiener_tight_W");
    roi_hf_filter_wf = ncr1->filter_waveform(m_c_data.cols());
  }

#ifdef HAVE_CUDA_H
#ifdef CUDA_TEST
  
  wstart = omp_get_wtime(); 
  decon_2D_tightROI_CUDA(m_c_data.rows(), m_c_data.cols(), roi_hf_filter_wf.data(), m_r_data.data(), m_pad_nwires[plane],0,m_nwires[plane],m_nticks);
  wend = omp_get_wtime();
  g_decon_2D_tightROI += wend - wstart;
  //log->debug("cudaOmnibusSigProc::decon_2D_tightROIer() : m_r_data : {} and {}", m_r_data(0,100), m_r_data(1,100) );
  log->debug("cudaOmnibusSigProc::decon_2D_tighterROI() : total running time : {}", g_decon_2D_tightROI);
  


  /*
  Array::array_xxc tc_data_afterfilter(m_c_data.rows(),m_c_data.cols());
  for (int irow=0; irow<m_c_data.rows(); ++irow) {
    for (int icol=0; icol<m_c_data.cols(); ++icol) {
      tc_data_afterfilter(irow,icol) = m_c_data(irow,icol) * roi_hf_filter_wf.at(icol);
    }
  }
  Array::array_xxf ttm_r_data = Array::idft_cr(tc_data_afterfilter,0);
  ttm_r_data -= m_r_data;
  //log->debug("cudaOmnibusSigProc::decon_2D_tightROIer() : m_r_data difference (L infinite): {}", ttm_r_data.lpNorm<1>() );
  log->debug("cudaOmnibusSigProc::decon_2D_tightROIer() : m_r_data size : {}, {}, ttm_r_data size {}, {}", m_r_data.rows(), m_r_data.cols(), ttm_r_data.rows(), ttm_r_data.cols() );
  //log->debug("cudaOmnibusSigProc::decon_2D_tightROIer() : m_r_data difference (L infinite): {}", ttm_r_data.matrix().squaredNorm() );
  //log->debug("cudaOmnibusSigProc::decon_2D_tightROIer() : m_r_data difference (L infinite): {}", ttm_r_data.matrix().lpNorm<1>() );
  log->debug("cudaOmnibusSigProc::decon_2D_tightROIer() : m_r_data difference (L infinite): {}", ttm_r_data.matrix().lpNorm<Eigen::Infinity>() );

  for(int j=0 ; j<ttm_r_data.rows() ; j++) {
    for(int i=0 ; i<ttm_r_data.cols() ; i++) {
      cout << ttm_r_data(i,j) << ", ";
    }
    cout << endl;
  }
  */

  restore_baseline_cuda(m_r_data);
  return;
  
#endif
#endif

  wstart = omp_get_wtime(); 
  Array::array_xxc c_data_afterfilter(m_c_data.rows(),m_c_data.cols());
  for (int irow=0; irow<m_c_data.rows(); ++irow) {
    for (int icol=0; icol<m_c_data.cols(); ++icol) {
      c_data_afterfilter(irow,icol) = m_c_data(irow,icol) * roi_hf_filter_wf.at(icol);
    }
  }
  
  //do the second round of inverse FFT on wire
  Array::array_xxf tm_r_data = Array::idft_cr(c_data_afterfilter,0);
  m_r_data = tm_r_data.block(m_pad_nwires[plane],0,m_nwires[plane],m_nticks);
  wend = omp_get_wtime(); 
  g_decon_2D_tightROI += wend - wstart;


  //log->debug("cudaOmnibusSigProc::decon_2D_tighterROI() : m_r_data : {} and {}", m_r_data(0,100), m_r_data(1,100) );


#ifdef HAVE_CUDA_H
  restore_baseline_cuda(m_r_data);
#else
  restore_baseline(m_r_data);
#endif

  g_decon_2D_tightROI += wend - wstart;
  log->debug("cudaOmnibusSigProc::decon_2D_tighterROI() : total running time : {}", g_decon_2D_tightROI);

}
 

void cudaOmnibusSigProc::decon_2D_looseROI(int plane){
  if (plane == 2) {
    return;                     // don't filter colleciton
  }

   // apply software filter on time

  Waveform::realseq_t roi_hf_filter_wf;
  Waveform::realseq_t roi_hf_filter_wf1;
  Waveform::realseq_t roi_hf_filter_wf2;
  if (plane ==0){
    auto ncr1 = Factory::find<IFilterWaveform>("HfFilter","Wiener_tight_U");
    roi_hf_filter_wf = ncr1->filter_waveform(m_c_data.cols());
    roi_hf_filter_wf1 = roi_hf_filter_wf;
    {
      auto ncr2 = Factory::find<IFilterWaveform>("LfFilter","ROI_loose_lf");
      auto temp_filter = ncr2->filter_waveform(m_c_data.cols());
      for(size_t i=0;i!=roi_hf_filter_wf.size();i++){
	roi_hf_filter_wf.at(i) *= temp_filter.at(i);
      }
    }
    {
      auto ncr2 = Factory::find<IFilterWaveform>("LfFilter","ROI_tight_lf");
      auto temp_filter = ncr2->filter_waveform(m_c_data.cols());
      for(size_t i=0;i!=roi_hf_filter_wf.size();i++){
	roi_hf_filter_wf1.at(i) *= temp_filter.at(i);
      }
    }
  }else if (plane==1){
    auto ncr1 = Factory::find<IFilterWaveform>("HfFilter","Wiener_tight_V");
    roi_hf_filter_wf = ncr1->filter_waveform(m_c_data.cols());
    roi_hf_filter_wf1 = roi_hf_filter_wf;
    {
      auto ncr2 = Factory::find<IFilterWaveform>("LfFilter","ROI_loose_lf");
      auto temp_filter = ncr2->filter_waveform(m_c_data.cols());
      for(size_t i=0;i!=roi_hf_filter_wf.size();i++){
	roi_hf_filter_wf.at(i) *= temp_filter.at(i);
      }
    }
     {
      auto ncr2 = Factory::find<IFilterWaveform>("LfFilter","ROI_tight_lf");
      auto temp_filter = ncr2->filter_waveform(m_c_data.cols());
      for(size_t i=0;i!=roi_hf_filter_wf.size();i++){
	roi_hf_filter_wf1.at(i) *= temp_filter.at(i);
      }
    }
  }else{
    auto ncr1 = Factory::find<IFilterWaveform>("HfFilter","Wiener_tight_W");
    roi_hf_filter_wf = ncr1->filter_waveform(m_c_data.cols());
  }

  const int n_lfn_nn = 2;
  const int n_bad_nn = plane ? 1 : 2;

  Array::array_xxc c_data_afterfilter(m_c_data.rows(),m_c_data.cols());
  for (auto och : m_channel_range[plane]) {
    const int irow = och.wire;

    roi_hf_filter_wf2 = roi_hf_filter_wf;
    if (masked_neighbors("bad", och, n_bad_nn) or
        masked_neighbors("lf_noisy", och, n_lfn_nn))
    {
      roi_hf_filter_wf2 = roi_hf_filter_wf1;
    }
    
    for (int icol=0; icol<m_c_data.cols(); ++icol) {
      c_data_afterfilter(irow,icol) = m_c_data(irow,icol) * roi_hf_filter_wf2.at(icol);
    }
  }
  
  //do the second round of inverse FFT on wire
  Array::array_xxf tm_r_data = Array::idft_cr(c_data_afterfilter,0);
  m_r_data = tm_r_data.block(m_pad_nwires[plane],0,m_nwires[plane],m_nticks);
#ifdef HAVE_CUDA_H
  restore_baseline_cuda(m_r_data);
#else
  restore_baseline(m_r_data);
#endif
}

// similar as decon_2D_looseROI() but without tightLF 
void cudaOmnibusSigProc::decon_2D_looseROI_debug_mode(int plane){
  // apply software filter on time
  if (plane == 2) {
    return;                     // don't filter colleciton
  }

  Waveform::realseq_t roi_hf_filter_wf;
  if (plane ==0){
    auto ncr1 = Factory::find<IFilterWaveform>("HfFilter","Wiener_tight_U");
    roi_hf_filter_wf = ncr1->filter_waveform(m_c_data.cols());
    auto ncr2 = Factory::find<IFilterWaveform>("LfFilter","ROI_loose_lf");
    auto temp_filter = ncr2->filter_waveform(m_c_data.cols());
    for(size_t i=0;i!=roi_hf_filter_wf.size();i++){
      roi_hf_filter_wf.at(i) *= temp_filter.at(i);
    }
  }else if (plane==1){
    auto ncr1 = Factory::find<IFilterWaveform>("HfFilter","Wiener_tight_V");
    roi_hf_filter_wf = ncr1->filter_waveform(m_c_data.cols());
    auto ncr2 = Factory::find<IFilterWaveform>("LfFilter","ROI_loose_lf");
    auto temp_filter = ncr2->filter_waveform(m_c_data.cols());
    for(size_t i=0;i!=roi_hf_filter_wf.size();i++){
      roi_hf_filter_wf.at(i) *= temp_filter.at(i);
    }
  }else{
    auto ncr1 = Factory::find<IFilterWaveform>("HfFilter","Wiener_tight_W");
    roi_hf_filter_wf = ncr1->filter_waveform(m_c_data.cols());
  }

  Array::array_xxc c_data_afterfilter(m_c_data.rows(),m_c_data.cols());
  for (int irow=0; irow<m_c_data.rows(); ++irow) {
    for (int icol=0; icol<m_c_data.cols(); ++icol) {
      c_data_afterfilter(irow,icol) = m_c_data(irow,icol) * roi_hf_filter_wf.at(icol);
    }
  }
  
  //do the second round of inverse FFT on wire
  Array::array_xxf tm_r_data = Array::idft_cr(c_data_afterfilter,0);
  m_r_data = tm_r_data.block(m_pad_nwires[plane],0,m_nwires[plane],m_nticks);
#ifdef HAVE_CUDA_H
  restore_baseline_cuda(m_r_data);
#else
  restore_baseline(m_r_data);
#endif

}

// return true if any channels w/in +/- nnn, inclusive, of the channel has the mask.
bool cudaOmnibusSigProc::masked_neighbors(const std::string& cmname, OspChan& ochan, int nnn)
{
  // take care of boundary cases
  int lo_wire = ochan.wire - nnn;
  int lo_chan = ochan.channel - nnn;
  while (lo_wire < 0) {
    ++lo_wire;
    ++lo_chan;
  }
  const int nwires = m_nwires[ochan.plane];
  int hi_wire = ochan.wire + nnn;
  int hi_chan = ochan.channel + nnn;
  while (hi_wire >= nwires) {
    --hi_wire;
    --hi_chan;
  }
  if (hi_chan < lo_chan) {      // how?  bogus inputs?
    return false;              
  }

  auto& cm = m_cmm[cmname];
  for (int och = lo_chan; och <= hi_chan; ++och) {
    if (cm.find(och) != cm.end()) {
      return true;
    }
  }
  return false;
}

void cudaOmnibusSigProc::decon_2D_hits(int plane){
   // apply software filter on time
  double wstart, wend;

  Waveform::realseq_t roi_hf_filter_wf;
  if (plane ==0){
    auto ncr1 = Factory::find<IFilterWaveform>("HfFilter","Wiener_wide_U");
    roi_hf_filter_wf = ncr1->filter_waveform(m_c_data.cols());
  }else if (plane==1){
    auto ncr1 = Factory::find<IFilterWaveform>("HfFilter","Wiener_wide_V");
    roi_hf_filter_wf = ncr1->filter_waveform(m_c_data.cols());
  }else{
    auto ncr1 = Factory::find<IFilterWaveform>("HfFilter","Wiener_wide_W");
    roi_hf_filter_wf = ncr1->filter_waveform(m_c_data.cols());
  }

#ifdef HAVE_CUDA_H
#ifdef CUDA_TEST
  
  wstart = omp_get_wtime(); 
  decon_2D_tightROI_CUDA(m_c_data.rows(), m_c_data.cols(), roi_hf_filter_wf.data(), m_r_data.data(), m_pad_nwires[plane],0,m_nwires[plane],m_nticks);
  wend = omp_get_wtime();
  g_decon_2D_tightROI += wend - wstart;
  log->debug("cudaOmnibusSigProc::decon_2D_hits() : total running time : {}", g_decon_2D_tightROI);

  restore_baseline_cuda(m_r_data);
  return;
  
#endif
#endif

  wstart = omp_get_wtime(); 
  Array::array_xxc c_data_afterfilter(m_c_data.rows(),m_c_data.cols());
  for (int irow=0; irow<m_c_data.rows(); ++irow) {
    for (int icol=0; icol<m_c_data.cols(); ++icol) {
      c_data_afterfilter(irow,icol) = m_c_data(irow,icol) * roi_hf_filter_wf.at(icol);
    }
  }
  
  //do the second round of inverse FFT on wire
  Array::array_xxf tm_r_data = Array::idft_cr(c_data_afterfilter,0);
  m_r_data = tm_r_data.block(m_pad_nwires[plane],0,m_nwires[plane],m_nticks);
  wend = omp_get_wtime();
  g_decon_2D_tightROI += wend - wstart;


  log->debug("cudaOmnibusSigProc::decon_2D_hits() : total running time : {}", g_decon_2D_tightROI);

  if (plane==2) {
#ifdef HAVE_CUDA_H
    restore_baseline_cuda(m_r_data);
#else
    restore_baseline(m_r_data);
#endif
  }
}

void cudaOmnibusSigProc::decon_2D_charge(int plane){
  // apply software filter on time

  double wstart, wend;

  Waveform::realseq_t roi_hf_filter_wf;
  if (plane ==0){
    auto ncr1 = Factory::find<IFilterWaveform>("HfFilter","Gaus_wide");
    roi_hf_filter_wf = ncr1->filter_waveform(m_c_data.cols());
  }else if (plane==1){
    auto ncr1 = Factory::find<IFilterWaveform>("HfFilter","Gaus_wide");
    roi_hf_filter_wf = ncr1->filter_waveform(m_c_data.cols());
  }else{
    auto ncr1 = Factory::find<IFilterWaveform>("HfFilter","Gaus_wide");
    roi_hf_filter_wf = ncr1->filter_waveform(m_c_data.cols());
  }

#ifdef HAVE_CUDA_H
#ifdef CUDA_TEST
  
  wstart = omp_get_wtime(); 
  decon_2D_tightROI_CUDA(m_c_data.rows(), m_c_data.cols(), roi_hf_filter_wf.data(), m_r_data.data(), m_pad_nwires[plane],0,m_nwires[plane],m_nticks);
  wend = omp_get_wtime();
  g_decon_2D_tightROI += wend - wstart;
  log->debug("cudaOmnibusSigProc::decon_2D_charge() : total running time : {}", g_decon_2D_tightROI);

  restore_baseline_cuda(m_r_data);
  return;
  
#endif
#endif
  
  wstart = omp_get_wtime();
  Array::array_xxc c_data_afterfilter(m_c_data.rows(),m_c_data.cols());
  for (int irow=0; irow<m_c_data.rows(); ++irow) {
    for (int icol=0; icol<m_c_data.cols(); ++icol) {
      c_data_afterfilter(irow,icol) = m_c_data(irow,icol) * roi_hf_filter_wf.at(icol);
    }
  }

  
  //do the second round of inverse FFT on wire
  Array::array_xxf tm_r_data = Array::idft_cr(c_data_afterfilter,0);
  m_r_data = tm_r_data.block(m_pad_nwires[plane],0,m_nwires[plane],m_nticks);
  wend = omp_get_wtime();
  g_decon_2D_tightROI += wend - wstart;



  if (plane==2) {
#ifdef HAVE_CUDA_H
    restore_baseline_cuda(m_r_data);
#else
    restore_baseline(m_r_data);
#endif
  }

  log->debug("cudaOmnibusSigProc::decon_2D_charge() : total running time : {}", g_decon_2D_tightROI);

}


bool cudaOmnibusSigProc::operator()(const input_pointer& in, output_pointer& out)
{
  //log->debug("cudaOmnibusSigProc::operator() :: called!!!!!");

  //cout << "==================================================" << endl;
  //cout << "== cudaOmnibusSigProc::operator()" << endl;
  //cout << "==================================================" << endl;
  //cout.flush();


  out = nullptr;
  if (!in) {
    log->debug("cudaOmnibusSigProc: see EOS");
    return true;
  }
  const size_t ntraces = in->traces()->size();
  if (ntraces) {
    log->debug("cudaOmnibusSigProc: receive frame {} with {} traces",
               in->ident(), ntraces);
  }
  else{
    out = std::make_shared<SimpleFrame>(in->ident(), in->time(),
                                        std::make_shared<ITrace::vector>(),
                                        in->tick());
    log->debug("cudaOmnibusSigProc: got and sending empty frame {}", out->ident());
    return true;
  }

  // Convert to OSP cmm indexed by OSB sequential channels, NOT WCT channel ID.
  m_cmm.clear();
  // double emap: name -> channel -> pair<int,int>
  for (auto cm : in->masks()) {
    const std::string name = cm.first;
    for (auto m: cm.second) {
      const int wct_channel_ident = m.first;
      const OspChan& och = m_channel_map[wct_channel_ident];
      if (och.plane < 0) {
        continue;               // in case user gives us multi apa frame
      }
      m_cmm[name][och.channel] = m.second;

    }
  }

  ITrace::vector* itraces = new ITrace::vector; // will become shared_ptr.
  IFrame::trace_summary_t thresholds;
  IFrame::trace_list_t wiener_traces, gauss_traces, perframe_traces[3];
  // here are some trace lists for debug mode
  IFrame::trace_list_t tight_lf_traces, loose_lf_traces, cleanup_roi_traces, break_roi_loop1_traces, break_roi_loop2_traces, shrink_roi_traces, extend_roi_traces;

  // initialize the overall response function ... 
  init_overall_response(in);

  // create a class for ROIs ... 
  ROI_formation roi_form(m_cmm, m_nwires[0], m_nwires[1], m_nwires[2], m_nticks, m_th_factor_ind, m_th_factor_col, m_pad, m_asy, m_rebin, m_l_factor, m_l_max_th, m_l_factor1, m_l_short_length, m_l_jump_one_bin);
  ROI_refinement roi_refine(m_cmm, m_nwires[0], m_nwires[1], m_nwires[2],m_r_th_factor,m_r_fake_signal_low_th,m_r_fake_signal_high_th,m_r_fake_signal_low_th_ind_factor,m_r_fake_signal_high_th_ind_factor,m_r_pad,m_r_break_roi_loop,m_r_th_peak,m_r_sep_peak,m_r_low_peak_sep_threshold_pre,m_r_max_npeaks,m_r_sigma,m_r_th_percent);//

  
  const std::vector<float>* perplane_thresholds[3] = {
    &roi_form.get_uplane_rms(),
    &roi_form.get_vplane_rms(),
    &roi_form.get_wplane_rms()
  };


  for (int iplane = 0; iplane != 3; ++iplane){
    const std::vector<float>& perwire_rmses = *perplane_thresholds[iplane];

    // load data into EIGEN matrices ...
    load_data(in, iplane); // load into a large matrix
    // initial decon ... 
    decon_2D_init(iplane); // decon in large matrix

    // Form tight ROIs
    if (iplane != 2){ // induction wire planes
      decon_2D_tighterROI(iplane);
      Array::array_xxf r_data_tight = m_r_data;
      //      r_data_tight = m_r_data;
      decon_2D_tightROI(iplane);
      roi_form.find_ROI_by_decon_itself(iplane, m_r_data, r_data_tight);
    }else{ // collection wire planes
      decon_2D_tightROI(iplane);
      roi_form.find_ROI_by_decon_itself(iplane, m_r_data);
    }
    
    // [wgu] save decon result after tight LF
    std::vector<double> dummy;
    if (m_use_roi_debug_mode) save_data(*itraces, tight_lf_traces, iplane, perwire_rmses, dummy);

    // Form loose ROIs
    if (iplane != 2){
      // [wgu] save decon result after loose LF
      if (m_use_roi_debug_mode) {
        decon_2D_looseROI_debug_mode(iplane);
        save_data(*itraces, loose_lf_traces, iplane, perwire_rmses, dummy);
      }

      decon_2D_looseROI(iplane);
      roi_form.find_ROI_loose(iplane,m_r_data);
      decon_2D_ROI_refine(iplane);
    }

    // [wgu] collection plane does not need loose LF
    // but save something to be consistent
    if (m_use_roi_debug_mode and iplane==2) save_data(*itraces, loose_lf_traces, iplane, perwire_rmses, dummy);

    // Refine ROIs
    roi_refine.load_data(iplane, m_r_data, roi_form);
    // roi_refine.refine_data(iplane, roi_form);
    if (not m_use_roi_debug_mode) // default: use_roi_debug_mode=false
      roi_refine.refine_data(iplane, roi_form);
    else { // CAVEAT: ONLY USE ME FOR DEBUGGING

      std::cout << "[wgu] CleanUpROIs ..." << std::endl;
      roi_refine.refine_data_debug_mode(iplane, roi_form, "CleanUpROIs");
      save_roi(*itraces, cleanup_roi_traces, iplane, roi_refine.get_rois_by_plane(iplane));

      std::cout << "[wgu] BreakROIs ..." << std::endl;
      roi_refine.refine_data_debug_mode(iplane, roi_form, "BreakROIs");
      save_roi(*itraces, break_roi_loop1_traces, iplane, roi_refine.get_rois_by_plane(iplane));

      std::cout << "[wgu] BreakROIs_2 ..." << std::endl;
      roi_refine.refine_data_debug_mode(iplane, roi_form, "BreakROIs");
      save_roi(*itraces, break_roi_loop2_traces, iplane, roi_refine.get_rois_by_plane(iplane));

      std::cout << "[wgu] ShrinkROIs ..." << std::endl;
      roi_refine.refine_data_debug_mode(iplane, roi_form, "ShrinkROIs");
      save_roi(*itraces, shrink_roi_traces, iplane, roi_refine.get_rois_by_plane(iplane));

      std::cout << "[wgu] ExtendROIs ..." << std::endl;
      roi_refine.refine_data_debug_mode(iplane, roi_form, "ExtendROIs");
      save_roi(*itraces, extend_roi_traces, iplane, roi_refine.get_rois_by_plane(iplane));
    }


    // merge results ...
    decon_2D_hits(iplane);
    roi_refine.apply_roi(iplane, m_r_data);
    //roi_form.apply_roi(iplane, m_r_data,1);
    save_data(*itraces, perframe_traces[iplane], iplane, perwire_rmses, thresholds);
    wiener_traces.insert(wiener_traces.end(), perframe_traces[iplane].begin(), perframe_traces[iplane].end());

    decon_2D_charge(iplane);
    roi_refine.apply_roi(iplane, m_r_data);
    //roi_form.apply_roi(iplane, m_r_data,1);
    std::vector<double> dummy_thresholds;
    save_data(*itraces, gauss_traces, iplane, perwire_rmses, dummy_thresholds);

    m_c_data.resize(0,0); // clear memory
    m_r_data.resize(0,0); // clear memory
  }

  SimpleFrame* sframe = new SimpleFrame(in->ident(), in->time(),
                                        ITrace::shared_vector(itraces),
                                        in->tick(), m_cmm);
  sframe->tag_frame(m_frame_tag);

  // this assumes save_data produces itraces in OSP channel order
  // std::vector<float> perplane_thresholds[3] = {
  //   roi_form.get_uplane_rms(),
  //   roi_form.get_vplane_rms(),
  //   roi_form.get_wplane_rms()
  // };

  // IFrame::trace_summary_t threshold;
  // for (int iplane=0; iplane<3; ++iplane) {
  //   for (float val : perplane_thresholds[iplane]) {
  //     threshold.push_back((double)val);
  //   }
  // }

  sframe->tag_traces(m_wiener_tag, wiener_traces);
  sframe->tag_traces(m_wiener_threshold_tag, wiener_traces, thresholds);
  sframe->tag_traces(m_gauss_tag, gauss_traces);

  if (m_use_roi_debug_mode) {
    sframe->tag_traces(m_tight_lf_tag, tight_lf_traces);
    sframe->tag_traces(m_loose_lf_tag, loose_lf_traces);
    sframe->tag_traces(m_cleanup_roi_tag, cleanup_roi_traces);
    sframe->tag_traces(m_break_roi_loop1_tag, break_roi_loop1_traces);
    sframe->tag_traces(m_break_roi_loop2_tag, break_roi_loop2_traces);
    sframe->tag_traces(m_shrink_roi_tag, shrink_roi_traces);
    sframe->tag_traces(m_extend_roi_tag, extend_roi_traces);
  }

  log->debug("cudaOmnibusSigProc: produce {} traces: {} {}, {} {}, frame tag: {}",
             itraces->size(),
             wiener_traces.size(), m_wiener_tag,
             gauss_traces.size(), m_gauss_tag, m_frame_tag);

  out = IFrame::pointer(sframe);
  
  return true;
}

// Local Variables:
// mode: c++
// c-basic-offset: 2
// End:

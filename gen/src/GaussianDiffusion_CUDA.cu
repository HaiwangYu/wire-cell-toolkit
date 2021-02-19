#include "WireCellGen/GaussianDiffusion.h"
#include "WireCellGen/BinnedDiffusion_transform.h"

#include <cuda.h>
#include <curand.h>
#include <thrust/reduce.h>

#include <iostream>             // debugging
#include <omp.h>


extern double g_set_sampling_part1;
extern double g_set_sampling_part2;
extern double g_set_sampling_part3;
extern double g_set_sampling_part4;
extern double g_set_sampling_part5;
extern double g_set_sampling_part6;

size_t g_total_sample_size = 0;

double* tempVec;


using namespace WireCell;
using namespace std;

#define MAX_NPSS_DEVICE 1000
#define MAX_NTSS_DEVICE 1000
#define WARP_SIZE 32
#define FULL_MASK 0xffffffff
#define MAX_RANDOM_LENGTH 512*1024*1024//67108864 // 64*1024*1024

__shared__ float partSum[WARP_SIZE];
__shared__ float finalSum;


// these macros are really really helpful
#define CUDA_SAFE_CALL( call) {                                              \
    cudaError err = call;                                                    \
    if( cudaSuccess != err) {                                                \
        fprintf(stderr, "CUDA error in file '%s' in line %i : %s.\n",        \
                __FILE__, __LINE__, cudaGetErrorString( err) );              \
        exit(EXIT_FAILURE);                                                  \
    } }

#define CUFFT_SAFE_CALL( call) {                                             \
    cufftResult err = call;                                                  \
    if( CUFFT_SUCCESS != err) {                                              \
        fprintf(stderr, "CUFFT error in file '%s' in line %i : %02X.\n",     \
                __FILE__, __LINE__, err );                                   \
        exit(EXIT_FAILURE);                                                  \
    } }

#define CHECKLASTERROR   {                                                 \
        cudaError_t err = cudaGetLastError();                                    \
        if (err != cudaSuccess) {                                                \
                fprintf(stderr, "CUDA error in file '%s' in line %i : %s.\n",        \
                __FILE__, __LINE__, cudaGetErrorString( err) );              \
        exit(EXIT_FAILURE);                                                  \
        } }

#define CURAND_SAFE_CALL(err) { \
    if((err)!=CURAND_STATUS_SUCCESS) { \
        fprintf(stderr, "CURAND error in file at '%s' in line %i : %02X.\n",__FILE__,__LINE__, err);\
        exit(EXIT_FAILURE); \
    } } 



#ifdef HAVE_CUDA_INC




__global__ void ker_outer_product(double* p, size_t plen,  double* t, size_t tlen, float* out) {

  int index = threadIdx.x + blockIdx.x * blockDim.x;
  int pidx = index / tlen;
  int tidx = index % tlen;

  if(index < plen*tlen) {
    out[index] = p[pidx]*t[tidx];
  }


}


__global__ void ker_normalize(float* out, double charge, float sum, size_t len, double charge_sign) {
  int index = threadIdx.x + blockIdx.x * blockDim.x;


  if(index < len) {
    out[index] *= (charge/sum);
  }
}

//
// https://devblogs.nvidia.com/faster-parallel-reductions-kepler/
// https://devblogs.nvidia.com/using-cuda-warp-level-primitives/
//

__global__ void ker_patching(double* p, size_t plen,  double* t, size_t tlen, float* out, double charge) {

  int index = threadIdx.x + blockIdx.x * blockDim.x;
  
  /* layout 1: current
   * index = tidx + pidx * tlen;
   */
//   int pidx = index / tlen;
//   int tidx = index % tlen;

  /* layout 2: consistent result with CPU
   * index = pidx + tidx * plen;
   */
  int pidx = index % plen;
  int tidx = index / plen;

  size_t len = plen*tlen;

  float sum = 0.0;

  if(index < len) {
    sum = p[pidx]*t[tidx];
    out[index] = sum;
  } 

  /////////////////////////////////////////////////////////////////////////////////

  int lane = index % WARP_SIZE;
  int wid = index / WARP_SIZE;

  for (int offset = WARP_SIZE/2; offset > 0; offset /= 2) 
    sum += __shfl_down_sync(FULL_MASK, sum, offset);



  if (lane==0) partSum[wid]=sum; // Write reduced value to shared memory

  __syncthreads();              // Wait for all partial reductions

  //read from shared memory only if that warp existed
  sum = (index < blockDim.x / WARP_SIZE) ? partSum[lane] : 0.0;

  if (wid==0) {
    for (int offset = WARP_SIZE/2; offset > 0; offset /= 2) 
      sum += __shfl_down_sync(FULL_MASK, sum, offset);

    if(index == 0) finalSum = sum;
  }


  __syncthreads();  

  /////////////////////////////////////////////////////////////////////////////////



  if(index < len) {
    out[index] *= (charge/finalSum);
  }



}



__global__ void ker_sampling(float* patch, float* rand, size_t len, double charge, double sign) {
  int index = threadIdx.x + blockIdx.x * blockDim.x;

  size_t n = __double2uint_rd(charge);

  float sum = 0.0;

  if(index < len) {
    double p = patch[index]/charge;
    double q = 1 - p;
    double mu = n*p;
    double sigma = sqrt(n*p*q);
      
    sum= rand[index] * sigma + mu;
    patch[index] = sum;
  }


  /////////////////////////////////////////////////////////////////////////////////

  int lane = index % WARP_SIZE;
  int wid = index / WARP_SIZE;

  for (int offset = WARP_SIZE/2; offset > 0; offset /= 2)
    sum += __shfl_down_sync(FULL_MASK, sum, offset);



  if (lane==0) partSum[wid]=sum; // Write reduced value to shared memory

  __syncthreads();              // Wait for all partial reductions

  //read from shared memory only if that warp existed
  sum = (index < blockDim.x / WARP_SIZE) ? partSum[lane] : 0.0;

  if (wid==0) {
    for (int offset = WARP_SIZE/2; offset > 0; offset /= 2)
      sum += __shfl_down_sync(FULL_MASK, sum, offset);

    if(index == 0) finalSum = sum;
  }


  __syncthreads();

  /////////////////////////////////////////////////////////////////////////////////

  if(index < len) {
    patch[index] *= (sign*charge/finalSum);
  }



}




void Gen::BinnedDiffusion_transform::init_Device() {
  CUDA_SAFE_CALL( cudaMalloc(&m_pvec_D, MAX_NPSS_DEVICE * sizeof(double)) );
  CUDA_SAFE_CALL( cudaMalloc(&m_tvec_D, MAX_NTSS_DEVICE * sizeof(double)) );
  CUDA_SAFE_CALL( cudaMalloc(&m_patch_D, MAX_NPSS_DEVICE * MAX_NTSS_DEVICE * sizeof(float)) );
  //CUDA_SAFE_CALL( cudaMalloc(&m_rand_D, MAX_NPSS_DEVICE * MAX_NTSS_DEVICE * sizeof(float)) );
  CUDA_SAFE_CALL( cudaMalloc(&m_rand_D, MAX_RANDOM_LENGTH * sizeof(float)) );

  CURAND_SAFE_CALL(curandCreateGenerator(&m_Gen,CURAND_RNG_PSEUDO_DEFAULT));
  CURAND_SAFE_CALL(curandSetPseudoRandomGeneratorSeed(m_Gen, 0));
  CURAND_SAFE_CALL(curandGenerateNormal(m_Gen, m_rand_D, MAX_RANDOM_LENGTH, 0.0, 1.0));

  tempVec = new double[1024];
  //cout << "Gen::BinnedDiffusion_transform::init_Device()" << endl;
}


void Gen::BinnedDiffusion_transform::clear_Device() {
  CUDA_SAFE_CALL(cudaFree(m_pvec_D));
  CUDA_SAFE_CALL(cudaFree(m_tvec_D));
  CUDA_SAFE_CALL(cudaFree(m_patch_D));
  CUDA_SAFE_CALL(cudaFree(m_rand_D));

  CURAND_SAFE_CALL(curandDestroyGenerator(m_Gen));

  delete[] tempVec;
}



void Gen::GaussianDiffusion::set_sampling_CUDA(double* pvec_D, double* tvec_D, float* patch_D, float* rand_D, curandGenerator_t* gen,
                                          const Binning& tbin, // overall time tick binning
                                          const Binning& pbin, // overall impact position binning
                                          double nsigma,
                                          IRandom::pointer fluctuate,
                                          unsigned int weightstrat) {

    if (m_patch.size() > 0) {
        return;
    }

    double wstart, wend;


    wstart = omp_get_wtime();
    /// Sample time dimension
    auto tval_range = m_time_desc.sigma_range(nsigma);
    auto tbin_range = tbin.sample_bin_range(tval_range.first, tval_range.second);
    const size_t ntss = tbin_range.second - tbin_range.first;
    m_toffset_bin = tbin_range.first;
    auto tvec =  m_time_desc.binint(tbin.edge(m_toffset_bin), tbin.binsize(), ntss);

    if (!ntss) {
        cerr << "Gen::GaussianDiffusion: no time bins for [" << tval_range.first/units::us << "," << tval_range.second/units::us << "] us\n";
        return;
    }

    /// Sample pitch dimension.
    auto pval_range = m_pitch_desc.sigma_range(nsigma);
    auto pbin_range = pbin.sample_bin_range(pval_range.first, pval_range.second);
    const size_t npss = pbin_range.second - pbin_range.first;
    m_poffset_bin = pbin_range.first;
    auto pvec = m_pitch_desc.binint(pbin.edge(m_poffset_bin), pbin.binsize(), npss);


    if (!npss) {
        cerr << "No impact bins [" << pval_range.first/units::mm << "," << pval_range.second/units::mm << "] mm\n";
        return;
    }


    // make charge weights for later interpolation.
    /// fixme: for hanyu.
    if(weightstrat == 2){
        auto wvec = m_pitch_desc.weight(pbin.edge(m_poffset_bin), pbin.binsize(), npss, pvec);
        m_qweights = wvec;
    }
    if(weightstrat == 1){
        m_qweights.resize(npss, 0.5);
    }
    wend = omp_get_wtime();
    g_set_sampling_part1 += wend - wstart;

    //cout << "set_sampling_CUDA() : npss=" << npss << ", ntss=" << ntss << ", m_deposition->charge() = " << m_deposition->charge() << endl;


    wstart = omp_get_wtime();
    patch_t ret = patch_t::Zero(npss, ntss);
    const double charge_sign = m_deposition->charge() < 0 ? -1 : 1;
    const double charge = charge_sign * m_deposition->charge();
    m_patch = ret;
    sampling_CUDA(pvec.data(), npss, tvec.data(), ntss, m_patch.data(), charge_sign, charge, pvec_D, tvec_D, patch_D, (fluctuate ? true : false), rand_D, gen);
    wend = omp_get_wtime();
    g_set_sampling_part2 += wend - wstart;


}











void Gen::GaussianDiffusion::sampling_CUDA(double* pvec, const size_t npss, double* tvec, const size_t ntss, float* output, const double sign, const double charge, double* pvec_D, double* tvec_D, float* patch_D, bool fluc, float* rand_D, curandGenerator_t* gen) {
    
    size_t len = npss*ntss;
    //g_total_sample_size += len;

    double wstart, wend;

    wstart = omp_get_wtime();
    //CUDA_SAFE_CALL( cudaMemcpy(pvec_D, pvec, npss*sizeof(double), cudaMemcpyHostToDevice) );
    //CUDA_SAFE_CALL( cudaMemcpy(tvec_D, tvec, ntss*sizeof(double), cudaMemcpyHostToDevice) );
    memcpy(tempVec, pvec, npss*sizeof(double));
    memcpy(&tempVec[npss], tvec, ntss*sizeof(double));
    CUDA_SAFE_CALL( cudaMemcpy(pvec_D, tempVec, (ntss+npss)*sizeof(double), cudaMemcpyHostToDevice) );
    tvec_D = &pvec_D[npss];
    //cudaDeviceSynchronize();
    wend = omp_get_wtime();
    g_set_sampling_part3 += wend - wstart;


    size_t thSize = len / WARP_SIZE;
    if(len % WARP_SIZE) {
        thSize = (thSize + 1)*WARP_SIZE;
    } else {
        thSize = thSize*WARP_SIZE;
    }


    wstart = omp_get_wtime();
    ker_patching<<<1, thSize>>>(pvec_D, npss, tvec_D, ntss, patch_D, charge);
    wend = omp_get_wtime();
    g_set_sampling_part4 += wend - wstart;


    //cudaDeviceSynchronize();
    if(fluc) {

        //wstart = omp_get_wtime();
        //CURAND_SAFE_CALL(curandGenerateNormal(*gen, rand_D, len*2, 0.0, 1.0));
        //wend = omp_get_wtime();
        //g_set_sampling_part4 += wend - wstart;
        

        wstart = omp_get_wtime();
        //ker_sampling<<<1, thSize>>>(patch_D, rand_D, len, charge, sign);
        ker_sampling<<<1, thSize>>>(patch_D, &rand_D[g_total_sample_size], len, charge, sign);
        g_total_sample_size += len;
        if(g_total_sample_size > MAX_RANDOM_LENGTH - 1000) g_total_sample_size -= (MAX_RANDOM_LENGTH-1000);
        //cudaDeviceSynchronize();
        //wend = omp_get_wtime();
        //g_set_sampling_part5 += wend - wstart;
        //cudaDeviceSynchronize();
        wend = omp_get_wtime();
        g_set_sampling_part5 += wend - wstart;
    }


    wstart = omp_get_wtime();
    CUDA_SAFE_CALL( cudaMemcpy(output, patch_D, len*sizeof(float), cudaMemcpyDeviceToHost) );
    wend = omp_get_wtime();
    g_set_sampling_part6 += wend - wstart;

}



#endif




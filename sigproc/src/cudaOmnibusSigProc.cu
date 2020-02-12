#include "WireCellSigProc/cudaOmnibusSigProc.h"


#ifdef HAVE_CUDA_H

#include <cuda.h>

#include <iostream>
#include <omp.h>

#include <thrust/fill.h>
#include <thrust/sort.h>

#include <cufft.h>

#define THREAD_PER_BLOCK 1024
#define ROW_BLOCK 300

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


using namespace std;

using namespace WireCell;

using namespace WireCell::SigProc;

__global__ void ker_restore_baseline_init(float* data, float* data_temp, int* flag,  int row, int col) {

  int index = threadIdx.x + blockIdx.x * blockDim.x;

  if(index < row*col) {
    data_temp[index] = data[index];
    /*
    if(data[index] != 0.0) {
      data_temp[index] = data[index];
      flag[index] = 1;
    } else {
      data_temp[index] = 0.0;
      flag[index] = 0;
    }
    */ 
  }

}

__global__ void ker_restore_baseline_sort1(float* data, float* median, int row, int col) {

  //int index = threadIdx.x + blockIdx.x * blockDim.x;
  int index = blockIdx.x;

  //thrust::device_ptr<float> data(&(data_ptr[index*col]));
 
  if(index < row) { 
    thrust::sort( thrust::device, data + index*col, data + (index+1)*col );
    //thrust::sort( thrust::device, data, data + col );
    //thrust::reduce( thrust::device, data, data + col );
    //thrust::sort( thrust::seq, &(data[index*col]), &(data[(index+1)*col]) );
    //thrust::sort( &(data[index*col]), &(data[(index+1)*col]) );
    median[index] = data[index*col + col/2];
  }

}

__global__ void ker_restore_baseline_sort2(float* data, float* median, int* flag, int row, int col) {

  //int index = threadIdx.x + blockIdx.x * blockDim.x;
  int index = blockIdx.x;

  if(index < row) { 
    thrust::sort( thrust::device, data + index*col, data + (index+1)*col );
    int len = thrust::count( thrust::device, flag + index*col, flag + (index+1)*col, 1 );
    median[index] = data[index*col + len/2];
  }

}

__global__ void ker_restore_baseline_base(float* data, float* median, int* flag,  int row, int col) {


  int index = threadIdx.x + blockIdx.x * blockDim.x;
  int rowIdx = index / col;

  float baseline = median[rowIdx];

  if(index < row*col) {
    float diff = fabs(data[index] - baseline);

    if(diff < 500) {
      data[index] = diff;
      flag[index] = 1;
    } else {
      data[index] = 501.0;
      flag[index] = 0;
    } 
  }

}


__global__ void ker_restore_baseline_shift(float* data, float* median, int row, int col) {


  int index = threadIdx.x + blockIdx.x * blockDim.x;
  int rowIdx = index / col;

  float baseline = median[rowIdx];

  if(index < row*col) {
    if(data[index] != 0.0) data[index] -= baseline; 
  }

}


__global__ void ker_decon_2D_tightROI(cufftComplex* data, int row, int col, float* filter) {

  int index = threadIdx.x + blockIdx.x * blockDim.x;
  int colIdx = index % col;
  float scale = filter[colIdx]/(float)col;

  if(index < row*col) {
    data[index].x *= scale; 
    data[index].y *= scale; 
  }

}


void cudaOmnibusSigProc::init_cudaOmnibusSigProc_CUDA() {
  
  CUDA_SAFE_CALL( cudaMalloc(&m_r_data_D, MAX_ROW_M_R_DATA_DEVICE * MAX_COL_M_R_DATA_DEVICE * sizeof(float)) );
  CUDA_SAFE_CALL( cudaMalloc(&signal_D, MAX_ROW_M_R_DATA_DEVICE * MAX_COL_M_R_DATA_DEVICE * sizeof(float)) );
  CUDA_SAFE_CALL( cudaMalloc(&temp_signal_D, MAX_ROW_M_R_DATA_DEVICE * MAX_COL_M_R_DATA_DEVICE * sizeof(float)) );

  CUDA_SAFE_CALL( cudaMalloc(&m_c_data_D, MAX_ROW_M_R_DATA_DEVICE * MAX_COL_M_R_DATA_DEVICE * sizeof(cufftComplex)) );
  CUDA_SAFE_CALL( cudaMalloc(&m_c_data_temp_D, MAX_ROW_M_R_DATA_DEVICE * MAX_COL_M_R_DATA_DEVICE * sizeof(cufftComplex)) );
  CUDA_SAFE_CALL( cudaMalloc(&roi_filter_D, MAX_COL_M_R_DATA_DEVICE * sizeof(float)) );
  log->debug("cudaOmnibusSigProc::init_cudaOmnibusSigProc_CUDA() : cudaMalloc() ");

  signal_dev_ptr = thrust::device_malloc<float>(MAX_ROW_M_R_DATA_DEVICE * MAX_COL_M_R_DATA_DEVICE);
  temp_signal_dev_ptr = thrust::device_malloc<float>(MAX_ROW_M_R_DATA_DEVICE * MAX_COL_M_R_DATA_DEVICE);

  signal_flag_dev_ptr = thrust::device_malloc<int>(MAX_ROW_M_R_DATA_DEVICE * MAX_COL_M_R_DATA_DEVICE);
  temp_signal_flag_dev_ptr = thrust::device_malloc<int>(MAX_ROW_M_R_DATA_DEVICE * MAX_COL_M_R_DATA_DEVICE);

  temp_row_dev_ptr = thrust::device_malloc<float>(MAX_ROW_M_R_DATA_DEVICE);


  for(int i=0 ; i<STREAM_NUM ; i++) {
    CUDA_SAFE_CALL( cudaStreamCreate(&m_Streams[i]) );
  }


}

void cudaOmnibusSigProc::upload_c_data(std::complex<float>* c, int row, int col) {

  CUDA_SAFE_CALL( cudaMemcpy(m_c_data_D, c, row*col*sizeof(std::complex<float>), cudaMemcpyHostToDevice) );

}


void cudaOmnibusSigProc::clean_cudaOmnibusSigProc_CUDA() {
  CUDA_SAFE_CALL(cudaFree(m_r_data_D));
  CUDA_SAFE_CALL(cudaFree(signal_D));
  CUDA_SAFE_CALL(cudaFree(temp_signal_D));

  thrust::device_free(signal_dev_ptr);
  thrust::device_free(temp_signal_dev_ptr);

  thrust::device_free(signal_flag_dev_ptr);
  thrust::device_free(temp_signal_flag_dev_ptr);

  thrust::device_free(temp_row_dev_ptr);

  for(int i=0 ; i<STREAM_NUM ; i++) {
    cudaStreamDestroy(m_Streams[i]);
  }

}

void cudaOmnibusSigProc::restore_baseline_CUDA(float* data, int row, int col) {

  if(row % STREAM_NUM != 0) {
    cout << "====================================================================" << endl;
    cout << "restore_base_CUDA() : row=" << row << ", STREAM_NUM=" << STREAM_NUM << endl;
    cout << "====================================================================" << endl;

  }

  int row_block_size = row / STREAM_NUM;
  cout << "restore_base_CUDA() : row_block_size=" << row_block_size << endl;

  

  //CUDA_SAFE_CALL( cudaHostRegister(data, row*col*sizeof(float), cudaHostRegisterDefault) );
  cudaHostRegister(data, row*col*sizeof(float), cudaHostRegisterPortable);
  //cudaHostRegister(data, MAX_ROW_M_R_DATA_DEVICE*MAX_COL_M_R_DATA_DEVICE*sizeof(float), cudaHostRegisterPortable);


  double wstart, wend;


  for(int s=0 ; s<STREAM_NUM ; s++) {

  /*
  cout << "signal : ";
  for(int i=0 ; i<col ; i++) {
    cout << data[i] << ", ";
  }
  cout << endl;
  */

  //CUDA_SAFE_CALL( cudaMemcpyAsync(m_r_data_D + s*row_block_size*col, data + s*row_block_size*col, row_block_size*col*sizeof(float), cudaMemcpyHostToDevice, m_Streams[s]) );

  //log->debug("cudaOmnibusSigProc::restore_baseline_CUDA() : cudaMemcpy() ");

  //ker_restore_baseline_init<<<(row*col)/THREAD_PER_BLOCK + 1, THREAD_PER_BLOCK>>>(m_r_data_D, thrust::raw_pointer_cast(signal_dev_ptr), thrust::raw_pointer_cast(signal_flag_dev_ptr), row, col);
  ker_restore_baseline_init<<<(row_block_size*col)/THREAD_PER_BLOCK + 1, THREAD_PER_BLOCK, 0, m_Streams[s]>>>(m_r_data_D + s*row_block_size*col, thrust::raw_pointer_cast(signal_dev_ptr) + s*row_block_size*col, thrust::raw_pointer_cast(signal_flag_dev_ptr) + s*row_block_size*col, row_block_size, col);

  /*
  int iter = row / ROW_BLOCK;
  (row % ROW_BLOCK == 0) ? : iter++;
  for (int i=0 ; i<iter ; i++) {
    ker_restore_baseline_sort1<<<ROW_BLOCK, 1>>>(thrust::raw_pointer_cast(signal_dev_ptr), thrust::raw_pointer_cast(temp_row_dev_ptr), row, col, i);
  }
  */

  int iter_size = row_block_size / 4;
  for(int i=0 ; i<4 ; i++) {
    ker_restore_baseline_sort1<<<row_block_size, 1, 0, m_Streams[s]>>>(thrust::raw_pointer_cast(signal_dev_ptr) + s*row_block_size*col + i*iter_size*col, thrust::raw_pointer_cast(temp_row_dev_ptr) + s*row_block_size + i*iter_size, iter_size, col);
  }


  /*
  wstart = omp_get_wtime();
  for (int i=0 ; i<row ; i++){
    thrust::sort( &(signal_dev_ptr[i*col]), &(signal_dev_ptr[(i+1)*col]) );
    temp_row_dev_ptr[i] = signal_dev_ptr[i*col + col/2];
    //float median = signal_dev_ptr[i*col + col/2];
    //cout << "[i:"<<i<<"] basseline : " << median << endl;
    //float median = signal_dev_ptr[i*col];
    //cout << "[i:"<<i<<"] basseline : " << median << endl;
  } 
  wend = omp_get_wtime();
  cout << "first sort : " << wend - wstart << endl;
  */


  //ker_restore_baseline_base<<<(row*col)/THREAD_PER_BLOCK + 1, THREAD_PER_BLOCK>>>(thrust::raw_pointer_cast(signal_dev_ptr), thrust::raw_pointer_cast(temp_row_dev_ptr), thrust::raw_pointer_cast(signal_flag_dev_ptr), row, col);
  ker_restore_baseline_base<<<(row_block_size*col)/THREAD_PER_BLOCK + 1, THREAD_PER_BLOCK, 0, m_Streams[s]>>>(thrust::raw_pointer_cast(signal_dev_ptr) + s*row_block_size*col, thrust::raw_pointer_cast(temp_row_dev_ptr) + s*row_block_size, thrust::raw_pointer_cast(signal_flag_dev_ptr) + s*row_block_size*col, row_block_size, col);

  /*
  for (int i=0 ; i<iter ; i++) {
    ker_restore_baseline_sort2<<<ROW_BLOCK, 1>>>(thrust::raw_pointer_cast(signal_dev_ptr), thrust::raw_pointer_cast(temp_row_dev_ptr), thrust::raw_pointer_cast(signal_flag_dev_ptr), row, col, i);
  }
  */

  for(int i=0 ; i<4 ; i++) {
    ker_restore_baseline_sort2<<<row_block_size, 1, 0, m_Streams[s]>>>(thrust::raw_pointer_cast(signal_dev_ptr) + s*row_block_size*col + i*iter_size*col, thrust::raw_pointer_cast(temp_row_dev_ptr) + s*row_block_size + i*iter_size, thrust::raw_pointer_cast(signal_flag_dev_ptr) + s*row_block_size*col + i*iter_size*col, iter_size, col);
  }

  /*
  wstart = omp_get_wtime();
  for (int i=0 ; i<row ; i++){
    thrust::sort( &(signal_dev_ptr[i*col]), &(signal_dev_ptr[(i+1)*col]) );
    int len = thrust::count( &(signal_flag_dev_ptr[i*col]), &(signal_flag_dev_ptr[(i+1)*col]), 1 );

    temp_row_dev_ptr[i] = signal_dev_ptr[i*col + len/2];

  }
  wend = omp_get_wtime();
  cout << "second sort : " << wend - wstart << endl;
  */

  ker_restore_baseline_shift<<<(row_block_size*col)/THREAD_PER_BLOCK + 1, THREAD_PER_BLOCK, 0, m_Streams[s]>>>(m_r_data_D + s*row_block_size*col, thrust::raw_pointer_cast(temp_row_dev_ptr) + s*row_block_size, row_block_size, col);


  CUDA_SAFE_CALL( cudaMemcpyAsync(data + s*row_block_size*col, m_r_data_D + s*row_block_size*col, row_block_size*col*sizeof(float), cudaMemcpyDeviceToHost, m_Streams[s]) );

  } // for stream

  cudaDeviceSynchronize();
  CUDA_SAFE_CALL( cudaHostUnregister(data) );
  
}



void cudaOmnibusSigProc::decon_2D_tightROI_CUDA(int row, int col, float* filter, float* r_data, int start_row, int start_col, int row_len, int col_len) {

  
  CUDA_SAFE_CALL( cudaMemcpy(roi_filter_D, filter, col*sizeof(float), cudaMemcpyHostToDevice) );

  ker_decon_2D_tightROI<<<(row*col)/THREAD_PER_BLOCK + 1, THREAD_PER_BLOCK>>>(m_c_data_D, row, col, roi_filter_D);
  CHECKLASTERROR;
  //cudaDeviceSynchronize();

  //std::complex<float>* c_data = (std::complex<float> *)( malloc(row*col*sizeof(std::complex<float>)) );
  //CUDA_SAFE_CALL( cudaMemcpy(c_data, m_c_data_D, row*col*sizeof(std::complex<float>), cudaMemcpyDeviceToHost) );
  //cout << "cudaOmnibusSigProc::decon_2D_tightROI_CUDA() : real : " << real(c_data[100]) << ", image : " << imag(c_data[100]) << endl;
  //cout << "cudaOmnibusSigProc::decon_2D_tightROI_CUDA() : filter : " << filter[100] << ", row : " << row << ", col : " << col << endl;


  cufftHandle handle;
  int rank = 1;                   // --- 1D FFTs
  int n[] = { col };              // --- Size of the Fourier transform
  int istride = 1, ostride = 1;   // --- Distance between two successive input/output elements
  int idist = col, odist = col;   // --- Distance between batches
  int inembed[] = { 0 };          // --- Input size with pitch (ignored for 1D transforms)
  int onembed[] = { 0 };          // --- Output size with pitch (ignored for 1D transforms)
  int batch = row;                // --- Number of batched executions
  CUFFT_SAFE_CALL( cufftPlanMany(&handle, rank, n, 
              inembed, istride, idist,
              onembed, ostride, odist, CUFFT_C2R, batch) );

  CUFFT_SAFE_CALL( cufftExecC2R(handle,  m_c_data_D, m_r_data_D) );

  CUFFT_SAFE_CALL( cufftDestroy(handle) );

  if(!(start_row == 0 && row == row_len && col == col_len)) {
    cout << "====================================================================" << endl;
    cout << "decon_2D_tightROI_CUDA() : start_row :" << start_row << ", row_len : " << row_len << ", col_len : " << col_len << endl;
    cout << "====================================================================" << endl;
  }




  /*
  ker_restore_baseline_init<<<(row*col)/THREAD_PER_BLOCK + 1, THREAD_PER_BLOCK>>>(m_r_data_D, thrust::raw_pointer_cast(signal_dev_ptr), thrust::raw_pointer_cast(signal_flag_dev_ptr), row, col);
  CHECKLASTERROR;

  int iter_size = row / 4;
  for(int i=0 ; i<4 ; i++) {
    ker_restore_baseline_sort1<<<row, 1>>>(thrust::raw_pointer_cast(signal_dev_ptr), thrust::raw_pointer_cast(temp_row_dev_ptr), iter_size, col);
  }

  ker_restore_baseline_base<<<(row*col)/THREAD_PER_BLOCK + 1, THREAD_PER_BLOCK>>>(thrust::raw_pointer_cast(signal_dev_ptr), thrust::raw_pointer_cast(temp_row_dev_ptr), thrust::raw_pointer_cast(signal_flag_dev_ptr), row, col);
  CHECKLASTERROR;

  for(int i=0 ; i<4 ; i++) {
    ker_restore_baseline_sort2<<<row, 1>>>(thrust::raw_pointer_cast(signal_dev_ptr), thrust::raw_pointer_cast(temp_row_dev_ptr), thrust::raw_pointer_cast(signal_flag_dev_ptr), iter_size, col);
  }

  ker_restore_baseline_shift<<<(row*col)/THREAD_PER_BLOCK + 1, THREAD_PER_BLOCK>>>(m_r_data_D, thrust::raw_pointer_cast(temp_row_dev_ptr), row, col);
  CHECKLASTERROR;
  */

  CUDA_SAFE_CALL( cudaMemcpy(r_data, m_r_data_D, row*col*sizeof(float), cudaMemcpyDeviceToHost) );
  //cudaDeviceSynchronize();


}

#endif



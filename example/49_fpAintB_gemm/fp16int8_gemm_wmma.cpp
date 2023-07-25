// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2023, Advanced Micro Devices, Inc. All rights reserved.

#include "common.hpp"

#include "ck/tensor_operation/gpu/device/impl/device_fpAintB_gemm_wmma.hpp"

using ADataType        = ck::half_t;
using BDataType        = int8_t;
using ScaleDataType    = ck::half_t;
using AccDataType      = float;
using CShuffleDataType = float;
using CDataType        = ck::half_t;

using ALayout = Row;
using BLayout = Col;
using CLayout = Row;

using AElementOp = PassThrough;
using BElementOp = PassThrough;
using CElementOp = PassThrough;

static constexpr auto GemmDefault = ck::tensor_operation::device::GemmSpecialization::MNKPadding;

// clang-format off
using DeviceGemmInstance = ck::tensor_operation::device::DeviceFpAintBGemm_Wmma_CShuffle
         < ALayout,             
           BLayout,             
           CLayout,             
           ADataType, 
           BDataType,
           ScaleDataType,
           CDataType, 
           AccDataType, 
           CShuffleDataType,  
           AElementOp,  
           BElementOp,  
           CElementOp,    
           GemmDefault, 
           1,           // Prefetch stage
           128,         // BlockSize
           128,         // MPerBlock
           64,          // NPerBlock
           64,          // KPerBlock
           8,           // K1
           16,          // MPerWmma
           16,          // NPerWmma
           4,           // M-Repeat // M-PerWmma / M-Repeat = M-Wave
           2,           // N-Repeat // N-PerWmma / N-Repeat = N-Wave
           S<4, 32, 1>,     
           S<1, 0, 2>,     
           S<1, 0, 2>,              
           2,              
           8,              
           8,      
           true,     
           S<4, 32, 1>,     
           S<1, 0, 2>,     
           S<1, 0, 2>,             
           2,              
           8,              
           8,      
           true,           
           1,           // C shuffle (M Repeat) Per store
           1,           // C shuffle (N Repeat) Per store
           S<1, 32, 1,  4>,               
           8>;
// clang-format on

using ReferenceGemmInstance = ck::tensor_operation::host::ReferencefpAintBGemm<ADataType,
                                                                               BDataType,
                                                                               ScaleDataType,
                                                                               CDataType,
                                                                               AccDataType,
                                                                               AElementOp,
                                                                               BElementOp,
                                                                               CElementOp>;

#include "run_gemm_example.inc"

int main(int argc, char* argv[]) { return !run_gemm_example(argc, argv); }

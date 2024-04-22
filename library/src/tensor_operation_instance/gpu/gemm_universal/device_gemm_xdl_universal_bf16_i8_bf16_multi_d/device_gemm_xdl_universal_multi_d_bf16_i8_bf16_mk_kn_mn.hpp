// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2023, Advanced Micro Devices, Inc. All rights reserved.

#include "ck/ck.hpp"
#include "ck/tensor_operation/gpu/device/tensor_layout.hpp"
#include "ck/tensor_operation/gpu/device/gemm_specialization.hpp"
#include "ck/tensor_operation/gpu/device/impl/device_gemm_multiple_d_xdl_cshuffle_v3.hpp"

#include "ck/tensor_operation/gpu/element/element_wise_operation.hpp"
#include "ck/library/tensor_operation_instance/add_device_operation_instance.hpp"

namespace ck {
namespace tensor_operation {
namespace device {
namespace instance {

using I8  = int8_t;
using BF16 = bhalf_t;
using F32 = float;

using Row = tensor_layout::gemm::RowMajor;
using Col = tensor_layout::gemm::ColumnMajor;

template <index_t... Is>
using S = Sequence<Is...>;

using PassThrough = element_wise::PassThrough;
using MultiplyAddFastGelu = element_wise::MultiplyAddFastGelu;

static constexpr auto GemmDefault    = GemmSpecialization::Default;
static constexpr auto GemmKPadding   = GemmSpecialization::KPadding;
static constexpr auto GemmMNPadding  = GemmSpecialization::MNPadding;
static constexpr auto GemmMNKPadding = GemmSpecialization::MNKPadding;

static constexpr auto Intrawave = BlockGemmPipelineScheduler::Intrawave;
static constexpr auto Interwave = BlockGemmPipelineScheduler::Interwave;

using DsLayout = ck::Tuple<Row, Row>;

template <
	typename DsDType,
	 typename CElementwiseOp,
GemmSpecialization GemmSpec
	>
using device_gemm_xdl_universal_multi_d_bf16_i8_bf16_mk_kn_mn_comp_instances = std::tuple<
    // clang-format off
        //#########################| ALayout| BLayout| CLayout|AData| BData|  DsData| CData| AccData| Cshuffle|           A|           B|               C|           GEMM| Block|  MPer|  NPer|  KPer| AK1| BK1|MPer| NPer| MXdl| NXdl|  ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockLds|  BBlockTransfer| BBlockTransfer| BBlockTransfer| BlockTransfer| BBlockTransfer| BBlockTransfer| BBlockLds|    CShuffle|    CShuffle|     CBlockTransferClusterLengths|  CBlockTransfer|                         Block-wiseGemm|               Block-wiseGemm|
        //#########################|        |        |        | Type|  Type|    Type|  Type|    Type|     Type| Elementwise| Elementwise|     Elementwise| Specialization|  Size| Block| Block| Block|    |    | XDL|  XDL|  Per|  Per|   ThreadCluster|  ThreadCluster| SrcAccessOrder|   SrcVectorDim|      SrcScalar|      DstScalar| AddExtraM|   ThreadCluster|  ThreadCluster| SrcAccessOrder|  SrcVectorDim|      SrcScalar|      DstScalar| AddExtraN| MXdlPerWave| NXdlPerWave| _MBlock_MXdlPerWave_MWaveMPerXdl| ScalarPerVector|                               Pipeline|                     Pipeline|
        //#########################|        |        |        |     |      |        |      |        |         |   Operation|   Operation|       Operation|               |      |      |      |      |    |    |    |     | Wave| Wave| Lengths_K0_M_K1|   ArrangeOrder|               |               |      PerVector|   PerVector_K1|          | Lengths_K0_N_K1|   ArrangeOrder|               |              |      PerVector|   PerVector_K1|          |  PerShuffle|  PerShuffle| _NBlock_NXdlPerWave_NWaveNPerXdl|   _NWaveNPerXdl|                              Scheduler|                     Verision|
        //#########################|        |        |        |     |      |        |      |        |         |            |            |                |               |      |      |      |      |    |    |    |     |     |     |                |               |               |               |               |               |          |                |               |               |              |               |               |          |            |            |                                 |                |                                       |                             |
                                                                                    
   DeviceGemmMultiD_Xdl_CShuffle_V3<       Row,     Row, DsLayout,    Row,  BF16,    I8, DsDType,   BF16,     F32,      F32,  PassThrough, PassThrough, CElementwiseOp,       GemmSpec,   256,   256,   256,    32,   8,   4,  32,   32,    4,    4,     S<4, 64, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,              8,              8,          0,    S<8, 32, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,               8,              4,          0,          1,           1,                   S<1, 32, 1, 8>,               8,  BlockGemmPipelineScheduler::Intrawave, BlockGemmPipelineVersion::v4>,
   DeviceGemmMultiD_Xdl_CShuffle_V3<       Row,     Row, DsLayout,    Row,  BF16,    I8, DsDType,   BF16,     F32,      F32,  PassThrough, PassThrough, CElementwiseOp,       GemmSpec,   256,   128,   128,    64,   8,   4,  32,   32,    2,    2,     S<8, 32, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,              8,              8,          0,    S<16, 16, 1>,    S<0, 2, 1>,     S<0, 2, 1>,             1,               8,              4,          0,          1,           1,                   S<1, 32, 1, 8>,               8,  BlockGemmPipelineScheduler::Intrawave, BlockGemmPipelineVersion::v4>,
   DeviceGemmMultiD_Xdl_CShuffle_V3<       Row,     Row, DsLayout,    Row,  BF16,    I8, DsDType,   BF16,     F32,      F32,  PassThrough, PassThrough, CElementwiseOp,       GemmSpec,   256,   256,   256,    32,   8,   4,  32,   32,    4,    4,     S<4, 64, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,              8,              8,          0,    S<8, 32, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,               8,              4,          0,          1,           1,                   S<1, 32, 1, 8>,               8,  BlockGemmPipelineScheduler::Intrawave, BlockGemmPipelineVersion::v5>,
   DeviceGemmMultiD_Xdl_CShuffle_V3<       Row,     Row, DsLayout,    Row,  BF16,    I8, DsDType,   BF16,     F32,      F32,  PassThrough, PassThrough, CElementwiseOp,       GemmSpec,   256,   256,   256,    32,   8,   4,  32,   32,    4,    4,     S<4, 64, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,              8,              8,          0,    S<8, 32, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,               8,              4,          0,          1,           1,                   S<1, 32, 1, 8>,               8,  BlockGemmPipelineScheduler::Intrawave, BlockGemmPipelineVersion::v3>,
   DeviceGemmMultiD_Xdl_CShuffle_V3<       Row,     Row, DsLayout,    Row,  BF16,    I8, DsDType,   BF16,     F32,      F32,  PassThrough, PassThrough, CElementwiseOp,       GemmSpec,   256,   224,   256,    64,   8,   4,  16,   16,    7,    8,     S<8, 32, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,              8,              8,          0,    S<16, 16, 1>,    S<0, 2, 1>,     S<0, 2, 1>,             1,              16,              4,          0,          1,           2,                   S<1, 32, 1, 8>,               8,  BlockGemmPipelineScheduler::Intrawave, BlockGemmPipelineVersion::v3>,
   DeviceGemmMultiD_Xdl_CShuffle_V3<       Row,     Row, DsLayout,    Row,  BF16,    I8, DsDType,   BF16,     F32,      F32,  PassThrough, PassThrough, CElementwiseOp,       GemmSpec,   256,   128,   128,    64,   8,   4,  32,   32,    2,    2,     S<8, 32, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,              8,              8,          0,    S<16, 16, 1>,    S<0, 2, 1>,     S<0, 2, 1>,             1,               8,              4,          0,          1,           1,                   S<1, 32, 1, 8>,               8,  BlockGemmPipelineScheduler::Intrawave, BlockGemmPipelineVersion::v3>,
   DeviceGemmMultiD_Xdl_CShuffle_V3<       Row,     Row, DsLayout,    Row,  BF16,    I8, DsDType,   BF16,     F32,      F32,  PassThrough, PassThrough, CElementwiseOp,       GemmSpec,   256,   128,   256,    32,   8,   4,  32,   32,    2,    4,     S<4, 64, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,              8,              8,          0,    S<8, 32, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,               8,              4,          0,          1,           1,                   S<1, 32, 1, 8>,               8,  BlockGemmPipelineScheduler::Interwave, BlockGemmPipelineVersion::v1>,
   DeviceGemmMultiD_Xdl_CShuffle_V3<       Row,     Row, DsLayout,    Row,  BF16,    I8, DsDType,   BF16,     F32,      F32,  PassThrough, PassThrough, CElementwiseOp,       GemmSpec,   256,   128,   128,    64,   8,   4,  32,   32,    2,    2,     S<8, 32, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,              8,              8,          0,    S<16, 16, 1>,    S<0, 2, 1>,     S<0, 2, 1>,             1,               8,              4,          0,          1,           1,                   S<1, 32, 1, 8>,               8,  BlockGemmPipelineScheduler::Interwave, BlockGemmPipelineVersion::v1>
    // clang-format on
    >;

template <
	typename DsDType,
	 typename CElementwiseOp,
	GemmSpecialization GemmSpec,
BlockGemmPipelineScheduler BlkGemmPipeSched
	>
using device_gemm_xdl_universal_multi_d_bf16_i8_bf16_mk_kn_mn_mem_instances = std::tuple<
    // clang-format off
        //#########################| ALayout| BLayout| CLayout|AData| BData|  DsData| CData| AccData| Cshuffle|           A|           B|              C|          GEMM| Block|  MPer|  NPer|  KPer| AK1| BK1|MPer| NPer| MXdl| NXdl|  ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockLds|  BBlockTransfer| BBlockTransfer| BBlockTransfer| BlockTransfer| BBlockTransfer| BBlockTransfer| BBlockLds|    CShuffle|    CShuffle|     CBlockTransferClusterLengths|  CBlockTransfer|    Block-wiseGemm|               Block-wiseGemm|
        //#########################|        |        |        | Type|  Type|    Type|  Type|    Type|     Type| Elementwise| Elementwise|    Elementwise|Specialization|  Size| Block| Block| Block|    |    | XDL|  XDL|  Per|  Per|   ThreadCluster|  ThreadCluster| SrcAccessOrder|   SrcVectorDim|      SrcScalar|      DstScalar| AddExtraM|   ThreadCluster|  ThreadCluster| SrcAccessOrder|  SrcVectorDim|      SrcScalar|      DstScalar| AddExtraN| MXdlPerWave| NXdlPerWave| _MBlock_MXdlPerWave_MWaveMPerXdl| ScalarPerVector|          Pipeline|                     Pipeline|
        //#########################|        |        |        |     |      |        |      |        |         |   Operation|   Operation|      Operation|              |      |      |      |      |    |    |    |     | Wave| Wave| Lengths_K0_M_K1|   ArrangeOrder|               |               |      PerVector|   PerVector_K1|          | Lengths_K0_N_K1|   ArrangeOrder|               |              |      PerVector|   PerVector_K1|          |  PerShuffle|  PerShuffle| _NBlock_NXdlPerWave_NWaveNPerXdl|   _NWaveNPerXdl|         Scheduler|                     Verision|
        //#########################|        |        |        |     |      |        |      |        |         |            |            |               |              |      |      |      |      |    |    |    |     |     |     |                |               |               |               |               |               |          |                |               |               |              |               |               |          |            |            |                                 |                |                  |                             |

   // Latency friendly
   DeviceGemmMultiD_Xdl_CShuffle_V3<  Row,     Row,  DsLayout,   Row,     BF16,   I8,    DsDType, BF16,   F32,     F32,      PassThrough, PassThrough, CElementwiseOp,       GemmSpec,    64,    16,   16,   256,   8,   4,  16,   16,    1,    1,     S<32, 2, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,              8,              8,          0,    S<64, 1, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              16,              4,          0,          1,           1,                   S<1, 16, 1, 4>,               4,  BlkGemmPipeSched, BlockGemmPipelineVersion::v1>,
   DeviceGemmMultiD_Xdl_CShuffle_V3<  Row,     Row,  DsLayout,   Row,     BF16,   I8,    DsDType, BF16,   F32,     F32,      PassThrough, PassThrough, CElementwiseOp,       GemmSpec,   128,    16,   32,   256,   8,   4,  16,   16,    1,    1,     S<32, 4, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,              8,              8,          0,    S<64, 2, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              16,              4,          0,          1,           1,                   S<1, 16, 1, 8>,               4,  BlkGemmPipeSched, BlockGemmPipelineVersion::v1>,
   // Memory friendly
   DeviceGemmMultiD_Xdl_CShuffle_V3<  Row,     Row,  DsLayout,   Row,     BF16,   I8,    DsDType, BF16,   F32,     F32,      PassThrough, PassThrough, CElementwiseOp,       GemmSpec,    64,    16,   16,   256,   8,   4,  16,   16,    1,    1,     S<32, 2, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,              8,              8,          0,    S<64, 1, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              16,              4,          0,          1,           1,                   S<1, 16, 1, 4>,               4,  BlkGemmPipeSched, BlockGemmPipelineVersion::v2>,
   DeviceGemmMultiD_Xdl_CShuffle_V3<  Row,     Row,  DsLayout,   Row,     BF16,   I8,    DsDType, BF16,   F32,     F32,      PassThrough, PassThrough, CElementwiseOp,       GemmSpec,   128,    16,   32,   256,   8,   4,  16,   16,    1,    1,     S<32, 4, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,              8,              8,          0,    S<64, 2, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              16,              4,          0,          1,           1,                   S<1, 16, 1, 8>,               4,  BlkGemmPipeSched, BlockGemmPipelineVersion::v2>,
   DeviceGemmMultiD_Xdl_CShuffle_V3<  Row,     Row,  DsLayout,   Row,     BF16,   I8,    DsDType, BF16,   F32,     F32,      PassThrough, PassThrough, CElementwiseOp,       GemmSpec,   128,    16,   64,   128,   8,   4,  16,   16,    1,    2,     S<16, 8, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,              8,              8,          0,    S<32, 4, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              16,              4,          0,          1,           1,                   S<1, 16, 1, 8>,               4,  BlkGemmPipeSched, BlockGemmPipelineVersion::v2>,
   DeviceGemmMultiD_Xdl_CShuffle_V3<  Row,     Row,  DsLayout,   Row,     BF16,   I8,    DsDType, BF16,   F32,     F32,      PassThrough, PassThrough, CElementwiseOp,       GemmSpec,   128,    32,   64,   128,   8,   4,  32,   32,    1,    1,     S<16, 8, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,              8,              8,          0,    S<32, 4, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              16,              4,          0,          1,           1,                   S<1, 16, 1, 8>,               8,  BlkGemmPipeSched, BlockGemmPipelineVersion::v2>,
   DeviceGemmMultiD_Xdl_CShuffle_V3<  Row,     Row,  DsLayout,   Row,     BF16,   I8,    DsDType, BF16,   F32,     F32,      PassThrough, PassThrough, CElementwiseOp,       GemmSpec,   128,    16,  128,    64,   8,   4,  16,   16,    1,    4,     S<8, 16, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,              8,              8,          0,    S<16, 8, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              16,              4,          0,          1,           1,                   S<1, 16, 1, 8>,               4,  BlkGemmPipeSched, BlockGemmPipelineVersion::v2>,
   DeviceGemmMultiD_Xdl_CShuffle_V3<  Row,     Row,  DsLayout,   Row,     BF16,   I8,    DsDType, BF16,   F32,     F32,      PassThrough, PassThrough, CElementwiseOp,       GemmSpec,   128,    32,  128,    64,   8,   4,  32,   32,    1,    2,     S<8, 16, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,              8,              8,          0,    S<16, 8, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              16,              4,          0,          1,           1,                   S<1, 16, 1, 8>,               8,  BlkGemmPipeSched, BlockGemmPipelineVersion::v2>,
   DeviceGemmMultiD_Xdl_CShuffle_V3<  Row,     Row,  DsLayout,   Row,     BF16,   I8,    DsDType, BF16,   F32,     F32,      PassThrough, PassThrough, CElementwiseOp,       GemmSpec,   256,    16,  256,    64,   8,   4,  16,   16,    1,    4,     S<8, 16, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,              8,              8,          0,    S<16, 16, 1>,    S<0, 2, 1>,     S<0, 2, 1>,             1,              16,              4,          0,          1,           1,                   S<1, 16, 1, 16>,              4,  BlkGemmPipeSched, BlockGemmPipelineVersion::v2>,
   DeviceGemmMultiD_Xdl_CShuffle_V3<  Row,     Row,  DsLayout,   Row,     BF16,   I8,    DsDType, BF16,   F32,     F32,      PassThrough, PassThrough, CElementwiseOp,       GemmSpec,   256,    32,  256,    64,   8,   4,  32,   32,    1,    2,     S<8, 32, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,              8,              8,          0,    S<16, 16, 1>,    S<0, 2, 1>,     S<0, 2, 1>,             1,              16,              4,          0,          1,           1,                   S<1, 16, 1, 16>,              8,  BlkGemmPipeSched, BlockGemmPipelineVersion::v2>
    // clang-format on
    >;
} // namespace instance
} // namespace device
} // namespace tensor_operation
} // namespace ck

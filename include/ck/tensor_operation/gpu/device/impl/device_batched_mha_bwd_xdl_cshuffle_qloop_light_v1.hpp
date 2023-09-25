// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2023, Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include <iostream>
#include <sstream>
#include <numeric>

#include "ck/utility/common_header.hpp"
#include "ck/utility/philox_rand.hpp"
#include "ck/tensor_description/tensor_descriptor.hpp"
#include "ck/tensor_description/tensor_descriptor_helper.hpp"
#include "ck/tensor_operation/gpu/device/device_base.hpp"
#include "ck/tensor_operation/gpu/device/gemm_specialization.hpp"
#include "ck/tensor_operation/gpu/device/masking_specialization.hpp"
#include "ck/tensor_operation/gpu/device/matrix_padder.hpp"
#include "ck/tensor_operation/gpu/device/tensor_layout.hpp"
#include "ck/tensor_operation/gpu/grid/gridwise_batched_mha_bwd_xdl_cshuffle_qloop_b2t_light_v1.hpp"
#include "ck/tensor_operation/gpu/grid/gridwise_batched_mha_bwd_xdl_cshuffle_qloop_ydotygrad.hpp"
#include "ck/tensor_operation/operator_transform/transform_contraction_to_gemm.hpp"
#include "ck/host_utility/device_prop.hpp"
#include "ck/host_utility/kernel_launch.hpp"

namespace ck {
namespace tensor_operation {
namespace device {

template <typename GridwiseGemm,
          typename InputDataType,
          typename DDataType,
          typename YGridDescriptor_MBlock_MPerBlock_NBlock_NPerBlock,
          typename DGridDescriptor_M,
          typename Block2CTileMap,
          typename ComputeBasePtrOfStridedBatch>
__global__ void
#if CK_USE_LAUNCH_BOUNDS
    __launch_bounds__(CK_MAX_THREAD_PER_BLOCK, /*CK_MIN_BLOCK_PER_CU*/ 1)
#endif
        kernel_batched_multihead_attention_backward_ydotygrad_v1(
            const InputDataType* __restrict__ p_y_grid,
            const InputDataType* __restrict__ p_ygrad_grid,
            DDataType* __restrict__ p_d_grid,
            const YGridDescriptor_MBlock_MPerBlock_NBlock_NPerBlock
                y_grid_desc_mblock_mperblock_nblock_nperblock,
            const DGridDescriptor_M d_grid_desc_m,
            const Block2CTileMap block_2_ctile_map,
            const index_t batch_count,
            const ComputeBasePtrOfStridedBatch compute_base_ptr_of_batch,
            const float p_drop)
{
#if(!defined(__HIP_DEVICE_COMPILE__) || defined(__gfx908__) || defined(__gfx90a__) || \
    defined(__gfx940__) || defined(__gfx941__) || defined(__gfx942__))
    const index_t num_blocks_per_batch =
        __builtin_amdgcn_readfirstlane(get_grid_size() / batch_count);
    const index_t g_idx = __builtin_amdgcn_readfirstlane(get_block_1d_id() / num_blocks_per_batch);

    // NOTE: assumes QKVY has the same layout as dQ/dK/dV/dY therefore being able to reuse batch
    // offsets
    const long_index_t c_batch_offset = __builtin_amdgcn_readfirstlane(
        static_cast<long_index_t>(compute_base_ptr_of_batch.GetCBasePtr(g_idx)));
    const long_index_t d_batch_offset = __builtin_amdgcn_readfirstlane(
        static_cast<long_index_t>(compute_base_ptr_of_batch.GetLSEBasePtr(g_idx)));

    GridwiseGemm::Run(p_y_grid + c_batch_offset,
                      p_ygrad_grid + c_batch_offset,
                      p_d_grid + d_batch_offset,
                      y_grid_desc_mblock_mperblock_nblock_nperblock,
                      d_grid_desc_m,
                      block_2_ctile_map,
                      p_drop);

#else
    ignore = p_y_grid;
    ignore = p_ygrad_grid;
    ignore = p_d_grid;
    ignore = y_grid_desc_mblock_mperblock_nblock_nperblock;
    ignore = d_grid_desc_m;
    ignore = block_2_ctile_map;
    ignore = batch_count;
    ignore = compute_base_ptr_of_batch;
    ignore = p_drop;
#endif // end of if (defined(__gfx908__) || defined(__gfx90a__))
}

template <typename GridwiseGemm,
          typename InputDataType,
          typename D0DataType,
          typename OutputDataType,
          typename ZDataType,
          typename LSEDataType,
          typename DDataType,
          typename AElementwiseOperation,
          typename BElementwiseOperation,
          typename AccElementwiseOperation,
          typename B1ElementwiseOperation,
          typename CElementwiseOperation,
          typename AGridDesc_AK0_M_AK1,
          typename BGridDesc_BK0_N_BK1,
          typename D0GridDescriptor_M0_N0_M1_M2_N1_M3,
          typename ZGridDescriptor_M0_N0_M1_N1_M2_N2_M3_M4_M5_N3,
          typename B1GridDesc_BK0_N_BK1,
          typename LSEGridDescriptor_M,
          typename YGradGridDesc_O0_M_O1,
          typename Block2CTileMap,
          typename ComputeBasePtrOfStridedBatch,
          typename C0MatrixMask,
          bool HasMainKBlockLoop,
          bool IsDropout,
          bool Deterministic>
__global__ void
#if CK_USE_LAUNCH_BOUNDS
    __launch_bounds__(CK_MAX_THREAD_PER_BLOCK, /*CK_MIN_BLOCK_PER_CU*/ 1)
#endif
        kernel_batched_multihead_attention_backward_qloop_xdl_cshuffle_light_v1(
            const InputDataType* __restrict__ p_a_grid,
            const InputDataType* __restrict__ p_b_grid,
            const D0DataType* __restrict__ p_d0_grid,
            ZDataType* __restrict__ p_z_grid,
            const InputDataType* __restrict__ p_b1_grid,
            const LSEDataType* __restrict__ p_lse_grid,
            const DDataType* __restrict__ p_d_grid,
            const InputDataType* __restrict__ p_ygrad_grid,
            OutputDataType* __restrict__ p_qgrad_grid,
            OutputDataType* __restrict__ p_kgrad_grid,
            D0DataType* __restrict__ p_d0grad_grid,
            OutputDataType* __restrict__ p_vgrad_grid,
            const AElementwiseOperation a_element_op,
            const BElementwiseOperation b_element_op,
            const AccElementwiseOperation acc_element_op,
            const B1ElementwiseOperation b1_element_op,
            const CElementwiseOperation c_element_op,
            const AGridDesc_AK0_M_AK1 a_grid_desc_ak0_m_ak1,
            const BGridDesc_BK0_N_BK1 b_grid_desc_bk0_n_bk1,
            const D0GridDescriptor_M0_N0_M1_M2_N1_M3 d0_grid_desc_m0_n0_m1_m2_n1_m3,
            const ZGridDescriptor_M0_N0_M1_N1_M2_N2_M3_M4_M5_N3
                c_grid_desc_m0_n0_m1_n1_m2_n2_m3_m4_m5_n3,
            const B1GridDesc_BK0_N_BK1 b1_grid_desc_bk0_n_bk1,
            const LSEGridDescriptor_M lse_grid_desc_m,
            const YGradGridDesc_O0_M_O1 ygrad_grid_desc_o0_m_o1,
            const Block2CTileMap block_2_ctile_map,
            const index_t batch_count,
            const index_t nblock,
            const ComputeBasePtrOfStridedBatch compute_base_ptr_of_batch,
            const C0MatrixMask c0_matrix_mask,
            const float p_drop,
            const unsigned long long seed,
            const unsigned long long offset,
            const index_t raw_m_padded,
            const index_t raw_n_padded)
{
#if(!defined(__HIP_DEVICE_COMPILE__) || defined(__gfx908__) || defined(__gfx90a__) || \
    defined(__gfx940__) || defined(__gfx941__) || defined(__gfx942__))
    __shared__ char p_shared[GridwiseGemm::GetSharedMemoryNumberOfByte()];
    const index_t num_blocks_per_batch =
        __builtin_amdgcn_readfirstlane(get_grid_size() / batch_count);
    const index_t g_idx = __builtin_amdgcn_readfirstlane(get_block_1d_id() / num_blocks_per_batch);

    // NOTE: assumes QKVY has the same layout as dQ/dK/dV/dY therefore being able to reuse batch
    // offsets
    const long_index_t a_batch_offset = __builtin_amdgcn_readfirstlane(
        static_cast<long_index_t>(compute_base_ptr_of_batch.GetABasePtr(g_idx)));
    const long_index_t b_batch_offset = __builtin_amdgcn_readfirstlane(
        static_cast<long_index_t>(compute_base_ptr_of_batch.GetBBasePtr(g_idx)));
    const long_index_t z_batch_offset = __builtin_amdgcn_readfirstlane(
        static_cast<long_index_t>(compute_base_ptr_of_batch.GetZBasePtr(g_idx)));
    const long_index_t b1_batch_offset = __builtin_amdgcn_readfirstlane(
        static_cast<long_index_t>(compute_base_ptr_of_batch.GetB1BasePtr(g_idx)));
    const long_index_t c_batch_offset = __builtin_amdgcn_readfirstlane(
        static_cast<long_index_t>(compute_base_ptr_of_batch.GetCBasePtr(g_idx)));
    const long_index_t lse_batch_offset = __builtin_amdgcn_readfirstlane(
        static_cast<long_index_t>(compute_base_ptr_of_batch.GetLSEBasePtr(g_idx)));

    ck::philox ph(seed, 0, offset);
    ZDataType* z_matrix_ptr = (p_z_grid == nullptr ? nullptr : p_z_grid + z_batch_offset);

    const index_t z_random_matrix_offset = g_idx * raw_m_padded * raw_n_padded;

    const D0DataType* tmp_p_d0_grid = nullptr;
    D0DataType* tmp_p_d0grad_grid   = nullptr;
    if constexpr(!is_same<D0DataType, void>::value)
    {
        const long_index_t d0_batch_offset = __builtin_amdgcn_readfirstlane(
            static_cast<long_index_t>(compute_base_ptr_of_batch.GetD0BasePtr(g_idx)));
        if(p_d0_grid != nullptr)
        {
            tmp_p_d0_grid = p_d0_grid + d0_batch_offset;
        }
        if(p_d0grad_grid != nullptr)
        {
            tmp_p_d0grad_grid = p_d0grad_grid + d0_batch_offset;
        }
    }
    if constexpr(Deterministic)
    {
        for(index_t i = 0; i < nblock; i++)
        {
            GridwiseGemm::template Run<HasMainKBlockLoop, IsDropout>(
                p_a_grid + a_batch_offset,
                p_b_grid + b_batch_offset,
                tmp_p_d0_grid,
                z_matrix_ptr,
                p_b1_grid + b1_batch_offset,
                p_lse_grid + lse_batch_offset,
                p_d_grid + lse_batch_offset,
                p_ygrad_grid + c_batch_offset,
                p_qgrad_grid + a_batch_offset,
                p_kgrad_grid + b_batch_offset,
                tmp_p_d0grad_grid,
                p_vgrad_grid + b1_batch_offset,
                p_shared,
                a_element_op,
                b_element_op,
                acc_element_op,
                b1_element_op,
                c_element_op,
                a_grid_desc_ak0_m_ak1,
                b_grid_desc_bk0_n_bk1,
                d0_grid_desc_m0_n0_m1_m2_n1_m3,
                c_grid_desc_m0_n0_m1_n1_m2_n2_m3_m4_m5_n3,
                b1_grid_desc_bk0_n_bk1,
                lse_grid_desc_m,
                ygrad_grid_desc_o0_m_o1,
                block_2_ctile_map,
                c0_matrix_mask,
                p_drop,
                ph,
                z_random_matrix_offset,
                raw_n_padded,
                i);
        }
    }
    else
    {
        GridwiseGemm::template Run<HasMainKBlockLoop, IsDropout>(
            p_a_grid + a_batch_offset,
            p_b_grid + b_batch_offset,
            tmp_p_d0_grid,
            z_matrix_ptr,
            p_b1_grid + b1_batch_offset,
            p_lse_grid + lse_batch_offset,
            p_d_grid + lse_batch_offset,
            p_ygrad_grid + c_batch_offset,
            p_qgrad_grid + a_batch_offset,
            p_kgrad_grid + b_batch_offset,
            tmp_p_d0grad_grid,
            p_vgrad_grid + b1_batch_offset,
            p_shared,
            a_element_op,
            b_element_op,
            acc_element_op,
            b1_element_op,
            c_element_op,
            a_grid_desc_ak0_m_ak1,
            b_grid_desc_bk0_n_bk1,
            d0_grid_desc_m0_n0_m1_m2_n1_m3,
            c_grid_desc_m0_n0_m1_n1_m2_n2_m3_m4_m5_n3,
            b1_grid_desc_bk0_n_bk1,
            lse_grid_desc_m,
            ygrad_grid_desc_o0_m_o1,
            block_2_ctile_map,
            c0_matrix_mask,
            p_drop,
            ph,
            z_random_matrix_offset,
            raw_n_padded,
            0);
    }
#else
    ignore = p_a_grid;
    ignore = p_b_grid;
    ignore = p_d0_grid;
    ignore = p_z_grid;
    ignore = p_b1_grid;
    ignore = p_lse_grid;
    ignore = p_d_grid;
    ignore = p_ygrad_grid;
    ignore = p_qgrad_grid;
    ignore = p_kgrad_grid;
    ignore = p_d0grad_grid;
    ignore = p_vgrad_grid;
    ignore = a_element_op;
    ignore = b_element_op;
    ignore = acc_element_op;
    ignore = b1_element_op;
    ignore = c_element_op;
    ignore = a_grid_desc_ak0_m_ak1;
    ignore = b_grid_desc_bk0_n_bk1;
    ignore = d0_grid_desc_m0_n0_m1_m2_n1_m3;
    ignore = c_grid_desc_m0_n0_m1_n1_m2_n2_m3_m4_m5_n3;
    ignore = b1_grid_desc_bk0_n_bk1;
    ignore = lse_grid_desc_m;
    ignore = ygrad_grid_desc_o0_m_o1;
    ignore = block_2_ctile_map;
    ignore = batch_count;
    ignore = nblock;
    ignore = compute_base_ptr_of_batch;
    ignore = c0_matrix_mask;
    ignore = p_drop;
    ignore = seed;
    ignore = offset;
    ignore = raw_m_padded;
    ignore = raw_n_padded;
#endif // end of if (defined(__gfx908__) || defined(__gfx90a__))
}

// Computes C = A * B0 * B1
//              ^^^^^^ (Acc0)
//              ^^^^^^^^^^^ (Acc1)
template <index_t NumDimG,
          index_t NumDimM,
          index_t NumDimN,
          index_t NumDimK,
          index_t NumDimO, // NumDimGemm1N
          typename InputDataType,
          typename OutputDataType,
          typename GemmDataType,
          typename ZDataType,
          typename LSEDataType,
          typename DDataType,
          typename Acc0BiasDataType,
          typename Acc1BiasDataType,
          typename GemmAccDataType,
          typename CShuffleDataType,
          typename AElementwiseOperation,
          typename BElementwiseOperation,
          typename AccElementwiseOperation,
          typename B1ElementwiseOperation,
          typename CElementwiseOperation,
          GemmSpecialization GemmSpec,
          TensorSpecialization ASpec,
          TensorSpecialization BSpec,
          TensorSpecialization B1Spec,
          TensorSpecialization CSpec,
          index_t NumGemmKPrefetchStage,
          index_t BlockSize,
          index_t MPerBlock,
          index_t NPerBlock, // Gemm0NPerBlock
          index_t KPerBlock, // Gemm0KPerBlock
          index_t Gemm1NPerBlock,
          index_t Gemm1KPerBlock,
          index_t Gemm2KPerBlock,
          index_t AK1,
          index_t BK1,
          index_t B1K1,
          index_t MPerXDL,
          index_t NPerXDL,
          index_t MXdlPerWave,
          index_t NXdlPerWave,
          index_t Gemm1NXdlPerWave,
          index_t Gemm2NXdlPerWave,
          index_t DKPerBlock,
          typename ABlockTransferThreadClusterLengths_AK0_M_AK1,
          typename ABlockTransferThreadClusterArrangeOrder,
          typename ABlockTransferSrcAccessOrder,
          index_t ABlockTransferSrcVectorDim,
          index_t ABlockTransferSrcScalarPerVector,
          index_t ABlockTransferDstScalarPerVector_AK1,
          bool ABlockLdsExtraM,
          typename BBlockTransferThreadClusterLengths_BK0_N_BK1,
          typename BBlockTransferThreadClusterArrangeOrder,
          typename BBlockTransferSrcAccessOrder,
          index_t BBlockTransferSrcVectorDim,
          index_t BBlockTransferSrcScalarPerVector,
          index_t BBlockTransferDstScalarPerVector_BK1,
          bool BBlockLdsExtraN,
          index_t D0BlockTransferSrcScalarPerVector,
          index_t CShuffleMXdlPerWavePerShuffle,
          index_t CShuffleNXdlPerWavePerShuffle,
          typename CShuffleBlockTransferClusterLengths_MBlock_MPerBlock_NBlock_NPerBlock,
          index_t CShuffleBlockTransferScalarPerVector_NPerBlock,
          MaskingSpecialization MaskingSpec,
          bool Deterministic,
          LoopScheduler LoopSched = LoopScheduler::Default>
struct DeviceBatchedMultiheadAttentionBackward_Qloop_Xdl_CShuffle_Light_V1
    : public BaseOperator // TODO inherit atten bwd op once API stablizes
{
    static_assert(NumDimG > 0 && NumDimM > 0 && NumDimN > 0 && NumDimK > 0 && NumDimO > 0,
                  "Number of dimension must be greater than 0");

    using D0DataType                    = Acc0BiasDataType;
    using D1DataType                    = Acc1BiasDataType;
    static constexpr index_t DMPerBlock = BlockSize;

    // TODO: implement bias combination
    static_assert(std::is_void<D1DataType>::value, "Acc1 Bias addition is unimplemented");

    using DeviceOp = DeviceBatchedMultiheadAttentionBackward_Qloop_Xdl_CShuffle_Light_V1;

    static constexpr auto I0 = Number<0>{};
    static constexpr auto I1 = Number<1>{};
    static constexpr auto I2 = Number<2>{};

    static constexpr index_t V_O1 = BK1;
    static constexpr index_t Y_O1 = AK1;
    static constexpr index_t Y_M1 = B1K1;

    static constexpr auto padder = GemmGemmPadder<GemmSpec,
                                                  Number<MPerBlock>,
                                                  Number<NPerBlock>,
                                                  Number<KPerBlock>,
                                                  Number<Gemm1NPerBlock>>{};

    using Transform = TransformBatchedContractionContractionToBatchedGemmGemm<
        Sequence<NumDimG, NumDimM, NumDimN, NumDimK, NumDimO>,
        Sequence<MPerBlock, NPerBlock, KPerBlock, Gemm1NPerBlock>,
        GemmSpec,
        ASpec,
        BSpec,
        B1Spec,
        CSpec>;

    using DTransform = TransformBatchedContractionContractionToBatchedGemmGemm<
        Sequence<NumDimG, NumDimM, NumDimN, NumDimK, NumDimO>,
        Sequence<DMPerBlock, NPerBlock, KPerBlock, Gemm1NPerBlock>,
        GemmSpecialization::MNKOPadding,
        ASpec,
        BSpec,
        B1Spec,
        CSpec>;

    /*
    Descriptors for inputs:

      Q, K, V, Y, dY, per-row softmax stats

    Descriptors for outputs:

      dQ, dK, dV

    */

    // Q in Gemm A position
    static auto MakeAGridDescriptor_AK0_M_AK1(const std::vector<index_t>& a_gs_ms_ks_lengths,
                                              const std::vector<index_t>& a_gs_ms_ks_strides)
    {
        return Transform::MakeAGridDescriptor_AK0_M_AK1(
            Transform::MakeAGridDescriptor_M_K(a_gs_ms_ks_lengths, a_gs_ms_ks_strides),
            Number<AK1>{});
    }

    // K in Gemm B0 position
    static auto MakeBGridDescriptor_BK0_N_BK1(const std::vector<index_t>& b_gs_ns_ks_lengths,
                                              const std::vector<index_t>& b_gs_ns_ks_strides)
    {
        return Transform::MakeB0GridDescriptor_BK0_N_BK1(
            Transform::MakeB0GridDescriptor_N_K(b_gs_ns_ks_lengths, b_gs_ns_ks_strides),
            Number<BK1>{});
    }

    // V in Gemm B1 position
    static auto
    MakeB1GridDescriptor_BK0_N_BK1(const std::vector<index_t>& b1_gs_gemm1ns_gemm1ks_lengths,
                                   const std::vector<index_t>& b1_gs_gemm1ns_gemm1ks_strides)
    {
        return Transform::MakeB1GridDescriptor_BK0_N_BK1(
            Transform::MakeB1GridDescriptor_N_K(b1_gs_gemm1ns_gemm1ks_lengths,
                                                b1_gs_gemm1ns_gemm1ks_strides),
            Number<B1K1>{});
    }

    //
    // dV = P^T * dY
    //

    // VGrad in Gemm C position
    static auto MakeVGradGridDescriptor_N_O(const std::vector<index_t>& v_gs_os_ns_lengths,
                                            const std::vector<index_t>& v_gs_os_ns_strides)
    {
        // v_gs_os_ns -> vgrad_gs_ns_os. O dims last because output is row-major.
        // Here directly rearrange lengths/strides before constructing tensor descriptor to reduce
        // transformation overhead
        // TODO: This will be much easier when inputs are Gs, Ms, Ns, Os. So there's no need to
        // extract subsequence and shuffle them.
        const index_t num_dims = NumDimG + NumDimN + NumDimO;

        // 0, 1, .. NumDimG - 1
        std::vector<index_t> gs_ids(NumDimG);
        std::iota(gs_ids.begin(), gs_ids.end(), 0);

        // NumDimG, NumDimG + 1, ... NumDimG + NumDimO - 1
        std::vector<index_t> os_ids(NumDimO);
        std::iota(os_ids.begin(), os_ids.end(), NumDimG);

        // NumDimG + NumDimO, NumDimG + NumDimO + 1, ... NumDimG + NumDimO + NumDimN - 1
        std::vector<index_t> ns_ids(NumDimN);
        std::iota(ns_ids.begin(), ns_ids.end(), NumDimG + NumDimO);

        std::vector<index_t> ids_old2new;
        ids_old2new.insert(ids_old2new.end(), gs_ids.begin(), gs_ids.end());
        ids_old2new.insert(ids_old2new.end(), ns_ids.begin(), ns_ids.end());
        ids_old2new.insert(ids_old2new.end(), os_ids.begin(), os_ids.end());

        std::vector<index_t> v_gs_ns_os_lengths(num_dims), v_gs_ns_os_strides(num_dims);
        for(int i = 0; i < num_dims; i++)
        {
            index_t id_new        = ids_old2new[i];
            v_gs_ns_os_lengths[i] = v_gs_os_ns_lengths[id_new];
            v_gs_ns_os_strides[i] = v_gs_os_ns_strides[id_new];
        }

        const auto vgrad_desc_nraw_oraw =
            MakeGridDescriptorPair<NumDimG, NumDimN, NumDimO, TensorSpecialization::Default>(
                v_gs_ns_os_lengths, v_gs_ns_os_strides)
                .second;

        return PadTensorDescriptor(vgrad_desc_nraw_oraw,
                                   make_tuple(NPerBlock, Gemm1NPerBlock),
                                   Sequence<padder.PadN, padder.PadO>{});
    }

    template <typename YGridDesc_M_O>
    static auto MakeYGradGridDescriptor_M0_O_M1(const YGridDesc_M_O& ygrad_grid_desc_m_o)
    {
        const auto M = ygrad_grid_desc_m_o.GetLength(I0);
        const auto O = ygrad_grid_desc_m_o.GetLength(I1);

        const auto Y_M0 = M / Y_M1;

        return transform_tensor_descriptor(
            ygrad_grid_desc_m_o,
            make_tuple(make_unmerge_transform(make_tuple(Y_M0, Y_M1)),
                       make_pass_through_transform(O)),
            make_tuple(Sequence<0>{}, Sequence<1>{}),
            make_tuple(Sequence<0, 2>{}, Sequence<1>{}));
    }

    //
    // dP = dY * V^T
    //

    // YGrad in Gemm A position
    static auto MakeYGradGridDescriptor_O0_M_O1(const std::vector<index_t>& y_gs_ms_os_lengths,
                                                const std::vector<index_t>& y_gs_ms_os_strides)
    {
        return Transform::MakeAGridDescriptor_AK0_M_AK1(
            Transform::MakeAGridDescriptor_M_K(y_gs_ms_os_lengths, y_gs_ms_os_strides),
            Number<Y_O1>{});
    }

    // V in Gemm B position
    static auto MakeVGridDescriptor_O0_N_O1(const std::vector<index_t>& v_gs_os_ns_lengths,
                                            const std::vector<index_t>& v_gs_os_ns_strides)
    {
        // v_gs_os_ns -> vgrad_gs_ns_os. O dims last because output is row-major.
        // Here directly rearrange lengths/strides before constructing tensor descriptor to reduce
        // transformation overhead
        // TODO: This will be much easier when inputs are Gs, Ms, Ns, Os. So there's no need to
        // extract subsequence and shuffle them.
        const index_t num_dims = NumDimG + NumDimN + NumDimO;

        // 0, 1, .. NumDimG - 1
        std::vector<index_t> gs_ids(NumDimG);
        std::iota(gs_ids.begin(), gs_ids.end(), 0);

        // NumDimG, NumDimG + 1, ... NumDimG + NumDimO - 1
        std::vector<index_t> os_ids(NumDimO);
        std::iota(os_ids.begin(), os_ids.end(), NumDimG);

        // NumDimG + NumDimO, NumDimG + NumDimO + 1, ... NumDimG + NumDimO + NumDimN - 1
        std::vector<index_t> ns_ids(NumDimN);
        std::iota(ns_ids.begin(), ns_ids.end(), NumDimG + NumDimO);

        std::vector<index_t> ids_old2new;
        ids_old2new.insert(ids_old2new.end(), gs_ids.begin(), gs_ids.end());
        ids_old2new.insert(ids_old2new.end(), ns_ids.begin(), ns_ids.end());
        ids_old2new.insert(ids_old2new.end(), os_ids.begin(), os_ids.end());

        std::vector<index_t> v_gs_ns_os_lengths(num_dims), v_gs_ns_os_strides(num_dims);
        for(int i = 0; i < num_dims; i++)
        {
            index_t id_new        = ids_old2new[i];
            v_gs_ns_os_lengths[i] = v_gs_os_ns_lengths[id_new];
            v_gs_ns_os_strides[i] = v_gs_os_ns_strides[id_new];
        }

        const auto v_grid_desc_nraw_oraw =
            MakeGridDescriptorPair<NumDimG, NumDimN, NumDimO, TensorSpecialization::Default>(
                v_gs_ns_os_lengths, v_gs_ns_os_strides)
                .second;

        const auto v_grid_desc_n_o = PadTensorDescriptor(v_grid_desc_nraw_oraw,
                                                         make_tuple(NPerBlock, Gemm1NPerBlock),
                                                         Sequence<padder.PadN, padder.PadO>{});

        // N_O to O0_N_O1; to refactor
        return Transform::MakeB0GridDescriptor_BK0_N_BK1(v_grid_desc_n_o, Number<V_O1>{});
    }

    // Z in Gemm0 C position
    static auto MakeZGridDescriptor_M_N(const std::vector<index_t>& z_gs_ms_ns_lengths,
                                        const std::vector<index_t>& z_gs_ms_ns_strides)
    {
        return Transform::MakeC0GridDescriptor_M_N(z_gs_ms_ns_lengths, z_gs_ms_ns_strides);
    }

    static auto MakeLSEGridDescriptor_M(index_t MRaw)
    {
        const auto lse_grid_desc_mraw = make_naive_tensor_descriptor_packed(make_tuple(MRaw));

        const auto M    = math::integer_divide_ceil(MRaw, MPerBlock) * MPerBlock;
        const auto MPad = M - MRaw;

        if constexpr(GemmSpec == GemmSpecialization::MPadding ||
                     GemmSpec == GemmSpecialization::MNPadding ||
                     GemmSpec == GemmSpecialization::MKPadding ||
                     GemmSpec == GemmSpecialization::MNKPadding)
        {
            // pad M
            return transform_tensor_descriptor(lse_grid_desc_mraw,
                                               make_tuple(make_right_pad_transform(MRaw, MPad)),
                                               make_tuple(Sequence<0>{}),
                                               make_tuple(Sequence<0>{}));
        }
        else
        {
            // not pad M
            return lse_grid_desc_mraw;
        }
    }
    // D0 in Gemm0 C position
    static auto MakeD0GridDescriptor_M_N(const std::vector<index_t>& d_gs_ms_ns_lengths,
                                         const std::vector<index_t>& d_gs_ms_ns_strides)
    {
        return Transform::MakeC0GridDescriptor_M_N(d_gs_ms_ns_lengths, d_gs_ms_ns_strides);
    }

    static auto MakeDGridDescriptor_M(index_t MRaw)
    {
        const auto d_grid_desc_mraw = make_naive_tensor_descriptor_packed(make_tuple(MRaw));

        const auto M    = math::integer_divide_ceil(MRaw, DMPerBlock) * DMPerBlock;
        const auto MPad = M - MRaw;

        if constexpr(GemmSpec == GemmSpecialization::MPadding ||
                     GemmSpec == GemmSpecialization::MNPadding ||
                     GemmSpec == GemmSpecialization::MKPadding ||
                     GemmSpec == GemmSpecialization::MNKPadding)
        {
            // pad M
            return transform_tensor_descriptor(d_grid_desc_mraw,
                                               make_tuple(make_right_pad_transform(MRaw, MPad)),
                                               make_tuple(Sequence<0>{}),
                                               make_tuple(Sequence<0>{}));
        }
        else
        {
            // not pad M
            return d_grid_desc_mraw;
        }
    }

    using AGridDesc_AK0_M_AK1  = decltype(MakeAGridDescriptor_AK0_M_AK1({}, {}));
    using BGridDesc_BK0_N_BK1  = decltype(MakeBGridDescriptor_BK0_N_BK1({}, {}));
    using D0GridDesc_G_M_N     = decltype(Transform::MakeC0GridDescriptor_G_M_N({}, {}));
    using B1GridDesc_BK0_N_BK1 = decltype(MakeBGridDescriptor_BK0_N_BK1({}, {}));
    using YGridDesc_M_O        = decltype(Transform::MakeCGridDescriptor_M_N({}, {}));
    using LSEGridDesc_M        = decltype(MakeLSEGridDescriptor_M(1));
    using AGridDesc_G_M_K      = decltype(Transform::MakeAGridDescriptor_G_M_K({}, {}));
    using BGridDesc_G_N_K      = decltype(Transform::MakeB0GridDescriptor_G_N_K({}, {}));
    using B1GridDesc_G_N_K     = decltype(Transform::MakeB1GridDescriptor_G_N_K({}, {}));
    using CGridDesc_G_M_N      = decltype(Transform::MakeCGridDescriptor_G_M_N({}, {}));
    using ZGridDesc_G_M_N      = decltype(Transform::MakeC0GridDescriptor_G_M_N({}, {}));
    using DYGridDesc_M_O       = decltype(DTransform::MakeCGridDescriptor_M_N({}, {}));
    using DGridDesc_M          = decltype(MakeDGridDescriptor_M(1));

    using D0GridDesc_M_N        = decltype(MakeD0GridDescriptor_M_N({}, {}));
    using KGridDesc_N_K         = decltype(Transform::MakeB0GridDescriptor_N_K({}, {}));
    using YGradGridDesc_O0_M_O1 = decltype(MakeYGradGridDescriptor_O0_M_O1({}, {}));
    using ZGridDesc_M_N         = decltype(MakeZGridDescriptor_M_N({}, {}));

    constexpr static auto make_MaskOutPredicate()
    {
        if constexpr(MaskingSpec == MaskingSpecialization::MaskDisabled)
        {
            return MaskDisabledPredicate{};
        }
        else if constexpr(MaskingSpec == MaskingSpecialization::MaskUpperTriangleFromTopLeft)
        {
            return MaskUpperTriangleFromTopLeftPredicate{};
        }
        else if constexpr(MaskingSpec == MaskingSpecialization::MaskUpperTriangleFromBottomRight)
        {
            return MaskUpperTriangleFromBottomRightPredicate{};
        }
    }
    using C0MatrixMask = C0MatrixMask_impl<decltype(make_MaskOutPredicate())>;

    struct ComputeBasePtrOfStridedBatch
    {
        ComputeBasePtrOfStridedBatch() {}
        ComputeBasePtrOfStridedBatch(const AGridDesc_G_M_K& a_grid_desc_g_m_k,
                                     const BGridDesc_G_N_K& b_grid_desc_g_n_k,
                                     const D0GridDesc_G_M_N& d0_grid_desc_g_m_n,
                                     const ZGridDesc_G_M_N& z_grid_desc_g_m_n,
                                     const B1GridDesc_G_N_K& b1_grid_desc_g_n_k,
                                     const CGridDesc_G_M_N& c_grid_desc_g_m_n,
                                     index_t BatchStrideLSE)
            : a_grid_desc_g_m_k_(a_grid_desc_g_m_k),
              b_grid_desc_g_n_k_(b_grid_desc_g_n_k),
              d0_grid_desc_g_m_n_(d0_grid_desc_g_m_n),
              z_grid_desc_g_m_n_(z_grid_desc_g_m_n),
              b1_grid_desc_g_n_k_(b1_grid_desc_g_n_k),
              c_grid_desc_g_m_n_(c_grid_desc_g_m_n),
              BatchStrideLSE_(BatchStrideLSE)
        {
        }

        __host__ __device__ constexpr long_index_t GetABasePtr(index_t g_idx) const
        {
            return a_grid_desc_g_m_k_.CalculateOffset(make_multi_index(g_idx, 0, 0));
        }

        __host__ __device__ constexpr long_index_t GetBBasePtr(index_t g_idx) const
        {
            return b_grid_desc_g_n_k_.CalculateOffset(make_multi_index(g_idx, 0, 0));
        }

        __host__ __device__ constexpr long_index_t GetD0BasePtr(index_t g_idx) const
        {
            return d0_grid_desc_g_m_n_.CalculateOffset(make_multi_index(g_idx, 0, 0));
        }

        __host__ __device__ constexpr long_index_t GetZBasePtr(index_t g_idx) const
        {
            return z_grid_desc_g_m_n_.CalculateOffset(make_multi_index(g_idx, 0, 0));
        }

        __host__ __device__ constexpr long_index_t GetB1BasePtr(index_t g_idx) const
        {
            return b1_grid_desc_g_n_k_.CalculateOffset(make_multi_index(g_idx, 0, 0));
        }

        __host__ __device__ constexpr long_index_t GetCBasePtr(index_t g_idx) const
        {
            return c_grid_desc_g_m_n_.CalculateOffset(make_multi_index(g_idx, 0, 0));
        }

        __host__ __device__ constexpr long_index_t GetLSEBasePtr(index_t g_idx) const
        {
            return g_idx * static_cast<long_index_t>(BatchStrideLSE_);
        }

        private:
        AGridDesc_G_M_K a_grid_desc_g_m_k_;
        BGridDesc_G_N_K b_grid_desc_g_n_k_;
        D0GridDesc_G_M_N d0_grid_desc_g_m_n_;
        ZGridDesc_G_M_N z_grid_desc_g_m_n_;
        B1GridDesc_G_N_K b1_grid_desc_g_n_k_;
        CGridDesc_G_M_N c_grid_desc_g_m_n_;

        index_t BatchStrideLSE_;
    };

    // GridwiseGemm
    using GridwiseGemm = GridwiseBatchedMultiheadAttentionBackward_Qloop_Xdl_CShuffle_Light_V1<
        InputDataType, // TODO: distinguish A/B datatype
        D0DataType,
        OutputDataType,
        ZDataType,
        GemmDataType,
        GemmAccDataType,
        CShuffleDataType,
        LSEDataType,
        DDataType,
        AElementwiseOperation,
        BElementwiseOperation,
        AccElementwiseOperation,
        B1ElementwiseOperation,
        CElementwiseOperation,
        InMemoryDataOperationEnum::Set,
        AGridDesc_AK0_M_AK1,
        BGridDesc_BK0_N_BK1,
        KGridDesc_N_K,
        D0GridDesc_M_N,
        ZGridDesc_M_N,
        B1GridDesc_BK0_N_BK1,
        YGridDesc_M_O,
        LSEGridDesc_M,
        NumGemmKPrefetchStage,
        BlockSize,
        MPerBlock,
        NPerBlock,
        KPerBlock,
        Gemm1NPerBlock,
        Gemm1KPerBlock,
        Gemm2KPerBlock,
        AK1,
        BK1,
        B1K1,
        MPerXDL,
        NPerXDL,
        MXdlPerWave,
        NXdlPerWave,
        Gemm1NXdlPerWave,
        Gemm2NXdlPerWave,
        ABlockTransferThreadClusterLengths_AK0_M_AK1,
        ABlockTransferThreadClusterArrangeOrder,
        ABlockTransferSrcAccessOrder,
        ABlockTransferSrcVectorDim,
        ABlockTransferSrcScalarPerVector,
        ABlockTransferDstScalarPerVector_AK1,
        true,
        ABlockLdsExtraM,
        BBlockTransferThreadClusterLengths_BK0_N_BK1,
        BBlockTransferThreadClusterArrangeOrder,
        BBlockTransferSrcAccessOrder,
        BBlockTransferSrcVectorDim,
        BBlockTransferSrcScalarPerVector,
        BBlockTransferDstScalarPerVector_BK1,
        true,
        BBlockLdsExtraN,
        D0BlockTransferSrcScalarPerVector,
        CShuffleMXdlPerWavePerShuffle,
        CShuffleNXdlPerWavePerShuffle,
        CShuffleBlockTransferClusterLengths_MBlock_MPerBlock_NBlock_NPerBlock,
        CShuffleBlockTransferScalarPerVector_NPerBlock,
        LoopSched,
        Transform::matrix_padder.PadN,
        MaskingSpec != MaskingSpecialization::MaskDisabled,
        Deterministic>;

    // GridwiseYDotYGrad
    using GridwiseYDotYGrad = GridwiseBatchedMultiheadAttentionBackward_YDotYGrad<InputDataType,
                                                                                  DDataType,
                                                                                  DYGridDesc_M_O,
                                                                                  DGridDesc_M,
                                                                                  BlockSize,
                                                                                  DMPerBlock,
                                                                                  DKPerBlock,
                                                                                  Gemm1NPerBlock>;
    // Argument
    struct Argument : public BaseArgument
    {
        Argument(const InputDataType* p_a_grid,
                 const InputDataType* p_b_grid,
                 ZDataType* p_z_grid,
                 const InputDataType* p_b1_grid,
                 const InputDataType* p_c_grid, // for dS
                 const LSEDataType* p_lse_grid,
                 DDataType* p_d_grid,
                 const InputDataType* p_ygrad_grid,
                 OutputDataType* p_qgrad_grid,
                 OutputDataType* p_kgrad_grid,
                 OutputDataType* p_vgrad_grid,
                 const D0DataType* p_acc0_bias,
                 const D1DataType* p_acc1_bias,
                 D0DataType* p_d0grad_grid,
                 D1DataType* p_d1grad_grid,
                 const std::vector<index_t>& a_gs_ms_ks_lengths,
                 const std::vector<index_t>& a_gs_ms_ks_strides,
                 const std::vector<index_t>& b_gs_ns_ks_lengths,
                 const std::vector<index_t>& b_gs_ns_ks_strides,
                 const std::vector<index_t>& z_gs_ms_ns_lengths,
                 const std::vector<index_t>& z_gs_ms_ns_strides,
                 const std::vector<index_t>& b1_gs_gemm1ns_gemm1ks_lengths, // b1_gs_os_ns_lengths
                 const std::vector<index_t>& b1_gs_gemm1ns_gemm1ks_strides, // b1_gs_os_ns_strides
                 const std::vector<index_t>& c_gs_ms_gemm1ns_lengths,       // c_gs_ms_os_lengths
                 const std::vector<index_t>& c_gs_ms_gemm1ns_strides,       // c_gs_ms_os_strides
                 const std::vector<index_t>& lse_gs_ms_lengths,
                 const std::vector<ck::index_t>& acc0_bias_gs_ms_ns_lengths,
                 const std::vector<ck::index_t>& acc0_bias_gs_ms_ns_strides,
                 const std::vector<ck::index_t>&
                     acc1_bias_gs_ms_gemm1ns_lengths, // acc1_bias_gs_ms_os_lengths
                 const std::vector<ck::index_t>&
                     acc1_bias_gs_ms_gemm1ns_strides, // acc1_bias_gs_ms_os_strides
                 AElementwiseOperation a_element_op,
                 BElementwiseOperation b_element_op,
                 AccElementwiseOperation acc_element_op,
                 B1ElementwiseOperation b1_element_op,
                 CElementwiseOperation c_element_op,
                 float p_drop,
                 std::tuple<unsigned long long, unsigned long long> seeds)
            : p_a_grid_{p_a_grid},
              p_b_grid_{p_b_grid},
              p_d0_grid_{p_acc0_bias},
              p_z_grid_{p_z_grid},
              p_b1_grid_{p_b1_grid},
              p_c_grid_{p_c_grid},
              p_lse_grid_{p_lse_grid},
              p_d_grid_{p_d_grid},
              p_ygrad_grid_{p_ygrad_grid},
              p_qgrad_grid_{p_qgrad_grid},
              p_kgrad_grid_{p_kgrad_grid},
              p_vgrad_grid_{p_vgrad_grid},
              p_d0grad_grid_{p_d0grad_grid},
              a_grid_desc_ak0_m_ak1_{
                  DeviceOp::MakeAGridDescriptor_AK0_M_AK1(a_gs_ms_ks_lengths, a_gs_ms_ks_strides)},
              b_grid_desc_bk0_n_bk1_{
                  DeviceOp::MakeBGridDescriptor_BK0_N_BK1(b_gs_ns_ks_lengths, b_gs_ns_ks_strides)},
              z_grid_desc_m_n_{MakeZGridDescriptor_M_N(z_gs_ms_ns_lengths, z_gs_ms_ns_strides)},
              b1_grid_desc_bk0_n_bk1_{DeviceOp::MakeVGridDescriptor_O0_N_O1(
                  b1_gs_gemm1ns_gemm1ks_lengths, b1_gs_gemm1ns_gemm1ks_strides)},
              y_grid_desc_m_o_{Transform::MakeCGridDescriptor_M_N(c_gs_ms_gemm1ns_lengths,
                                                                  c_gs_ms_gemm1ns_strides)},
              d_y_grid_desc_m_o_{DTransform::MakeCGridDescriptor_M_N(c_gs_ms_gemm1ns_lengths,
                                                                     c_gs_ms_gemm1ns_strides)},
              lse_grid_desc_m_{DeviceOp::MakeLSEGridDescriptor_M(lse_gs_ms_lengths[NumDimG])},
              d_grid_desc_m_{DeviceOp::MakeDGridDescriptor_M(lse_gs_ms_lengths[NumDimG])},
              k_grid_desc_n_k_{
                  Transform::MakeB0GridDescriptor_N_K(b_gs_ns_ks_lengths, b_gs_ns_ks_strides)},
              ygrad_grid_desc_o0_m_o1_{DeviceOp::MakeYGradGridDescriptor_O0_M_O1(
                  c_gs_ms_gemm1ns_lengths, c_gs_ms_gemm1ns_strides)},
              // batch offsets
              a_grid_desc_g_m_k_{
                  Transform::MakeAGridDescriptor_G_M_K(a_gs_ms_ks_lengths, a_gs_ms_ks_strides)},
              b_grid_desc_g_n_k_{
                  Transform::MakeB0GridDescriptor_G_N_K(b_gs_ns_ks_lengths, b_gs_ns_ks_strides)},
              b1_grid_desc_g_n_k_{Transform::MakeB1GridDescriptor_G_N_K(
                  b1_gs_gemm1ns_gemm1ks_lengths, b1_gs_gemm1ns_gemm1ks_strides)},
              c_grid_desc_g_m_n_{Transform::MakeCGridDescriptor_G_M_N(c_gs_ms_gemm1ns_lengths,
                                                                      c_gs_ms_gemm1ns_strides)},
              z_grid_desc_g_m_n_{
                  Transform::MakeC0GridDescriptor_G_M_N(z_gs_ms_ns_lengths, z_gs_ms_ns_strides)},
              block_2_ctile_map_{GridwiseGemm::MakeDefaultBlock2CTileMap(k_grid_desc_n_k_)},
              d_block_2_ctile_map_{
                  GridwiseYDotYGrad::MakeDefaultBlock2CTileMap(d_y_grid_desc_m_o_)},
              d_y_grid_desc_mblock_mperblock_oblock_operblock_{},
              a_element_op_{a_element_op},
              b_element_op_{b_element_op},
              acc_element_op_{acc_element_op},
              b1_element_op_{b1_element_op},
              c_element_op_{c_element_op},
              c0_matrix_mask_{a_grid_desc_g_m_k_.GetLength(I1), b_grid_desc_g_n_k_.GetLength(I1)},
              raw_lengths_mz_nz_kz_gemm1nz_{a_gs_ms_ks_lengths[NumDimG + NumDimM - 1],
                                            b_gs_ns_ks_lengths[NumDimG + NumDimN - 1],
                                            b_gs_ns_ks_lengths[NumDimG + NumDimN + NumDimK - 1],
                                            b1_gs_gemm1ns_gemm1ks_lengths[NumDimG + NumDimO - 1]},
              a_mz_kz_strides_{a_gs_ms_ks_strides[NumDimG + NumDimM - 1],
                               a_gs_ms_ks_strides[NumDimG + NumDimM + NumDimK - 1]},
              b_nz_kz_strides_{b_gs_ns_ks_strides[NumDimG + NumDimN - 1],
                               b_gs_ns_ks_strides[NumDimG + NumDimN + NumDimK - 1]},
              b1_nz_kz_strides_{b1_gs_gemm1ns_gemm1ks_strides[NumDimG + NumDimO - 1],
                                b1_gs_gemm1ns_gemm1ks_strides[NumDimG + NumDimO + NumDimN - 1]},
              c_mz_gemm1nz_strides_{c_gs_ms_gemm1ns_strides[NumDimG + NumDimM - 1],
                                    c_gs_ms_gemm1ns_strides[NumDimG + NumDimM + NumDimO - 1]},
              batch_count_{c_grid_desc_g_m_n_.GetLength(I0)},
              p_drop_{p_drop}
        {
            // TODO: implement bias addition
            ignore = p_d1grad_grid;
            ignore = p_acc1_bias;
            ignore = acc1_bias_gs_ms_gemm1ns_lengths;
            ignore = acc1_bias_gs_ms_gemm1ns_strides;

            if constexpr(!is_same<D0DataType, void>::value)
            {
                const auto d0_grid_desc_m_n = MakeD0GridDescriptor_M_N(acc0_bias_gs_ms_ns_lengths,
                                                                       acc0_bias_gs_ms_ns_strides);
                d0_grid_desc_m0_n0_m1_m2_n1_m3_ =
                    GridwiseGemm::MakeD0GridDescriptor_M0_N0_M1_M2_N1_M3(d0_grid_desc_m_n);

                d0_grid_desc_g_m_n_ = Transform::MakeC0GridDescriptor_G_M_N(
                    acc0_bias_gs_ms_ns_lengths, acc0_bias_gs_ms_ns_strides);

                d0_n_length_stride_.push_back(acc0_bias_gs_ms_ns_lengths[NumDimG + NumDimM]);
                d0_n_length_stride_.push_back(acc0_bias_gs_ms_ns_strides[NumDimG + NumDimM]);
            }

            compute_base_ptr_of_batch_ = ComputeBasePtrOfStridedBatch(
                a_grid_desc_g_m_k_,
                b_grid_desc_g_n_k_,
                d0_grid_desc_g_m_n_,
                z_grid_desc_g_m_n_,
                b1_grid_desc_g_n_k_,
                c_grid_desc_g_m_n_,
                type_convert<index_t>(lse_grid_desc_m_.GetElementSpaceSize()));

            seed_   = std::get<0>(seeds);
            offset_ = std::get<1>(seeds);

            c_grid_desc_m0_n0_m1_n1_m2_n2_m3_m4_m5_n3_ =
                GridwiseGemm::MakeCGridDescriptor_M0_N0_M1_N1_M2_N2_M3_M4_M5_N3(z_grid_desc_m_n_);
            d_y_grid_desc_mblock_mperblock_oblock_operblock_ =
                GridwiseYDotYGrad::MakeYGridDescriptor_MBlock_MPerBlock_NBlock_NPerBlock(
                    d_y_grid_desc_m_o_);
            // Print();

            m_raw_padded_ = GridwiseGemm::GetPaddedSize(raw_lengths_mz_nz_kz_gemm1nz_[0]);
            n_raw_padded_ = GridwiseGemm::GetPaddedSize(raw_lengths_mz_nz_kz_gemm1nz_[1]);
        }

        void Print() const
        {
            std::cout << "a_grid_desc_g_m_k_: " << a_grid_desc_g_m_k_.GetLength(I0) << ", "
                      << a_grid_desc_g_m_k_.GetLength(I1) << ", "
                      << a_grid_desc_g_m_k_.GetLength(I2) << '\n';
            // a_grid_desc_g_m_k_.Print();
            std::cout << "b_grid_desc_g_n_k_: " << b_grid_desc_g_n_k_.GetLength(I0) << ", "
                      << b_grid_desc_g_n_k_.GetLength(I1) << ", "
                      << b_grid_desc_g_n_k_.GetLength(I2) << '\n';
            // b_grid_desc_g_n_k_.Print();
            std::cout << "b1_grid_desc_g_o_n_: " << b1_grid_desc_g_n_k_.GetLength(I0) << ", "
                      << b1_grid_desc_g_n_k_.GetLength(I1) << ", "
                      << b1_grid_desc_g_n_k_.GetLength(I2) << '\n';
            // b1_grid_desc_g_n_k_.Print();
            std::cout << "c_grid_desc_g_m_o_: " << c_grid_desc_g_m_n_.GetLength(I0) << ", "
                      << c_grid_desc_g_m_n_.GetLength(I1) << ", "
                      << c_grid_desc_g_m_n_.GetLength(I2) << '\n';
            // c_grid_desc_g_m_n_.Print();
            std::cout << "k_grid_desc_n_k_: " << k_grid_desc_n_k_.GetLength(I0) << ", "
                      << k_grid_desc_n_k_.GetLength(I1) << '\n';
            std::cout << "ygrad_grid_desc_o0_m_o1_: " << ygrad_grid_desc_o0_m_o1_.GetLength(I0)
                      << ", " << ygrad_grid_desc_o0_m_o1_.GetLength(I1) << ", "
                      << ygrad_grid_desc_o0_m_o1_.GetLength(I2) << '\n';
        }

        // pointers
        const InputDataType* p_a_grid_;
        const InputDataType* p_b_grid_;
        const D0DataType* p_d0_grid_;
        ZDataType* p_z_grid_;
        const InputDataType* p_b1_grid_;
        const InputDataType* p_c_grid_;
        const LSEDataType* p_lse_grid_;
        DDataType* p_d_grid_;
        const InputDataType* p_ygrad_grid_;
        OutputDataType* p_qgrad_grid_;
        OutputDataType* p_kgrad_grid_;
        OutputDataType* p_vgrad_grid_;
        D0DataType* p_d0grad_grid_;

        // tensor descriptor
        AGridDesc_AK0_M_AK1 a_grid_desc_ak0_m_ak1_;
        BGridDesc_BK0_N_BK1 b_grid_desc_bk0_n_bk1_;
        typename GridwiseGemm::D0GridDescriptor_M0_N0_M1_M2_N1_M3 d0_grid_desc_m0_n0_m1_m2_n1_m3_;
        ZGridDesc_M_N z_grid_desc_m_n_;
        B1GridDesc_BK0_N_BK1 b1_grid_desc_bk0_n_bk1_;
        YGridDesc_M_O y_grid_desc_m_o_;
        DYGridDesc_M_O d_y_grid_desc_m_o_;
        LSEGridDesc_M lse_grid_desc_m_;
        DGridDesc_M d_grid_desc_m_;
        KGridDesc_N_K k_grid_desc_n_k_;
        YGradGridDesc_O0_M_O1 ygrad_grid_desc_o0_m_o1_;

        // batch offsets
        AGridDesc_G_M_K a_grid_desc_g_m_k_;
        BGridDesc_G_N_K b_grid_desc_g_n_k_;
        D0GridDesc_G_M_N d0_grid_desc_g_m_n_;
        B1GridDesc_G_N_K b1_grid_desc_g_n_k_;
        CGridDesc_G_M_N c_grid_desc_g_m_n_;
        ZGridDesc_G_M_N z_grid_desc_g_m_n_;

        typename GridwiseGemm::ZGridDescriptor_M0_N0_M1_N1_M2_N2_M3_M4_M5_N3
            c_grid_desc_m0_n0_m1_n1_m2_n2_m3_m4_m5_n3_;

        // block-to-c-tile map
        typename GridwiseGemm::DefaultBlock2CTileMap block_2_ctile_map_;
        typename GridwiseYDotYGrad::DefaultBlock2CTileMap d_block_2_ctile_map_;
        typename GridwiseYDotYGrad::YGridDescriptor_MBlock_MPerBlock_NBlock_NPerBlock
            d_y_grid_desc_mblock_mperblock_oblock_operblock_;

        // element-wise op
        AElementwiseOperation a_element_op_;
        BElementwiseOperation b_element_op_;
        AccElementwiseOperation acc_element_op_;
        B1ElementwiseOperation b1_element_op_;
        CElementwiseOperation c_element_op_;

        // check C0 masking and padding
        C0MatrixMask c0_matrix_mask_;

        // For robust IsSupportedArgument() check
        std::vector<index_t> raw_lengths_mz_nz_kz_gemm1nz_;
        std::vector<index_t> a_mz_kz_strides_;
        std::vector<index_t> b_nz_kz_strides_;
        std::vector<index_t> b1_nz_kz_strides_;
        std::vector<index_t> c_mz_gemm1nz_strides_;

        index_t batch_count_;
        ComputeBasePtrOfStridedBatch compute_base_ptr_of_batch_;

        float p_drop_;
        unsigned long long seed_;
        unsigned long long offset_;

        index_t m_raw_padded_;
        index_t n_raw_padded_;

        // raw data
        std::vector<ck::index_t> d0_n_length_stride_;
    };

    // Invoker
    struct Invoker : public BaseInvoker
    {
        using Argument = DeviceOp::Argument;

        float Run(const Argument& arg, const StreamConfig& stream_config = StreamConfig{})
        {
            if(!DeviceOp::IsSupportedArgument(arg))
            {
                throw std::runtime_error("wrong! unsupported argument");
            }

            float ave_time = 0;
            {
                const index_t grid_size =
                    arg.d_block_2_ctile_map_.CalculateGridSize(arg.d_y_grid_desc_m_o_) *
                    arg.batch_count_;

                auto launch_kernel = [&]() {
                    const auto kernel = kernel_batched_multihead_attention_backward_ydotygrad_v1<
                        GridwiseYDotYGrad,
                        InputDataType,
                        DDataType,
                        typename GridwiseYDotYGrad::
                            YGridDescriptor_MBlock_MPerBlock_NBlock_NPerBlock,
                        DeviceOp::DGridDesc_M,
                        typename GridwiseYDotYGrad::DefaultBlock2CTileMap,
                        ComputeBasePtrOfStridedBatch>;

                    return launch_and_time_kernel(
                        stream_config,
                        kernel,
                        dim3(grid_size),
                        dim3(BlockSize),
                        0,
                        arg.p_c_grid_,
                        arg.p_ygrad_grid_,
                        arg.p_d_grid_,
                        arg.d_y_grid_desc_mblock_mperblock_oblock_operblock_,
                        arg.d_grid_desc_m_,
                        arg.d_block_2_ctile_map_,
                        arg.batch_count_,
                        arg.compute_base_ptr_of_batch_,
                        arg.p_drop_);
                };

                ave_time = launch_kernel();
            }

            const index_t grid_size =
                (Deterministic ? 1
                               : arg.block_2_ctile_map_.CalculateGridSize(arg.k_grid_desc_n_k_)) *
                arg.batch_count_;

            auto launch_kernel = [&](auto has_main_k_block_loop_, auto is_dropout_) {
                const auto kernel =
                    kernel_batched_multihead_attention_backward_qloop_xdl_cshuffle_light_v1<
                        GridwiseGemm,
                        InputDataType,
                        D0DataType,
                        OutputDataType,
                        ZDataType,
                        LSEDataType,
                        DDataType,
                        AElementwiseOperation,
                        BElementwiseOperation,
                        AccElementwiseOperation,
                        B1ElementwiseOperation,
                        CElementwiseOperation,
                        DeviceOp::AGridDesc_AK0_M_AK1,
                        DeviceOp::BGridDesc_BK0_N_BK1,
                        typename GridwiseGemm::D0GridDescriptor_M0_N0_M1_M2_N1_M3,
                        typename GridwiseGemm::ZGridDescriptor_M0_N0_M1_N1_M2_N2_M3_M4_M5_N3,
                        DeviceOp::B1GridDesc_BK0_N_BK1,
                        DeviceOp::LSEGridDesc_M,
                        DeviceOp::YGradGridDesc_O0_M_O1,
                        typename GridwiseGemm::DefaultBlock2CTileMap,
                        ComputeBasePtrOfStridedBatch,
                        C0MatrixMask,
                        has_main_k_block_loop_,
                        is_dropout_,
                        Deterministic>;

                return launch_and_time_kernel(
                    stream_config,
                    kernel,
                    dim3(grid_size),
                    dim3(BlockSize),
                    0,
                    arg.p_a_grid_,
                    arg.p_b_grid_,
                    arg.p_d0_grid_,
                    arg.p_z_grid_,
                    arg.p_b1_grid_,
                    arg.p_lse_grid_,
                    arg.p_d_grid_,
                    arg.p_ygrad_grid_,
                    arg.p_qgrad_grid_,
                    arg.p_kgrad_grid_,
                    arg.p_d0grad_grid_,
                    arg.p_vgrad_grid_,
                    arg.a_element_op_,
                    arg.b_element_op_,
                    arg.acc_element_op_,
                    arg.b1_element_op_,
                    arg.c_element_op_,
                    arg.a_grid_desc_ak0_m_ak1_,
                    arg.b_grid_desc_bk0_n_bk1_,
                    arg.d0_grid_desc_m0_n0_m1_m2_n1_m3_,
                    arg.c_grid_desc_m0_n0_m1_n1_m2_n2_m3_m4_m5_n3_,
                    arg.b1_grid_desc_bk0_n_bk1_,
                    arg.lse_grid_desc_m_,
                    arg.ygrad_grid_desc_o0_m_o1_,
                    arg.block_2_ctile_map_,
                    arg.batch_count_,
                    arg.block_2_ctile_map_.CalculateGridSize(arg.k_grid_desc_n_k_),
                    arg.compute_base_ptr_of_batch_,
                    arg.c0_matrix_mask_,
                    arg.p_drop_,
                    arg.seed_,
                    arg.offset_,
                    arg.m_raw_padded_,
                    arg.n_raw_padded_);
            };
            if(arg.p_drop_ > 0.0)
            {
                ave_time += launch_kernel(integral_constant<bool, false>{},
                                          integral_constant<bool, true>{});
            }
            else
            {
                ave_time += launch_kernel(integral_constant<bool, false>{},
                                          integral_constant<bool, false>{});
            }
            return ave_time;
        }

        // polymorphic
        float Run(const BaseArgument* p_arg,
                  const StreamConfig& stream_config = StreamConfig{}) override
        {
            return Run(*dynamic_cast<const Argument*>(p_arg), stream_config);
        }
    };

    static constexpr bool IsValidCompilationParameter()
    {
        // TODO: properly implement this check
        return true;
    }

    static bool IsSupportedArgument(const Argument& arg)
    {
#if DEBUG_LOG
        arg.Print();
#endif

        if(!(ck::get_device_name() == "gfx908" || ck::get_device_name() == "gfx90a" ||
             ck::get_device_name() == "gfx940" || ck::get_device_name() == "gfx941" ||
             ck::get_device_name() == "gfx942"))
        {
            return false;
        }

        // TODO: Check if tensor specialization & strides mismatch

        // Check if C permute dimension matches GEMM + GEMM shape
        const index_t c_g      = arg.c_grid_desc_g_m_n_.GetLength(I0); // unpadded
        const index_t c_m      = arg.y_grid_desc_m_o_.GetLength(I0);
        const index_t c_gemm1n = arg.y_grid_desc_m_o_.GetLength(I1);
        const index_t a_m      = arg.a_grid_desc_ak0_m_ak1_.GetLength(I1);
        const index_t b1_gemm1n =
            arg.b1_grid_desc_bk0_n_bk1_.GetLength(I0) * arg.b1_grid_desc_bk0_n_bk1_.GetLength(I2);

        if(!(c_g == arg.batch_count_ && c_m == a_m && c_gemm1n == b1_gemm1n))
        {
            return false;
        }

        if constexpr(!is_same<D0DataType, void>::value)
        {
            if(arg.d0_n_length_stride_[1] == 1 &&
               arg.d0_n_length_stride_[0] % D0BlockTransferSrcScalarPerVector != 0)
            {
                return false;
            }
            if(arg.d0_n_length_stride_[1] != 1 && D0BlockTransferSrcScalarPerVector != 1)
            {
                return false;
            }
        }

        // Note: we need raw lengths since threadwise copy can not handle vector load when part of
        // vector is out of bounds
        // Note: need lowest dim in Ms/Ns/Ks/Os, not merged M/N/K/O
        const auto MzRaw      = arg.raw_lengths_mz_nz_kz_gemm1nz_[0];
        const auto NzRaw      = arg.raw_lengths_mz_nz_kz_gemm1nz_[1];
        const auto KzRaw      = arg.raw_lengths_mz_nz_kz_gemm1nz_[2];
        const auto Gemm1NzRaw = arg.raw_lengths_mz_nz_kz_gemm1nz_[3];

        // Check scalar per vector requirement
        const auto a_extent_lowest = ABlockTransferSrcVectorDim == 2 ? KzRaw : MzRaw;
        const auto b_extent_lowest = BBlockTransferSrcVectorDim == 2 ? KzRaw : NzRaw;
        const auto c_extent_lowest = Gemm1NzRaw;

        if(!(a_extent_lowest % ABlockTransferSrcScalarPerVector == 0 &&
             b_extent_lowest % BBlockTransferSrcScalarPerVector == 0 &&
             c_extent_lowest % CShuffleBlockTransferScalarPerVector_NPerBlock == 0))
        {
            return false;
        }

        // Check vector load/store requirement
        const auto a_stride_lowest =
            ABlockTransferSrcVectorDim == 2 ? arg.a_mz_kz_strides_[1] : arg.a_mz_kz_strides_[0];
        const auto b_stride_lowest =
            BBlockTransferSrcVectorDim == 2 ? arg.b_nz_kz_strides_[1] : arg.b_nz_kz_strides_[0];
        const auto c_stride_lowest =
            arg.c_mz_gemm1nz_strides_[1]; // cshuffle assumes lowest dim in Gemm1Ns to be contiguous

        if(!(a_stride_lowest == 1 || b_stride_lowest == 1 || c_stride_lowest == 1))
        {
            return false;
        }

        return GridwiseGemm::CheckValidity(arg.a_grid_desc_ak0_m_ak1_,
                                           arg.b_grid_desc_bk0_n_bk1_,
                                           arg.b1_grid_desc_bk0_n_bk1_,
                                           arg.y_grid_desc_m_o_);
    }

    // polymorphic
    bool IsSupportedArgument(const BaseArgument* p_arg) override
    {
        return IsSupportedArgument(*dynamic_cast<const Argument*>(p_arg));
    }

    static auto
    MakeArgument(const InputDataType* p_a,
                 const InputDataType* p_b,
                 ZDataType* p_z,
                 const InputDataType* p_b1,
                 const InputDataType* p_c,
                 const LSEDataType* p_lse,
                 DDataType* p_d_grid,
                 const InputDataType* p_ygrad_grid,
                 OutputDataType* p_qgrad_grid,
                 OutputDataType* p_kgrad_grid,
                 OutputDataType* p_vgrad_grid,
                 const D0DataType* p_acc0_bias,
                 const D1DataType* p_acc1_bias,
                 D0DataType* p_d0grad_grid,
                 D1DataType* p_d1grad_grid,
                 const std::vector<index_t>& a_gs_ms_ks_lengths,
                 const std::vector<index_t>& a_gs_ms_ks_strides,
                 const std::vector<index_t>& b_gs_ns_ks_lengths,
                 const std::vector<index_t>& b_gs_ns_ks_strides,
                 const std::vector<index_t>& z_gs_ms_ns_lengths,
                 const std::vector<index_t>& z_gs_ms_ns_strides,
                 const std::vector<index_t>& b1_gs_gemm1ns_gemm1ks_lengths, // b1_gs_os_ns_lengths
                 const std::vector<index_t>& b1_gs_gemm1ns_gemm1ks_strides, // b1_gs_os_ns_strides
                 const std::vector<index_t>& c_gs_ms_gemm1ns_lengths,       // c_gs_ms_os_lengths
                 const std::vector<index_t>& c_gs_ms_gemm1ns_strides,       // c_gs_ms_os_strides
                 const std::vector<index_t>& lse_gs_ms_lengths,
                 const std::vector<ck::index_t>& acc0_bias_gs_ms_ns_lengths,
                 const std::vector<ck::index_t>& acc0_bias_gs_ms_ns_strides,
                 const std::vector<ck::index_t>&
                     acc1_bias_gs_ms_gemm1ns_lengths, // acc1_bias_gs_ms_os_lengths
                 const std::vector<ck::index_t>&
                     acc1_bias_gs_ms_gemm1ns_strides, // acc1_bias_gs_ms_os_strides
                 AElementwiseOperation a_element_op,
                 BElementwiseOperation b_element_op,
                 AccElementwiseOperation acc_element_op,
                 B1ElementwiseOperation b1_element_op,
                 CElementwiseOperation c_element_op,
                 float p_drop,
                 std::tuple<unsigned long long, unsigned long long> seeds)
    {
        return Argument{p_a,
                        p_b,
                        p_z,
                        p_b1,
                        p_c,
                        p_lse,
                        p_d_grid,
                        p_ygrad_grid,
                        p_qgrad_grid,
                        p_kgrad_grid,
                        p_vgrad_grid,
                        p_acc0_bias,
                        p_acc1_bias,
                        p_d0grad_grid,
                        p_d1grad_grid,
                        a_gs_ms_ks_lengths,
                        a_gs_ms_ks_strides,
                        b_gs_ns_ks_lengths,
                        b_gs_ns_ks_strides,
                        z_gs_ms_ns_lengths,
                        z_gs_ms_ns_strides,
                        b1_gs_gemm1ns_gemm1ks_lengths, // b1_gs_os_ns_lengths
                        b1_gs_gemm1ns_gemm1ks_strides, // b1_gs_os_ns_strides
                        c_gs_ms_gemm1ns_lengths,       // c_gs_ms_os_lengths
                        c_gs_ms_gemm1ns_strides,       // c_gs_ms_os_strides
                        lse_gs_ms_lengths,
                        acc0_bias_gs_ms_ns_lengths,
                        acc0_bias_gs_ms_ns_strides,
                        acc1_bias_gs_ms_gemm1ns_lengths, // acc1_bias_gs_ms_os_lengths
                        acc1_bias_gs_ms_gemm1ns_strides, // acc1_bias_gs_ms_os_strides
                        a_element_op,
                        b_element_op,
                        acc_element_op,
                        b1_element_op,
                        c_element_op,
                        p_drop,
                        seeds};
    }

    static auto MakeInvoker() { return Invoker{}; }

    // polymorphic
    // FIXME: constness
    std::unique_ptr<BaseArgument> MakeArgumentPointer(
        const void* p_a,
        const void* p_b,
        void* p_z,
        const void* p_b1,
        const void* p_c,
        const void* p_lse,
        void* p_d_grid,
        const void* p_ygrad_grid,
        void* p_qgrad_grid,
        void* p_kgrad_grid,
        void* p_vgrad_grid,
        const D0DataType* p_acc0_bias,
        const D1DataType* p_acc1_bias,
        void* p_d0grad_grid,
        void* p_d1grad_grid,
        const std::vector<index_t>& a_gs_ms_ks_lengths,
        const std::vector<index_t>& a_gs_ms_ks_strides,
        const std::vector<index_t>& b_gs_ns_ks_lengths,
        const std::vector<index_t>& b_gs_ns_ks_strides,
        const std::vector<index_t>& z_gs_ms_ns_lengths,
        const std::vector<index_t>& z_gs_ms_ns_strides,
        const std::vector<index_t>& b1_gs_gemm1ns_gemm1ks_lengths, // b1_gs_os_ns_lengths
        const std::vector<index_t>& b1_gs_gemm1ns_gemm1ks_strides, // b1_gs_os_ns_strides
        const std::vector<index_t>& c_gs_ms_gemm1ns_lengths,       // c_gs_ms_os_lengths
        const std::vector<index_t>& c_gs_ms_gemm1ns_strides,       // c_gs_ms_os_strides
        const std::vector<index_t>& lse_gs_ms_lengths,
        const std::vector<ck::index_t>& acc0_bias_gs_ms_ns_lengths,
        const std::vector<ck::index_t>& acc0_bias_gs_ms_ns_strides,
        const std::vector<ck::index_t>&
            acc1_bias_gs_ms_gemm1ns_lengths, // acc1_bias_gs_ms_os_lengths
        const std::vector<ck::index_t>&
            acc1_bias_gs_ms_gemm1ns_strides, // acc1_bias_gs_ms_os_strides
        AElementwiseOperation a_element_op,
        BElementwiseOperation b_element_op,
        AccElementwiseOperation acc_element_op,
        B1ElementwiseOperation b1_element_op,
        CElementwiseOperation c_element_op,
        float p_drop,
        std::tuple<unsigned long long, unsigned long long> seeds) // override
    {
        return std::make_unique<Argument>(
            static_cast<const InputDataType*>(p_a),
            static_cast<const InputDataType*>(p_b),
            static_cast<ZDataType*>(p_z),
            static_cast<const InputDataType*>(p_b1),
            static_cast<const InputDataType*>(p_c),
            static_cast<const LSEDataType*>(p_lse),
            static_cast<DDataType*>(p_d_grid),
            static_cast<const InputDataType*>(p_ygrad_grid),
            static_cast<OutputDataType*>(p_qgrad_grid),
            static_cast<OutputDataType*>(p_kgrad_grid),
            static_cast<OutputDataType*>(p_vgrad_grid),
            static_cast<const D0DataType*>(p_acc0_bias), // cast in struct Argument
            static_cast<const D1DataType*>(p_acc1_bias), // cast in struct Argument
            static_cast<D0DataType*>(p_d0grad_grid),
            static_cast<D1DataType*>(p_d1grad_grid),
            a_gs_ms_ks_lengths,
            a_gs_ms_ks_strides,
            b_gs_ns_ks_lengths,
            b_gs_ns_ks_strides,
            z_gs_ms_ns_lengths,
            z_gs_ms_ns_strides,
            b1_gs_gemm1ns_gemm1ks_lengths, // b1_gs_os_ns_lengths
            b1_gs_gemm1ns_gemm1ks_strides, // b1_gs_os_ns_strides
            c_gs_ms_gemm1ns_lengths,       // c_gs_ms_os_lengths
            c_gs_ms_gemm1ns_strides,       // c_gs_ms_os_strides
            lse_gs_ms_lengths,
            acc0_bias_gs_ms_ns_lengths,
            acc0_bias_gs_ms_ns_strides,
            acc1_bias_gs_ms_gemm1ns_lengths,
            acc1_bias_gs_ms_gemm1ns_strides,
            a_element_op,
            b_element_op,
            acc_element_op,
            b1_element_op,
            c_element_op,
            p_drop,
            seeds);
    }

    // polymorphic
    std::unique_ptr<BaseInvoker> MakeInvokerPointer() // override
    {
        return std::make_unique<Invoker>(Invoker{});
    }

    // polymorphic
    std::string GetTypeString() const override
    {
        auto str = std::stringstream();

        // clang-format off
        str << "DeviceBatchedMultiheadAttentionBackward_Qloop_Xdl_CShuffle_Light_V1"
            << "<"
            << BlockSize << ", "
            << MPerBlock << ", "
            << NPerBlock << ", "
            << KPerBlock << ", "
            << AK1 << ", "
            << BK1 << ", "
            << MPerBlock << ", "
            << Gemm1NPerBlock << ", "
            << Gemm1KPerBlock << ", "
            << Gemm2KPerBlock << ", "
            << B1K1 << ", "
            << getGemmSpecializationString(GemmSpec) << ", "
            << "ASpec" << getTensorSpecializationString(ASpec) << ", "
            << "B0Spec" << getTensorSpecializationString(BSpec) << ", "
            << "B1Spec" << getTensorSpecializationString(B1Spec) << ", "
            << "CSpec" << getTensorSpecializationString(CSpec) << ", "
            << getMaskingSpecializationString(MaskingSpec) << ">";
        // clang-format on

        return str.str();
    }
};

} // namespace device
} // namespace tensor_operation
} // namespace ck

// SPDX-License-Identifier: GPL-2.0-or-later
// See LICENSE and NOTICE for the inherited AviSynth linking exception.
#ifndef AIF_FOCUS_KERNEL_H
#define AIF_FOCUS_KERNEL_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum aif_focus_status { AIF_FOCUS_OK = 0, AIF_FOCUS_INVALID = 1 };
enum aif_focus_layout { AIF_FOCUS_PLANAR = 0, AIF_FOCUS_RGB3 = 1, AIF_FOCUS_RGB4 = 2, AIF_FOCUS_YUY2 = 3 };
enum aif_focus_cpu {
  AIF_FOCUS_SSE2 = 1,
  AIF_FOCUS_SSSE3 = 2,
  AIF_FOCUS_SSE41 = 4,
  AIF_FOCUS_AVX2 = 8,
  AIF_FOCUS_NEON = 1 << 8,
  AIF_FOCUS_SVE2 = 1 << 9
};

// Initial migration ABI, not yet frozen. No SDK, allocations, global CPU policy,
// callbacks or exceptions. CPU bits are this header's bits, NOT AviSynth bits.
// Zero requests scalar. Other bits are intersected with compiled/host support.
// Legacy scalar and SIMD coefficient quantization can differ: selecting another
// backend does not promise bit-identical Blur/Sharpen output in this baseline.
uint32_t aif_focus_supported_cpu(void);
// Highest supported execution profile within allowed; zero means scalar.
// SSE41 uses Highway SSSE3 to stay below the SSE4.2 requirement of HWY_SSE4.
// SVE2 additionally requires NEON permission. Does not change global dispatch.
uint32_t aif_focus_selected_cpu(uint32_t allowed);

// Shared buffer contract:
// - Positive dimensions/strides; bits = 8, 9..16, or 32 (float).
// - row_bytes/stride are bytes, integral multiples of the component size.
// - U16/F32 pointers have natural alignment. Stride covers the active row.
// - Every row, INCLUDING the last, owns stride bytes of storage.
// - When SIMD is requested, caller provides 32-byte-aligned rows/strides and
//   readable/writable padding through round_up(row_bytes,32). Padding may change.
//   Unaligned rows use scalar. No source/destination overlap except explicitly
//   allowed below. External synchronization owns all buffer/plan lifetimes.
// - U16 input is within its declared bit depth. Float input is finite, with
//   finite intermediates; floating-point rounding mode is round-to-nearest.

// Vertical adjustment is in-place. scratch is distinct from data and has at
// least round_up(row_bytes,32) bytes; align it to 32 for SIMD. It need not be
// initialized. half_amount = round(32768 * pow(2,amount)), within 0..65536;
// float_amount is the corresponding floating coefficient, within 0..2.
int aif_focus_vertical(void* data, int stride, int row_bytes, int height, int bits, int half_amount, float float_amount,
                       void* scratch, size_t scratch_size, uint32_t cpu);

// Horizontal adjustment copies source to destination where needed. Planar and
// packed RGB integer/planar F32 layouts are supported; YUY2 is U8 only.
// Buffers must not overlap. Packed RGB includes alpha; planar alpha selection
// belongs to the caller. Boundary samples are replicated within each channel.
int aif_focus_horizontal(const void* source, int source_stride, void* destination, int destination_stride,
                         int row_bytes, int height, int bits, int layout, int half_amount, float float_amount,
                         uint32_t cpu);

// One in-place center row, 0..14 disjoint read-only neighbor rows. Each buffer
// has round_up(row_bytes,32) accessible bytes for SIMD, otherwise row_bytes.
// Thresholds are 0..255. PLANAR applies one threshold to all samples; YUY2 uses
// luma/chroma alternation. PLANAR can also represent packed RGB temporal samples.
// A neighbor outside threshold contributes the original center, not zero.
int aif_focus_temporal_line(uint8_t* center, const uint8_t* const* neighbors, int neighbor_count, int row_bytes,
                            int bits, int layout, unsigned luma_threshold, unsigned chroma_threshold, uint32_t cpu);

// Sum of absolute differences, normalized to the 8-bit range. Does not omit
// alpha or padding on the caller's behalf: row_bytes is the exact active span.
int aif_focus_sad(const void* a, const void* b, int stride_a, int stride_b, int row_bytes, int height, int bits,
                  uint32_t cpu, int64_t* result);

// Legacy joint Y/U/V SpatialSoften. radius 0..32, thresholds 0..255.
// YUY2 rows are multiples of four bytes. No overlap or SIMD padding required.
// Horizontal border groups are copied, vertical borders replicated. If the
// radius leaves no interior, the active image is copied without overreading.
int aif_focus_spatial_yuy2(const uint8_t* source, int source_stride, uint8_t* destination, int destination_stride,
                           int row_bytes, int height, int radius, unsigned luma_threshold, unsigned chroma_threshold);

#ifdef __cplusplus
}
#endif
#endif

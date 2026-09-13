// SPDX-License-Identifier: GPL-2.0-or-later
#include "scalar.inc"
#include "backend.h"
#include <limits>

namespace {
using namespace aif::focus::scalar;

int pixel_size(int bits) {
  return bits == 8 ? 1 : bits >= 9 && bits <= 16 ? 2 : bits == 32 ? 4 : 0;
}
bool aligned(const void* p, size_t n) {
  return (reinterpret_cast<uintptr_t>(p) & (n - 1)) == 0;
}
bool valid_plane(const void* p, int stride, int row, int height, int bits) {
  const int size = pixel_size(bits);
  return p && size && row > 0 && row <= std::numeric_limits<int>::max() - 32 && stride >= row && height > 0 &&
         row % size == 0 && stride % size == 0 && aligned(p, size) &&
         static_cast<uint64_t>(stride) * height <= static_cast<uint64_t>(std::numeric_limits<ptrdiff_t>::max());
}
bool valid_amount(int half, float amount) {
  return half >= 0 && half <= 65536 && std::isfinite(amount) && amount >= 0 && amount <= 2;
}
uint32_t usable_cpu(uint32_t cpu, const void* p, int stride) {
  return aligned(p, 32) && stride % 32 == 0 ? cpu & aif_focus_supported_cpu() : 0;
}
void copy_rows(const uint8_t* src, int sp, uint8_t* dst, int dp, int row, int h) {
  for (int y = 0; y < h; ++y) {
    std::memcpy(dst, src, row);
    src += sp;
    dst += dp;
  }
}
} // namespace

extern "C" int aif_focus_vertical(void* data, int stride, int row_bytes, int height, int bits, int half_amount,
                                  float float_amount, void* scratch, size_t scratch_size, uint32_t cpu) {
  if (!valid_plane(data, stride, row_bytes, height, bits) || !scratch || scratch == data ||
      !aligned(scratch, pixel_size(bits)) || !valid_amount(half_amount, float_amount) ||
      scratch_size < ((static_cast<size_t>(row_bytes) + 31) & ~size_t(31)))
    return AIF_FOCUS_INVALID;
  cpu = usable_cpu(cpu, data, stride);
  if (!aligned(scratch, 32))
    cpu = 0;
  std::memcpy(scratch, data, row_bytes);
  // Keep the existing initialized scratch-padding contract.
  std::memset(static_cast<uint8_t*>(scratch) + row_bytes, 0,
              ((static_cast<size_t>(row_bytes) + 31) & ~size_t(31)) - row_bytes);
  auto* line = static_cast<uint8_t*>(scratch);
  auto* dst = static_cast<uint8_t*>(data);
  if (const auto* hw = aif::focus::backend(cpu)) {
    hw->vertical(dst, stride, row_bytes, height, bits, half_amount, float_amount, line, cpu);
    return AIF_FOCUS_OK;
  }
  if (bits == 8)
    af_vertical_process<uint8_t>(line, dst, height, stride, row_bytes, half_amount, bits);
  else if (bits == 32)
    af_vertical_process_float(line, dst, height, stride, row_bytes, float_amount);
  else
    af_vertical_process<uint16_t>(line, dst, height, stride, row_bytes, half_amount, bits);
  return AIF_FOCUS_OK;
}

extern "C" int aif_focus_horizontal(const void* source, int source_stride, void* destination, int destination_stride,
                                    int row_bytes, int height, int bits, int layout, int half_amount,
                                    float float_amount, uint32_t cpu) {
  if (!valid_plane(source, source_stride, row_bytes, height, bits) ||
      !valid_plane(destination, destination_stride, row_bytes, height, bits) || source == destination ||
      !valid_amount(half_amount, float_amount) || layout < AIF_FOCUS_PLANAR || layout > AIF_FOCUS_YUY2 ||
      (bits == 32 && layout != AIF_FOCUS_PLANAR) || (layout == AIF_FOCUS_YUY2 && (bits != 8 || row_bytes % 4)))
    return AIF_FOCUS_INVALID;
  const int size = pixel_size(bits);
  const int channels = layout == AIF_FOCUS_RGB3 ? 3 : layout == AIF_FOCUS_RGB4 ? 4 : layout == AIF_FOCUS_YUY2 ? 2 : 1;
  if (row_bytes % (channels * size))
    return AIF_FOCUS_INVALID;
  const int width = row_bytes / (channels * size);
  auto* dst = static_cast<uint8_t*>(destination);
  auto* src = static_cast<const uint8_t*>(source);
  cpu = usable_cpu(cpu, dst, destination_stride) & usable_cpu(cpu, src, source_stride);
  if (const auto* hw = aif::focus::backend(cpu)) {
    hw->horizontal(src, source_stride, dst, destination_stride, row_bytes, height, bits, layout, half_amount,
                   float_amount, cpu);
    return AIF_FOCUS_OK;
  }
  copy_rows(src, source_stride, dst, destination_stride, row_bytes, height);
  if (layout == AIF_FOCUS_PLANAR) {
    {
      if (size == 1)
        af_horizontal_planar_c<uint8_t>(dst, height, destination_stride, row_bytes, half_amount, bits);
      else if (size == 2)
        af_horizontal_planar_c<uint16_t>(dst, height, destination_stride, row_bytes, half_amount, bits);
      else
        af_horizontal_planar_float_c(dst, height, destination_stride, row_bytes, float_amount);
    }
  } else if (layout == AIF_FOCUS_YUY2) {
    af_horizontal_yuy2_c(dst, height, destination_stride, width, half_amount);
  } else if (layout == AIF_FOCUS_RGB4) {
    {
      if (size == 1)
        af_horizontal_rgb32_64_c<uint8_t>(dst, height, destination_stride, width, half_amount);
      else
        af_horizontal_rgb32_64_c<uint16_t>(dst, height, destination_stride, width, half_amount);
    }
  } else {
    if (size == 1)
      af_horizontal_rgb24_48_c<uint8_t>(dst, height, destination_stride, width, half_amount);
    else
      af_horizontal_rgb24_48_c<uint16_t>(dst, height, destination_stride, width, half_amount);
  }
  return AIF_FOCUS_OK;
}

extern "C" int aif_focus_temporal_line(uint8_t* center, const uint8_t* const* neighbors, int count, int row_bytes,
                                       int bits, int layout, unsigned luma, unsigned chroma, uint32_t cpu) {
  if (!valid_plane(center, row_bytes, row_bytes, 1, bits) || count < 0 || count > 14 || (count && !neighbors) ||
      luma > 255 || chroma > 255 || (layout != AIF_FOCUS_PLANAR && layout != AIF_FOCUS_YUY2) ||
      (layout == AIF_FOCUS_YUY2 && (bits != 8 || row_bytes % 4)))
    return AIF_FOCUS_INVALID;
  const uint8_t* input[14];
  cpu = aligned(center, 32) ? cpu & aif_focus_supported_cpu() : 0;
  for (int i = 0; i < count; ++i) {
    if (!valid_plane(neighbors[i], row_bytes, row_bytes, 1, bits) || neighbors[i] == center)
      return AIF_FOCUS_INVALID;
    input[i] = neighbors[i];
    if (!aligned(input[i], 32))
      cpu = 0;
  }
  if (!count)
    return AIF_FOCUS_OK;
  if (const auto* hw = aif::focus::backend(cpu)) {
    hw->temporal(center, input, count, row_bytes, bits, layout, luma, chroma, cpu);
    return AIF_FOCUS_OK;
  }
  const int divisor = 32768 / (count + 1);
  if (layout == AIF_FOCUS_YUY2)
    accumulate_line_yuy2(center, input, count, row_bytes, static_cast<uint8_t>(luma), static_cast<uint8_t>(chroma),
                         divisor);
  else
    accumulate_line(center, input, count, row_bytes, static_cast<uint8_t>(luma), divisor, pixel_size(bits), bits);
  return AIF_FOCUS_OK;
}

extern "C" int aif_focus_sad(const void* a, const void* b, int sa, int sb, int row_bytes, int height, int bits,
                             uint32_t cpu, int64_t* result) {
  if (!result || !valid_plane(a, sa, row_bytes, height, bits) || !valid_plane(b, sb, row_bytes, height, bits))
    return AIF_FOCUS_INVALID;
  cpu = usable_cpu(cpu, a, sa) & usable_cpu(cpu, b, sb);
  // Bound the original int row accumulator. Large integer frames use a 64-bit
  // scalar sum, avoiding the legacy byte-SAD whole-frame int overflow.
  const int bytes = pixel_size(bits);
  const int64_t peak = bits == 32 ? 0 : (int64_t(1) << bits) - 1;
  const int64_t samples = int64_t(row_bytes / bytes) * height;
  if (bits != 32 && samples > std::numeric_limits<int64_t>::max() / peak)
    return AIF_FOCUS_INVALID;
  if (bits == 32) {
    double sum = 0;
    for (int y = 0; y < height; ++y) {
      const auto* aa = reinterpret_cast<const float*>(static_cast<const uint8_t*>(a) + ptrdiff_t(y) * sa);
      const auto* bb = reinterpret_cast<const float*>(static_cast<const uint8_t*>(b) + ptrdiff_t(y) * sb);
      float row_sum = 0;
      for (int x = 0; x < row_bytes / 4; ++x)
        row_sum += std::abs(aa[x] - bb[x]);
      sum += row_sum;
    }
    const double scaled = sum * 255;
    if (!std::isfinite(scaled) || scaled >= 9223372036854775808.0)
      return AIF_FOCUS_INVALID;
    *result = static_cast<int64_t>(scaled);
  } else if (const auto* hw = aif::focus::backend(cpu)) {
    *result = hw->sad(static_cast<const uint8_t*>(a), static_cast<const uint8_t*>(b), sa, sb, row_bytes, height, bits);
  } else if (samples > std::numeric_limits<int>::max() / peak) {
    int64_t sum = 0;
    for (int y = 0; y < height; ++y) {
      const auto* aa = static_cast<const uint8_t*>(a) + ptrdiff_t(y) * sa;
      const auto* bb = static_cast<const uint8_t*>(b) + ptrdiff_t(y) * sb;
      for (int x = 0; x < row_bytes / bytes; ++x)
        sum += bits == 8 ? std::abs(int(aa[x]) - int(bb[x]))
                         : std::abs(int(reinterpret_cast<const uint16_t*>(aa)[x]) -
                                    int(reinterpret_cast<const uint16_t*>(bb)[x]));
    }
    *result = sum >> (bits - 8);
  } else {
    *result = calculate_sad(static_cast<const uint8_t*>(a), static_cast<const uint8_t*>(b), sa, sb, row_bytes, height,
                            bytes, bits);
  }
  return AIF_FOCUS_OK;
}

extern "C" int aif_focus_spatial_yuy2(const uint8_t* srcp, int src_pitch, uint8_t* dstp, int dst_pitch, int row_size,
                                      int height, int radius, unsigned luma_threshold, unsigned chroma_threshold) {
  if (!valid_plane(srcp, src_pitch, row_size, height, 8) || !valid_plane(dstp, dst_pitch, row_size, height, 8) ||
      srcp == dstp || row_size % 4 || radius < 0 || radius > 32 || luma_threshold > 255 || chroma_threshold > 255)
    return AIF_FOCUS_INVALID;
  const int diameter = radius * 2 + 1;
#include "spatial_body.inc"
  return AIF_FOCUS_OK;
}

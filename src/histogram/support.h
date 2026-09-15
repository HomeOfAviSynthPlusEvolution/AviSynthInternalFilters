#pragma once
#include <avisynth.h>
#include <algorithm>
namespace aif::filters::histogram {
inline float c8tof(int v) {
  return float(v) / 255.0f;
}
inline float uv8tof(int v) {
  return float(v - 128) / 255.0f;
}
template <class T>
void fill_plane(uint8_t* p, int h, int row, int pitch, T value) {
  for (int y = 0; y < h; ++y)
    std::fill_n(reinterpret_cast<T*>(p + ptrdiff_t(y) * pitch), row / sizeof(T), value);
}
template <class T>
void fill_chroma(uint8_t* u, uint8_t* v, int h, int row, int pitch, T value) {
  fill_plane(u, h, row, pitch, value);
  fill_plane(v, h, row, pitch, value);
}
inline void initialize_alpha(PVideoFrame& dst, const PVideoFrame& src, const VideoInfo& vi, bool keep,
                             IScriptEnvironment* env) {
  if (!vi.IsYUVA() && !vi.IsPlanarRGBA())
    return;
  auto* p = dst->GetWritePtr(PLANAR_A);
  int h = dst->GetHeight(PLANAR_A), row = dst->GetRowSize(PLANAR_A), pitch = dst->GetPitch(PLANAR_A);
  if (vi.ComponentSize() == 1)
    fill_plane<uint8_t>(p, h, row, pitch, 255);
  else if (vi.ComponentSize() == 2)
    fill_plane<uint16_t>(p, h, row, pitch, uint16_t((1u << vi.BitsPerComponent()) - 1));
  else
    fill_plane<float>(p, h, row, pitch, 1.0f);
  if (keep)
    env->BitBlt(p, pitch, src->GetReadPtr(PLANAR_A), src->GetPitch(PLANAR_A), std::min(row, src->GetRowSize(PLANAR_A)),
                std::min(h, src->GetHeight(PLANAR_A)));
}
void DrawStringPlanar(VideoInfo&, PVideoFrame&, int, int, const char*, IScriptEnvironment*);
} // namespace aif::filters::histogram

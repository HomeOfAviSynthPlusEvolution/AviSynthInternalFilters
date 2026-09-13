// SPDX-License-Identifier: GPL-2.0-or-later
#include "color_bars.h"
#include "single_frame.h"
#include "color_bars/kernel.h"
#include "kernel_adapter.h"
#include <array>
#include <cmath>
#include <cstring>
#include <string_view>
#include <new>
#define PI 3.1415926535897932384626433832795
namespace aif::filters::color_bars {
namespace {
struct PixelTypeName {
  int pixel_type;
  std::string_view name;
};

constexpr std::array kPixelTypeNames{
    PixelTypeName{VideoInfo::CS_BGR24, "RGB24"},
    PixelTypeName{VideoInfo::CS_BGR32, "RGB32"},
    PixelTypeName{VideoInfo::CS_YUY2, "YUY2"},
    PixelTypeName{VideoInfo::CS_YV24, "YV24"},
    PixelTypeName{VideoInfo::CS_YV16, "YV16"},
    PixelTypeName{VideoInfo::CS_YV12, "YV12"},
    PixelTypeName{VideoInfo::CS_I420, "YV12"},
    PixelTypeName{VideoInfo::CS_YUV9, "YUV9"},
    PixelTypeName{VideoInfo::CS_YV411, "YV411"},
    PixelTypeName{VideoInfo::CS_Y8, "Y8"},
    PixelTypeName{VideoInfo::CS_YUV420P10, "YUV420P10"},
    PixelTypeName{VideoInfo::CS_YUV422P10, "YUV422P10"},
    PixelTypeName{VideoInfo::CS_YUV444P10, "YUV444P10"},
    PixelTypeName{VideoInfo::CS_Y10, "Y10"},
    PixelTypeName{VideoInfo::CS_YUV420P12, "YUV420P12"},
    PixelTypeName{VideoInfo::CS_YUV422P12, "YUV422P12"},
    PixelTypeName{VideoInfo::CS_YUV444P12, "YUV444P12"},
    PixelTypeName{VideoInfo::CS_Y12, "Y12"},
    PixelTypeName{VideoInfo::CS_YUV420P14, "YUV420P14"},
    PixelTypeName{VideoInfo::CS_YUV422P14, "YUV422P14"},
    PixelTypeName{VideoInfo::CS_YUV444P14, "YUV444P14"},
    PixelTypeName{VideoInfo::CS_Y14, "Y14"},
    PixelTypeName{VideoInfo::CS_YUV420P16, "YUV420P16"},
    PixelTypeName{VideoInfo::CS_YUV422P16, "YUV422P16"},
    PixelTypeName{VideoInfo::CS_YUV444P16, "YUV444P16"},
    PixelTypeName{VideoInfo::CS_Y16, "Y16"},
    PixelTypeName{VideoInfo::CS_YUV420PS, "YUV420PS"},
    PixelTypeName{VideoInfo::CS_YUV422PS, "YUV422PS"},
    PixelTypeName{VideoInfo::CS_YUV444PS, "YUV444PS"},
    PixelTypeName{VideoInfo::CS_Y32, "Y32"},
    PixelTypeName{VideoInfo::CS_BGR48, "RGB48"},
    PixelTypeName{VideoInfo::CS_BGR64, "RGB64"},
    PixelTypeName{VideoInfo::CS_RGBP, "RGBP"},
    PixelTypeName{VideoInfo::CS_RGBP10, "RGBP10"},
    PixelTypeName{VideoInfo::CS_RGBP12, "RGBP12"},
    PixelTypeName{VideoInfo::CS_RGBP14, "RGBP14"},
    PixelTypeName{VideoInfo::CS_RGBP16, "RGBP16"},
    PixelTypeName{VideoInfo::CS_RGBPS, "RGBPS"},
    PixelTypeName{VideoInfo::CS_YUVA420, "YUVA420"},
    PixelTypeName{VideoInfo::CS_YUVA422, "YUVA422"},
    PixelTypeName{VideoInfo::CS_YUVA444, "YUVA444"},
    PixelTypeName{VideoInfo::CS_YUVA420P10, "YUVA420P10"},
    PixelTypeName{VideoInfo::CS_YUVA422P10, "YUVA422P10"},
    PixelTypeName{VideoInfo::CS_YUVA444P10, "YUVA444P10"},
    PixelTypeName{VideoInfo::CS_YUVA420P12, "YUVA420P12"},
    PixelTypeName{VideoInfo::CS_YUVA422P12, "YUVA422P12"},
    PixelTypeName{VideoInfo::CS_YUVA444P12, "YUVA444P12"},
    PixelTypeName{VideoInfo::CS_YUVA420P14, "YUVA420P14"},
    PixelTypeName{VideoInfo::CS_YUVA422P14, "YUVA422P14"},
    PixelTypeName{VideoInfo::CS_YUVA444P14, "YUVA444P14"},
    PixelTypeName{VideoInfo::CS_YUVA420P16, "YUVA420P16"},
    PixelTypeName{VideoInfo::CS_YUVA422P16, "YUVA422P16"},
    PixelTypeName{VideoInfo::CS_YUVA444P16, "YUVA444P16"},
    PixelTypeName{VideoInfo::CS_YUVA420PS, "YUVA420PS"},
    PixelTypeName{VideoInfo::CS_YUVA422PS, "YUVA422PS"},
    PixelTypeName{VideoInfo::CS_YUVA444PS, "YUVA444PS"},
    PixelTypeName{VideoInfo::CS_RGBAP, "RGBAP"},
    PixelTypeName{VideoInfo::CS_RGBAP10, "RGBAP10"},
    PixelTypeName{VideoInfo::CS_RGBAP12, "RGBAP12"},
    PixelTypeName{VideoInfo::CS_RGBAP14, "RGBAP14"},
    PixelTypeName{VideoInfo::CS_RGBAP16, "RGBAP16"},
    PixelTypeName{VideoInfo::CS_RGBAPS, "RGBAPS"},
    PixelTypeName{VideoInfo::CS_YV24, "YUV444"},
    PixelTypeName{VideoInfo::CS_YV16, "YUV422"},
    PixelTypeName{VideoInfo::CS_YV12, "YUV420"},
    PixelTypeName{VideoInfo::CS_YV411, "YUV411"},
    PixelTypeName{VideoInfo::CS_RGBP, "RGBP8"},
    PixelTypeName{VideoInfo::CS_RGBAP, "RGBAP8"},
    PixelTypeName{VideoInfo::CS_YV24, "YUV444P8"},
    PixelTypeName{VideoInfo::CS_YV16, "YUV422P8"},
    PixelTypeName{VideoInfo::CS_YV12, "YUV420P8"},
    PixelTypeName{VideoInfo::CS_YV411, "YUV411P8"},
    PixelTypeName{VideoInfo::CS_YUVA420, "YUVA420P8"},
    PixelTypeName{VideoInfo::CS_YUVA422, "YUVA422P8"},
    PixelTypeName{VideoInfo::CS_YUVA444, "YUVA444P8"},
};

[[nodiscard]] bool equals_ascii_ignore_case(const std::string_view left, const std::string_view right) noexcept {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t index = 0; index < left.size(); ++index) {
    const auto to_upper = [](const char value) {
      return value >= 'a' && value <= 'z' ? static_cast<char>(value - ('a' - 'A')) : value;
    };
    if (to_upper(left[index]) != to_upper(right[index])) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] int pixel_type_from_name(const char* const name) noexcept {
  const std::string_view requested(name);
  for (const auto& candidate : kPixelTypeNames) {
    if (equals_ascii_ignore_case(requested, candidate.name)) {
      return candidate.pixel_type;
    }
  }
  return VideoInfo::CS_UNKNOWN;
}

} // namespace
ColorBars::~ColorBars() {
  delete[] audio;
}
ColorBars::ColorBars(int w, int h, const char* pixel_type, bool _staticframes, int type, IScriptEnvironment* env) {
  memset(&vi, 0, sizeof(VideoInfo));
  staticframes = _staticframes; // P.F.
  vi.width = w;
  vi.height = h;
  vi.fps_numerator = 30000;
  vi.fps_denominator = 1001;
  vi.num_frames = 107892; // 1 hour
  int i_pixel_type = pixel_type_from_name(pixel_type);
  vi.pixel_type = i_pixel_type;

  if (type) { // ColorbarsHD
    if (!vi.Is444())
      env->ThrowError("ColorBarsHD: pixel_type must be \"YV24\" or other 4:4:4 video format");
  } else if (vi.IsRGB32() || vi.IsRGB64() || vi.IsRGB24() || vi.IsRGB48()) {
    // no special check
  } else if (vi.IsRGB() && vi.IsPlanar()) { // planar RGB
    // no special check
  } else if (vi.IsYUY2()) { // YUY2
    if (w & 1)
      env->ThrowError("ColorBars: YUY2 width must be even!");
  } else if (vi.Is420()) { // 4:2:0
    if ((w & 1) || (h & 1))
      env->ThrowError("ColorBars: for 4:2:0 both height and width must be even!");
  } else if (vi.Is422()) { // 4:2:2
    if (w & 1)
      env->ThrowError("ColorBars: for 4:2:2 width must be even!");
  } else if (vi.IsYV411()) { // 4:1:1
    if (w & 3)
      env->ThrowError("ColorBars: for 4:1:1 width must be divisible by 4!");
  } else if (vi.Is444()) { // 4:4:4
                           // no special check
  } else {
    env->ThrowError("ColorBars: this pixel_type not supported");
  }
  vi.sample_type = SAMPLE_FLOAT;
  vi.nchannels = 2;
  vi.audio_samples_per_second = 48000;
  vi.num_audio_samples = vi.AudioSamplesFromFrames(vi.num_frames);

  frame = env->NewVideoFrame(vi);

  // set basic frame properties
  auto props = env->getFramePropsRW(frame);
  int theMatrix;
  int theColorRange;
  if (type) {
    // ColorBarsHD 444 only
    theMatrix = 1;
    theColorRange = 1;
  } else {
    // ColorBars can be rgb or yuv
    theMatrix = vi.IsRGB() ? 0 : 6;
    // Studio RGB: limited!
    theColorRange = vi.IsRGB() ? 1 : 1;
  }
  env->propSetInt(props, "_Matrix", theMatrix, 0);
  env->propSetInt(props, "_ColorRange", theColorRange, 0);

  const int layout = vi.Is444()                     ? 0
                     : vi.Is422()                   ? 1
                     : vi.Is420()                   ? 2
                     : vi.IsYV411()                 ? 3
                     : vi.IsPlanar()                ? 4
                     : vi.IsRGB24() || vi.IsRGB48() ? 5
                     : vi.IsRGB32() || vi.IsRGB64() ? 6
                                                    : 7;
  const bool rgb = vi.IsPlanarRGB() || vi.IsPlanarRGBA();
  const int order[] = {rgb ? PLANAR_R : PLANAR_Y, rgb ? PLANAR_G : PLANAR_U, rgb ? PLANAR_B : PLANAR_V, PLANAR_A};
  uint8_t* ptrs[4] = {};
  int pitches[4] = {};
  for (int p = 0; p < (vi.IsPlanar() ? vi.NumComponents() : 1); ++p) {
    ptrs[p] = frame->GetWritePtr(order[p]);
    pitches[p] = frame->GetPitch(order[p]);
  }
  if (aif_color_bars_draw(ptrs, pitches, w, h, vi.BitsPerComponent(), layout, type, allowed_cpu(env)))
    env->ThrowError("ColorBars: invalid kernel geometry");
  // Generate Audio buffer
  {
    unsigned x = vi.audio_samples_per_second, y = Hz;
    while (y) { // find gcd
      unsigned t = x % y;
      x = y;
      y = t;
    }
    nsamples = vi.audio_samples_per_second / x; // 1200
    const unsigned ncycles = Hz / x;            // 11

    audio = new (std::nothrow) float[nsamples];
    if (!audio)
      env->ThrowError("ColorBars: insufficient memory");

    const double add_per_sample = ncycles / (double)nsamples;
    double second_offset = 0.0;
    for (unsigned i = 0; i < nsamples; i++) {
      audio[i] = (float)sin(PI * 2.0 * second_offset);
      second_offset += add_per_sample;
    }
  }
}
PVideoFrame __stdcall ColorBars::GetFrame(int n, IScriptEnvironment* env) {
  (void)n;
  if (staticframes)
    return frame; // original default method returns precomputed static frame.
  else {
    PVideoFrame result = env->NewVideoFrameP(vi, &frame);
    env->BitBlt(result->GetWritePtr(), result->GetPitch(), frame->GetReadPtr(), frame->GetPitch(), frame->GetRowSize(),
                frame->GetHeight());
    env->BitBlt(result->GetWritePtr(PLANAR_V), result->GetPitch(PLANAR_V), frame->GetReadPtr(PLANAR_V),
                frame->GetPitch(PLANAR_V), frame->GetRowSize(PLANAR_V), frame->GetHeight(PLANAR_V));
    env->BitBlt(result->GetWritePtr(PLANAR_U), result->GetPitch(PLANAR_U), frame->GetReadPtr(PLANAR_U),
                frame->GetPitch(PLANAR_U), frame->GetRowSize(PLANAR_U), frame->GetHeight(PLANAR_U));
    env->BitBlt(result->GetWritePtr(PLANAR_A), result->GetPitch(PLANAR_A), frame->GetReadPtr(PLANAR_A),
                frame->GetPitch(PLANAR_A), frame->GetRowSize(PLANAR_A), frame->GetHeight(PLANAR_A));
    return result;
  }
}
bool __stdcall ColorBars::GetParity(int n) {
  (void)n;
  return false;
}
const VideoInfo& __stdcall ColorBars::GetVideoInfo() {
  return vi;
}
int __stdcall ColorBars::SetCacheHints(int cachehints, int frame_range) {
  (void)frame_range;
  switch (cachehints) {
    case CACHE_GET_MTMODE:
      return MT_NICE_FILTER;
    case CACHE_DONT_CACHE_ME:
      return 1;
    default:
      return 0;
  }
}
void ColorBars::FillAudioZeros(void* buf, int start_offset, int count) {
  const int bps = vi.BytesPerAudioSample();
  unsigned char* byte_buf = (unsigned char*)buf;
  memset(byte_buf + start_offset * bps, 0, count * bps);
}
void __stdcall ColorBars::GetAudio(void* buf, int64_t start, int64_t count, IScriptEnvironment* env) {
  (void)env;
#if 1
  // This filter is non-cached so we guard against negative start and overread, like in Cache::GetAudio
  if ((start + count <= 0) || (start >= vi.num_audio_samples)) {
    // Completely skip.
    FillAudioZeros(buf, 0, (int)count);
    count = 0;
    return;
  }

  if (start < 0) {                       // Partial initial skip
    FillAudioZeros(buf, 0, (int)-start); // Fill all samples before 0 with silence.
    count += start;                      // Subtract start bytes from count.
    buf = ((BYTE*)buf) - (int)(start * vi.BytesPerAudioSample());
    start = 0;
  }

  if (start + count > vi.num_audio_samples) { // Partial ending skip
    FillAudioZeros(buf, (int)(vi.num_audio_samples - start),
                   (int)(count - (vi.num_audio_samples - start))); // Fill end samples
    count = (vi.num_audio_samples - start);
  }

  const int d_mod = vi.audio_samples_per_second * 2;
  float* samples = (float*)buf;

  unsigned j = (unsigned)(start % nsamples);
  for (int i = 0; i < count; i++) {
    samples[i * 2] = audio[j];
    if (((start + i) % d_mod) > vi.audio_samples_per_second) {
      samples[i * 2 + 1] = audio[j];
    } else {
      samples[i * 2 + 1] = 0;
    }
    if (++j >= nsamples)
      j = 0;
  }
#else
  int64_t Hz = 440;
  // Calculate what start equates in cycles.
  // This is the number of cycles (rounded down) that has already been taken.
  int64_t startcycle = (start * Hz) / vi.audio_samples_per_second;

  // Move offset down - this is to avoid float rounding errors
  int start_offset = (int)(start - ((startcycle * vi.audio_samples_per_second) / Hz));

  double add_per_sample = Hz / (double)vi.audio_samples_per_second;
  double second_offset = ((double)start_offset * add_per_sample);
  int d_mod = vi.audio_samples_per_second * 2;
  float* samples = (float*)buf;

  for (int i = 0; i < count; i++) {
    samples[i * 2] = (float)sin(PI * 2.0 * second_offset);
    if (((start + i) % d_mod) > vi.audio_samples_per_second) {
      samples[i * 2 + 1] = samples[i * 2];
    } else {
      samples[i * 2 + 1] = 0;
    }
    second_offset += add_per_sample;
  }
#endif
}
AVSValue __cdecl ColorBars::Create(AVSValue args, void* _type, IScriptEnvironment* env) {
  const int type = (int)(size_t)_type;
  bool staticframes = args[3].AsBool(true);

  PClip clip = new ColorBars(args[0].AsInt(type ? 1288 : 640), args[1].AsInt(type ? 720 : 480),
                             args[2].AsString(type ? "YV24" : "RGB32"),
                             staticframes, // new staticframes parameter
                             type, env);
  // wrap in OnCPU to support multi devices
  AVSValue arg[2]{clip, 1}; // prefetch=1: enable cache but not thread
  AVSValue ret = env->Invoke("OnCPU", AVSValue(arg, 2));
  if (staticframes) {
    return new SingleFrame(ret.AsClip());
  }
  return ret;
}
} // namespace aif::filters::color_bars

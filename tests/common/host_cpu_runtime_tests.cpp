// SPDX-License-Identifier: GPL-2.0-or-later
#define NOMINMAX
#include "../../src/common/host_cpu.h"
#include "../../src/common/host_properties.h"
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
const AVS_Linkage* AVS_linkage = nullptr;

uint64_t pixels(PClip clip, IScriptEnvironment* env) {
  const auto& vi = clip->GetVideoInfo();
  const int yuv[] = {PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A};
  const int rgb[] = {PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A};
  const auto* planes = vi.IsPlanarRGB() || vi.IsPlanarRGBA() ? rgb : yuv;
  uint64_t hash = 14695981039346656037ULL;
  for (int n = 0; n < 2; ++n) {
    auto frame = clip->GetFrame(n, env);
    for (int p = 0; p < (vi.IsPlanar() ? vi.NumComponents() : 1); ++p) {
      const int plane = vi.IsPlanar() ? planes[p] : 0;
      for (int y = 0; y < frame->GetHeight(plane); ++y)
        for (int x = 0; x < frame->GetRowSize(plane); ++x)
          hash = (hash ^ frame->GetReadPtr(plane)[y * frame->GetPitch(plane) + x]) * 1099511628211ULL;
    }
  }
  return hash;
}
int main(int argc, char** argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 3)
    return 2;
#ifdef _WIN32
  auto library = LoadLibraryA(argv[1]);
  auto symbol = library ? GetProcAddress(library, "CreateScriptEnvironment") : nullptr;
#else
  auto library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  auto symbol = library ? dlsym(library, "CreateScriptEnvironment") : nullptr;
#endif
  using Create = IScriptEnvironment*(__stdcall*)(int);
  auto create = reinterpret_cast<Create>(symbol);
  if (!create)
    return 3;
  // Compile with current headers, explicitly request the oldest supported host.
  auto env = create(8);
  if (!env)
    return 4;
  AVS_linkage = env->GetAVSLinkage();
  int result = 0;
  try {
    for (int i = 2; i < argc; ++i)
      env->Invoke("LoadPlugin", argv[i]);
    bool v12 = true;
    try {
      env->CheckVersion(12);
    } catch (const AvisynthError&) {
      v12 = false;
    }
    const auto expected = v12 ? uint64_t(env->GetCPUFlagsEx()) : uint64_t(uint32_t(env->GetCPUFlags()));
    if (aif::filters::host_cpu_flags(env) != expected)
      throw std::runtime_error("CPU mask mismatch");
    std::puts("checking saturated properties");
    AVSMap* map = env->createMap();
    for (const int64_t value : {INT64_MIN, int64_t(INT32_MIN), int64_t(17), int64_t(INT32_MAX), INT64_MAX}) {
      env->propSetInt(map, "test", value, PROPAPPENDMODE_REPLACE);
      int error = -1;
      const auto want = value < INT32_MIN ? INT32_MIN : value > INT32_MAX ? INT32_MAX : int(value);
      if (aif::filters::property_int(env, map, "test", 0, &error) != want || error != 0)
        throw std::runtime_error("saturated property mismatch");
    }
    int error = 0;
    if (aif::filters::property_int(env, map, "missing", 0, &error) != 0 || error == 0)
      throw std::runtime_error("missing property error lost");
    env->freeMap(map);
    std::puts("property checks passed");
    if (!env->FunctionExists("MultiOverlay")) {
      bool rejected = false;
      try {
        env->Invoke("Eval", "AddBorders(BlankClip(width=32,height=16),2,2,2,2,r=1)");
      } catch (const AvisynthError& e) {
        rejected = std::string(e.msg).find("requires a host with MultiOverlay") != std::string::npos;
      }
      if (!rejected)
        throw std::runtime_error("missing transient resize dependency not diagnosed");
    }
    const char* cases[] = {"Sharpen(c,0.5)",
                           "TemporalSoften(c,1,8,8)",
                           "SpatialSoften(y,1,8,8)",
                           "TurnLeft(c)",
                           "FlipHorizontal(c)",
                           "FlipVertical(c)",
                           "AddBorders(c,2,2,2,2)",
                           "SwapUV(y)",
                           "CombinePlanes(c,planes=\"YUV\",source_planes=\"YUV\")",
                           "CombinePlanes(c,planes=\"RGB\",source_planes=\"YUV\",pixel_type=\"RGBP\")",
                           "ExtractY(c)",
                           "SeparateRows(c,2)",
                           "WeaveRows(c,2)",
                           "Crop(c,2,2,-2,-2)",
                           "StackHorizontal(c,c)",
                           "Sharpen(BlankClip(width=32,height=16,length=4,pixel_type=\"YUV444P16\"),0.5)",
                           "Invert(BlankClip(width=32,height=16,length=4,pixel_type=\"RGBAP16\"))",
                           "TurnRight(BlankClip(width=32,height=16,length=4,pixel_type=\"RGBAPS\"))",
                           "UToY(y)",
                           "YToUV(UToY8(y),VToY8(y))",
                           "ShowRed(r)",
                           "MergeRGB(c,c,c)",
                           "ShowFiveVersions(c,c,c,c,c)",
                           "SeparateColumns(c,2)",
                           "WeaveColumns(c,2)",
                           "Levels(c,0,1.2,255,0,255)",
                           "Tweak(y,sat=0.7)",
                           "RGBAdjust(r,r=0.8)",
                           "ColorYUV(y,gain_y=8)",
                           "GeneralConvolution(c,matrix=\"1 1 1 1 1 1 1 1 1\")",
                           "Merge(c,c,0.25)",
                           "Greyscale(r)",
                           "Invert(c)",
                           "Limiter(c)",
                           "BlankClip(width=32,height=16,length=4,pixel_type=\"YUV420P8\",color=$123456)",
                           "ColorBars(width=640,height=480,pixel_type=\"RGB32\")"};
    const std::string setup = "c=BlankClip(width=32,height=16,length=4,pixel_type=\"YUV444P8\",color=$345678)\ny="
                              "ConvertToYUY2(c)\nr=ConvertToRGB32(c)\n";
    for (const auto* expr : cases) {
      std::vector<uint64_t> hashes;
      for (const char* mode : {"", "none"}) {
        env->Invoke("SetMaxCPU", mode);
        const std::string script = setup + expr;
        PClip clip = env->Invoke("Eval", script.c_str()).AsClip();
        hashes.push_back(pixels(clip, env));
      }
      if (hashes[0] != hashes[1])
        throw std::runtime_error(std::string("native/scalar mismatch: ") + expr);
      std::printf("PASS %s\n", expr);
    }
    std::printf("host v%s: 36 cases, native/scalar, repeated frames passed\n", v12 ? "12+" : "8-11");
  } catch (const AvisynthError& e) {
    std::fprintf(stderr, "%s\n", e.msg);
    result = 1;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    result = 1;
  }
  env->DeleteScriptEnvironment();
  return result;
}

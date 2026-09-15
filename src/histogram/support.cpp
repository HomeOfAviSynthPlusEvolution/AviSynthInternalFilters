#include "support.h"
#include "text_frame.h"
namespace aif::filters::histogram {
void DrawStringPlanar(VideoInfo& vi, PVideoFrame& frame, int x, int y, const char* text, IScriptEnvironment* env) {
  PVideoFrame rendered;
  {
    PClip input(new TextFrame(vi, frame));
    // info_h, baseline-left, white luma 230, background fade, no halo.
    const AVSValue args[] = {input, text, double(x), double(y), 0, 0, "info_h", 20.0, 0xF9F9F9, int(0xFF000000u), 4};
    auto filter = env->Invoke("Text", AVSValue(args, 11)).AsClip();
    rendered = filter->GetFrame(0, env);
  }
  const int planes[] = {PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A};
  for (int c = 0; c < vi.NumComponents(); ++c) {
    const int p = planes[c];
    env->BitBlt(frame->GetWritePtr(p), frame->GetPitch(p), rendered->GetReadPtr(p), rendered->GetPitch(p),
                frame->GetRowSize(p), frame->GetHeight(p));
  }
}
} // namespace aif::filters::histogram

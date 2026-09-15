#define main stack_test_main
#include "../../tests/stack/filter_tests.cpp"
#undef main
#include <chrono>
int main(int argc, char** argv) {
  if (argc != 3)
    return 2;
  Runtime runtime(argv[1]);
  Environment holder(runtime);
  auto env = holder.env;
  env->Invoke("LoadPlugin", argv[2]);
  PClip source(new Sequence(env, VideoInfo::CS_YV12, 960, 1080, false));
  AVSValue args[] = {source, source};
  auto filter = env->Invoke("StackHorizontal", AVSValue(args, 2)).AsClip();
  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 50; ++i) {
    auto frame = filter->GetFrame(i % 5, env);
  }
  std::printf("StackHorizontal 1080p: %.3f ms\n",
              std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / 50);
}

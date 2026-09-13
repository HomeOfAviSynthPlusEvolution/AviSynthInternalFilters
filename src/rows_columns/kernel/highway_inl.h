HWY_BEFORE_NAMESPACE();
namespace aif::rows_columns {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;
template <class T, bool separate = false>
void Typed(const uint8_t* const* src, uint8_t* dst, int count, int period, int phase, int weave) {
  const hn::CappedTag<T, (separate ? 32 : HWY_MAX_BYTES) / sizeof(T)> d;
  int n = int(hn::Lanes(d)), x = 0;
  T* out = reinterpret_cast<T*>(dst);
  for (; x <= count - n; x += n) {
    auto a = hn::Zero(d), b = a, c = a, e = a;
    if (weave) {
      a = hn::LoadU(d, reinterpret_cast<const T*>(src[0]) + x);
      b = hn::LoadU(d, reinterpret_cast<const T*>(src[1]) + x);
      if (period >= 3)
        c = hn::LoadU(d, reinterpret_cast<const T*>(src[2]) + x);
      if (period == 4)
        e = hn::LoadU(d, reinterpret_cast<const T*>(src[3]) + x);
      if (period == 2)
        hn::StoreInterleaved2(a, b, d, out + x * period);
      else if (period == 3)
        hn::StoreInterleaved3(a, b, c, d, out + x * period);
      else
        hn::StoreInterleaved4(a, b, c, e, d, out + x * period);
    } else {
      const T* in = reinterpret_cast<const T*>(src[0]) + x * period;
      if (period == 2)
        hn::LoadInterleaved2(d, in, a, b);
      else if (period == 3)
        hn::LoadInterleaved3(d, in, a, b, c);
      else
        hn::LoadInterleaved4(d, in, a, b, c, e);
      hn::StoreU(phase == 0 ? a : phase == 1 ? b : phase == 2 ? c : e, d, out + x);
    }
  }
  const uint8_t* tail[4] = {};
  for (int c = 0; c < (weave ? period : 1); ++c)
    tail[c] = src[c] + x * sizeof(T) * (weave ? 1 : period);
  scalar(tail, dst + x * sizeof(T) * (weave ? period : 1), count - x, sizeof(T), period, phase, weave);
}
void ProcessRow(const uint8_t* const* src, uint8_t* dst, int count, int size, int period, int phase, int weave) {
  if (period < 2 || period > 4) {
    scalar(src, dst, count, size, period, phase, weave);
    return;
  }
  switch (size) {
    case 1:
      if (weave || period == 2)
        Typed<uint8_t>(src, dst, count, period, phase, weave);
      else
        Typed<uint8_t, true>(src, dst, count, period, phase, weave);
      break;
    case 2:
      if (weave)
        Typed<uint16_t>(src, dst, count, period, phase, weave);
      else
        Typed<uint16_t, true>(src, dst, count, period, phase, weave);
      break;
    case 4:
      if (weave || period == 4)
        Typed<uint32_t>(src, dst, count, period, phase, weave);
      else
        Typed<uint32_t, true>(src, dst, count, period, phase, weave);
      break;
    case 8:
      if (weave)
        Typed<uint64_t>(src, dst, count, period, phase, weave);
      else
        Typed<uint64_t, true>(src, dst, count, period, phase, weave);
      break;
    default:
      scalar(src, dst, count, size, period, phase, weave);
  }
}
} // namespace HWY_NAMESPACE
} // namespace aif::rows_columns
HWY_AFTER_NAMESPACE();

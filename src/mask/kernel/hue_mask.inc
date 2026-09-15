// show mask
if (child->GetVideoInfo().IsYUY2()) {
  const uint8_t* srcp = src->GetReadPtr();
  const int src_pitch = src->GetPitch();
  const int height = src->GetHeight();

#ifndef MaskPointResizing
  const int row_size = src->GetRowSize() >> 2;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < row_size; x++) {
      dstp[x] = mapUV[((srcp[x * 4 + 1]) << 8) | srcp[x * 4 + 3]];
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
#else
  const int row_size = src->GetRowSize();

  for (int y = 0; y < height; y++) {
    for (int xs = 0, xd = 0; xs < row_size; xs += 4, xd += 2) {
      const BYTE mapped = mapY[((srcp[xs + 1]) << 8) | srcp[xs + 3]];
      dstp[xd] = mapped;
      dstp[xd + 1] = mapped;
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
#endif
} else if (child->GetVideoInfo().IsPlanar()) {
  const int srcu_pitch = src->GetPitch(PLANAR_U);
  const uint8_t* srcpu = src->GetReadPtr(PLANAR_U);
  const uint8_t* srcpv = src->GetReadPtr(PLANAR_V);
  const int width = src->GetRowSize(PLANAR_U) / pixelsize;
  const int heightu = src->GetHeight(PLANAR_U);

#ifndef MaskPointResizing
  if (realcalc_chroma) {
    double uv_range_corr = (pixelsize == 4) ? 255.0 : 1.0 / (1 << (bits_per_pixel - 8));
    if (pixelsize == 1) {
      for (int y = 0; y < heightu; ++y) {
        for (int x = 0; x < width; ++x) {
          const double destu = srcpu[x] - limits.middle_chroma;
          const double destv = srcpv[x] - limits.middle_chroma;
          int iSat = 0; // won't be used in MaskHS; interpolation is skipped since p==0:
          bool ppres =
              ProcessPixel(destv * uv_range_corr, destu * uv_range_corr, dstartHue, dendHue, maxSat, minSat, 0.0, iSat);
          dstp[x] = ppres ? mask_high : mask_low;
        }
        dstp += dst_pitch;
        srcpu += srcu_pitch;
        srcpv += srcu_pitch;
      }
    } else if (pixelsize == 2) {
      for (int y = 0; y < heightu; ++y) {
        for (int x = 0; x < width; ++x) {
          const double destu = (reinterpret_cast<const uint16_t*>(srcpu)[x] - limits.middle_chroma);
          const double destv = (reinterpret_cast<const uint16_t*>(srcpv)[x] - limits.middle_chroma);
          int iSat = 0; // won't be used in MaskHS; interpolation is skipped since p==0:
          bool ppres =
              ProcessPixel(destv * uv_range_corr, destu * uv_range_corr, dstartHue, dendHue, maxSat, minSat, 0.0, iSat);
          reinterpret_cast<uint16_t*>(dstp)[x] = ppres ? mask_high : mask_low;
        }
        dstp += dst_pitch;
        srcpu += srcu_pitch;
        srcpv += srcu_pitch;
      }
    } else { // pixelsize == 4
      for (int y = 0; y < heightu; ++y) {
        for (int x = 0; x < width; ++x) {
          const double destu = (reinterpret_cast<const float*>(srcpu)[x] - limits.middle_chroma_f);
          const double destv = (reinterpret_cast<const float*>(srcpv)[x] - limits.middle_chroma_f);
          int iSat = 0; // won't be used in MaskHS; interpolation is skipped since p==0:
          bool ppres =
              ProcessPixel(destv * uv_range_corr, destu * uv_range_corr, dstartHue, dendHue, maxSat, minSat, 0.0, iSat);
          reinterpret_cast<float*>(dstp)[x] = ppres ? mask_high_f : mask_low_f;
        }
        dstp += dst_pitch;
        srcpu += srcu_pitch;
        srcpv += srcu_pitch;
      }
    }

  } else {
    // use LUT
    if (pixelsize == 1) {
      for (int y = 0; y < heightu; ++y) {
        for (int x = 0; x < width; ++x) {
          dstp[x] = mapUV[((srcpu[x]) << 8) | srcpv[x]];
        }
        dstp += dst_pitch;
        srcpu += srcu_pitch;
        srcpv += srcu_pitch;
      }
    } else if (pixelsize == 2) {
      for (int y = 0; y < heightu; ++y) {
        for (int x = 0; x < width; ++x) {
          reinterpret_cast<uint16_t*>(dstp)[x] =
              reinterpret_cast<uint16_t*>(mapUV)[((reinterpret_cast<const uint16_t*>(srcpu)[x]) << bits_per_pixel) |
                                                 reinterpret_cast<const uint16_t*>(srcpv)[x]];
        }
        dstp += dst_pitch;
        srcpu += srcu_pitch;
        srcpv += srcu_pitch;
      }
    } // no lut for float (and for 14-16 bit)
  }
#else
  const int swidth = child->GetVideoInfo().GetPlaneWidthSubsampling(PLANAR_U);
  const int sheight = child->GetVideoInfo().GetPlaneHeightSubsampling(PLANAR_U);
  const int sw = 1 << swidth;
  const int sh = 1 << sheight;

  const int dpitch = dst_pitch << sheight;
  for (int y = 0; y < heightu; ++y) {
    for (int x = 0; x < row_sizeu; ++x) {
      const BYTE mapped = mapY[((srcpu[x]) << 8) | srcpv[x]];
      const int sx = x << swidth;

      for (int lumv = 0; lumv < sh; ++lumv) {
        const int sy = lumv * dst_pitch + sx;

        for (int lumh = 0; lumh < sw; ++lumh) {
          dstp[sy + lumh] = mapped;
        }
      }
    }
    dstp += dpitch;
    srcpu += srcu_pitch;
    srcpv += srcu_pitch;
  }
#endif
}

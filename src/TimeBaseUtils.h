#pragma once

#include "common.h"

extern "C" {
#include "libavutil/avutil.h"
}

namespace FFmpegWrapper::TimeBaseUtils {

inline FFMPEG_WRAPPER_EXPORT double tsToSeconds(int64_t ts,
                                                const AVRational& tb) {
  if (ts == AV_NOPTS_VALUE) return -1.0;
  return static_cast<double>(ts) * av_q2d(tb);
}

inline FFMPEG_WRAPPER_EXPORT int64_t secondsToTs(double seconds,
                                                 const AVRational& tb) {
  if (seconds < 0) return -1;
  return static_cast<int64_t>(seconds / av_q2d(tb));
}

inline FFMPEG_WRAPPER_EXPORT int64_t rescaleTs(int64_t ts,
                                               const AVRational& srcTb,
                                               const AVRational& dstTb) {
  return av_rescale_q(ts, srcTb, dstTb);
}

}  // namespace FFmpegWrapper::TimeBaseUtils
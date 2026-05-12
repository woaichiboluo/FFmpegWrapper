#pragma once

#include <type_traits>

#include "Common.h"

extern "C" {
#include "libavutil/avutil.h"
#include "libavutil/channel_layout.h"
#include "libavutil/pixdesc.h"
#include "libavutil/pixfmt.h"
#include "libavutil/rational.h"
#include "libavutil/samplefmt.h"
}

namespace FFmpegWrapper {

namespace TimeBaseUtils {

inline double tsToSeconds(int64_t ts, const AVRational& tb) {
  if (ts == AV_NOPTS_VALUE) return -1.0;
  return static_cast<double>(ts) * av_q2d(tb);
}

inline int64_t secondsToTs(double seconds, const AVRational& tb) {
  if (seconds < 0) return -1;
  return static_cast<int64_t>(seconds / av_q2d(tb));
}

inline int64_t rescaleTs(int64_t ts, const AVRational& srcTb,
                         const AVRational& dstTb) {
  return av_rescale_q(ts, srcTb, dstTb);
}

// get the real value of time base, i.e., tb.num / tb.den
inline double getTbRealValue(const AVRational& rational) {
  return av_q2d(rational);
}

}  // namespace TimeBaseUtils

namespace CodecUtils {

template <typename T>
bool isCodecSupported(T fmt, const T* codecSupportedList) {
  if (!codecSupportedList) return true;
  auto checkEnd = [](const T& a) {
    if constexpr (std::is_same_v<T, AVPixelFormat>) {
      return a == AV_PIX_FMT_NONE;
    } else if constexpr (std::is_same_v<T, AVRational>) {
      return a.num == 0 && a.den == 0;
    } else if constexpr (std::is_same_v<T, int>) {
      return a == 0;
    } else if constexpr (std::is_same_v<T, AVSampleFormat>) {
      return a == AV_SAMPLE_FMT_NONE;
    } else if constexpr (std::is_same_v<T, AVChannelLayout>) {
      return a.nb_channels == 0 && a.u.mask == 0;
    } else if constexpr (std::is_same_v<T, AVColorRange>) {
      return a == AVCOL_RANGE_UNSPECIFIED;
    } else if constexpr (std::is_same_v<T, AVColorSpace>) {
      return a == AVCOL_SPC_UNSPECIFIED;
    } else if constexpr (std::is_same_v<T, AVAlphaMode>) {
      return a == AVALPHA_MODE_UNSPECIFIED;
    } else {
      static_assert(false, "Unsupported type");
    }
  };

  for (const T* p = codecSupportedList; p && !checkEnd(*p); ++p) {
    if constexpr (std::is_same_v<T, AVRational>) {
      if (fmt.num == p->num && fmt.den == p->den) return true;
    } else if constexpr (std::is_same_v<T, AVChannelLayout>) {
      if (fmt.nb_channels == p->nb_channels && fmt.u.mask == p->u.mask)
        return true;
    } else if (*p == fmt) {
      return true;
    }
  }
  return false;
}

template <typename T>
T useSourceOrDefault(T fmt, const T* codecSupportedList, T defaultFmt) {
  return isCodecSupported(fmt, codecSupportedList) ? fmt : defaultFmt;
}

}  // namespace CodecUtils

struct FFMPEG_WRAPPER_EXPORT VideoBaseInfo {
  AVPixelFormat pixFmt;
  int width;
  int height;
  AVRational frameRate;
  AVColorRange colorRange;
  AVColorSpace colorSpace;

  std::string dump() const {
    const char* pixFmtName = av_get_pix_fmt_name(pixFmt);
    const char* colorRangeName = av_color_range_name(colorRange);
    const char* colorSpaceName = av_color_space_name(colorSpace);
    return "VideoBaseInfo{pixFmt=" +
           (pixFmtName ? std::string(pixFmtName) : "unknown") +
           ", width=" + std::to_string(width) +
           ", height=" + std::to_string(height) +
           ", frameRate=" + std::to_string(frameRate.num) + "/" +
           std::to_string(frameRate.den) + "(" +
           std::to_string(TimeBaseUtils::getTbRealValue(frameRate)) +
           ")), colorRange=" +
           (colorRangeName ? std::string(colorRangeName) : "unknown") +
           ", colorSpace=" +
           (colorSpaceName ? std::string(colorSpaceName) : "unknown") + "}";
  }
};

struct FFMPEG_WRAPPER_EXPORT AudioBaseInfo {
  AVSampleFormat sampleFmt{};
  int sampleRate{};
  AVChannelLayout channelLayout{};

  AudioBaseInfo() = default;

  AudioBaseInfo(AVSampleFormat sampleFmt, int sampleRate, int channelCount)
      : sampleFmt(sampleFmt), sampleRate(sampleRate) {
    av_channel_layout_default(&channelLayout, channelCount);
  }

  int getChannelCount() const { return channelLayout.nb_channels; }

  std::string dump() const {
    const char* sampleFmtName = av_get_sample_fmt_name(sampleFmt);
    return "AudioBaseInfo{sampleFmt=" +
           (sampleFmtName ? std::string(sampleFmtName) : "unknown") +
           ", sampleRate=" + std::to_string(sampleRate) +
           ", channelLayout=" + std::to_string(getChannelCount()) + "}";
  }
};

}  // namespace FFmpegWrapper
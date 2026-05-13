#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

#include "Common.h"

extern "C" {
#include "libavutil/avutil.h"
#include "libavutil/channel_layout.h"
#include "libavutil/imgutils.h"
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

inline int getPerImageSize(AVPixelFormat pixFmt, int width, int height) {
  return av_image_get_buffer_size(pixFmt, width, height, 1);
}

inline int getPerAudioFrameSize(int* linesize, AVSampleFormat sampleFmt,
                                int nbSamples, int channelCount) {
  // NOTE: av_samples_get_buffer_size() treats linesize as an OUT parameter.
  return av_samples_get_buffer_size(linesize, channelCount, nbSamples,
                                    sampleFmt, 1);
}

inline bool isAudioPlanar(AVSampleFormat sampleFmt) {
  return av_sample_fmt_is_planar(sampleFmt) != 0;
}

inline int getBytesPerSample(AVSampleFormat sampleFmt) {
  return av_get_bytes_per_sample(sampleFmt);
}

inline int imageCopyToBuffer(std::vector<uint8_t>& dst,
                             const uint8_t* const* srcData,
                             const int* srcLinesize, enum AVPixelFormat pixFmt,
                             int width, int height) {
  int requiredSize = getPerImageSize(pixFmt, width, height);
  if (requiredSize < 0) return -1;
  dst.resize(requiredSize);
  return av_image_copy_to_buffer(dst.data(), static_cast<int>(dst.size()),
                                 srcData, srcLinesize, pixFmt, width, height,
                                 1);
}

inline AVSampleFormat getPackedFmt(AVSampleFormat sampleFmt) {
  return av_get_packed_sample_fmt(sampleFmt);
}

inline const char* getSampleFmtName(AVSampleFormat sampleFmt) {
  return av_get_sample_fmt_name(sampleFmt);
}

inline int audioCopyToBuffer(std::vector<uint8_t>& dst,
                             const uint8_t* const* srcData,
                             const int* srcLinesize,
                             enum AVSampleFormat sampleFmt, int nbSamples,
                             int channelCount) {
  if (!srcData) return -1;
  if (nbSamples <= 0 || channelCount <= 0) return -1;

  // This helper always outputs PACKED(interleaved) PCM.
  const AVSampleFormat packedFmt = av_get_packed_sample_fmt(sampleFmt);
  if (packedFmt == AV_SAMPLE_FMT_NONE) return -1;

  int requiredSize =
      getPerAudioFrameSize(nullptr, packedFmt, nbSamples, channelCount);
  if (requiredSize < 0) return -1;
  dst.resize(static_cast<size_t>(requiredSize));

  if (isAudioPlanar(sampleFmt)) {
    // planar ch 2 :[0] left left left [1] right right right
    // convert to interleaved: left right left right left right
    int perSampleSize = getBytesPerSample(sampleFmt);
    uint8_t* dstPtr = dst.data();
    for (int n = 0; n < nbSamples; ++n) {
      for (int ch = 0; ch < channelCount; ++ch) {
        if (!srcData[ch]) return -1;
        const uint8_t* src = srcData[ch] + n * perSampleSize;
        memcpy(dstPtr, src, static_cast<size_t>(perSampleSize));
        dstPtr += perSampleSize;
      }
    }
  } else {
    if (!srcData[0]) return -1;
    // Packed input: copy as much as is safely available.
    size_t copyBytes = dst.size();
    if (srcLinesize) {
      if (srcLinesize[0] <= 0) return -1;
      copyBytes = std::min(copyBytes, static_cast<size_t>(srcLinesize[0]));
    }
    memcpy(dst.data(), srcData[0], copyBytes);
    // for alignment reasons
    if (copyBytes < dst.size()) {
      memset(dst.data() + copyBytes, 0, dst.size() - copyBytes);
    }
  }
  return static_cast<int>(dst.size());
}

}  // namespace CodecUtils

struct FFMPEG_WRAPPER_EXPORT VideoBaseInfo {
  AVPixelFormat pixFmt;
  int width;
  int height;
  AVRational frameRate;
  AVColorRange colorRange;
  AVColorSpace colorSpace;

  int getBytesPerPixel() const {
    return av_image_get_buffer_size(pixFmt, width, height, 1) /
           (width * height);
  }

  const char* getPixFmtName() const { return av_get_pix_fmt_name(pixFmt); }

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

  int getBytesPerSample() const {
    return av_get_bytes_per_sample(sampleFmt) * getChannelCount();
  }

  const char* getSampleFmtName() const {
    return av_get_sample_fmt_name(sampleFmt);
  }

  const char* getPackedSampleFmtName() const {
    AVSampleFormat packedFmt = av_get_packed_sample_fmt(sampleFmt);
    return av_get_sample_fmt_name(packedFmt);
  }

  // Returns ffplay/ffmpeg raw PCM demuxer format name for the *packed* sample
  // format (e.g. "s16le", "f32le").
  // Note: raw PCM is byte-order sensitive; for typical Windows builds this is
  // little-endian.
  const char* getFFplayPackedPcmFormatName() const {
    switch (av_get_packed_sample_fmt(sampleFmt)) {
      case AV_SAMPLE_FMT_U8:
        return "u8";
      case AV_SAMPLE_FMT_S16:
        return "s16le";
      case AV_SAMPLE_FMT_S32:
        return "s32le";
      case AV_SAMPLE_FMT_FLT:
        return "f32le";
      case AV_SAMPLE_FMT_DBL:
        return "f64le";
      default:
        return nullptr;
    }
  }

  bool isPlanar() const { return av_sample_fmt_is_planar(sampleFmt); }

  std::string dump() const {
    const char* sampleFmtName = av_get_sample_fmt_name(sampleFmt);
    return "AudioBaseInfo{sampleFmt=" +
           (sampleFmtName ? std::string(sampleFmtName) : "unknown") +
           ", sampleRate=" + std::to_string(sampleRate) +
           ", channelLayout=" + std::to_string(getChannelCount()) + "}";
  }
};

}  // namespace FFmpegWrapper
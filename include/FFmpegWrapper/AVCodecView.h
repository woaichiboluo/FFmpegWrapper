#pragma once

#include <stdexcept>

#include "Common.h"
#include "Utils.h"

struct AVCodec;

#define GET_SUPPORTED_CONFIG_METHOD(RetType, FuncSuffix, ConfigValue)   \
  RetType getSupported##FuncSuffix() const {                            \
    RetType ret = nullptr;                                              \
    avcodec_get_supported_config(nullptr, m_ptr, ConfigValue, 0,        \
                                 reinterpret_cast<const void **>(&ret), \
                                 nullptr);                              \
    return ret;                                                         \
  }

#define CHECK_CODEC_SUPPORT_METHOD(FuncSuffix, ConfigValue)                \
  void check##FuncSuffix##ForCodec(const FuncSuffix##BaseInfo &info,       \
                                   AVCodecView codecView) {                \
    auto supportedList = codecView.getSupported##FuncSuffix();             \
    if (!isCodecSupported(info.ConfigValue, supportedList)) {              \
      throw std::runtime_error("Unsupported " #FuncSuffix " for codec: " + \
                               std::string(codecView.getName()));          \
    }                                                                      \
  }

namespace FFmpegWrapper {

class FFMPEG_WRAPPER_EXPORT AVCodecView
    : public detail::ViewBase<const AVCodec> {
 public:
  AVCodecView() = default;
  explicit AVCodecView(const AVCodec *ptr) : ViewBase(ptr) {}
  ~AVCodecView() = default;

  static AVCodecView findEncoder(AVCodecID id) {
    return AVCodecView(avcodec_find_encoder(id));
  }

  static AVCodecView findEncoderByName(const char *name) {
    return AVCodecView(avcodec_find_encoder_by_name(name));
  }

  static AVCodecView findDecoder(AVCodecID id) {
    return AVCodecView(avcodec_find_decoder(id));
  }

  static AVCodecView findDecoderByName(const char *name) {
    return AVCodecView(avcodec_find_decoder_by_name(name));
  }

  bool isEncoder() const { return av_codec_is_encoder(m_ptr) != 0; }
  bool isDecoder() const { return av_codec_is_decoder(m_ptr) != 0; }

  std::string_view getName() const { return m_ptr->name; }
  std::string_view getLongName() const { return m_ptr->long_name; }
  AVCodecID getId() const { return m_ptr->id; }

  // supported pixel formats
  GET_SUPPORTED_CONFIG_METHOD(const AVPixelFormat *, PixelFormat,
                              AV_CODEC_CONFIG_PIX_FORMAT);
  // supported frame rates
  GET_SUPPORTED_CONFIG_METHOD(const AVRational *, Framerate,
                              AV_CODEC_CONFIG_FRAME_RATE);
  // supported sample rates
  GET_SUPPORTED_CONFIG_METHOD(const int *, SampleRate,
                              AV_CODEC_CONFIG_SAMPLE_RATE);
  // supported sample formats
  GET_SUPPORTED_CONFIG_METHOD(const AVSampleFormat *, SampleFormat,
                              AV_CODEC_CONFIG_SAMPLE_FORMAT);
  // supported channel layouts
  GET_SUPPORTED_CONFIG_METHOD(const AVChannelLayout *, ChannelLayout,
                              AV_CODEC_CONFIG_CHANNEL_LAYOUT);
  // supported color ranges
  GET_SUPPORTED_CONFIG_METHOD(const AVColorRange *, ColorRange,
                              AV_CODEC_CONFIG_COLOR_RANGE);
  // supported color spaces
  GET_SUPPORTED_CONFIG_METHOD(const AVColorSpace *, ColorSpace,
                              AV_CODEC_CONFIG_COLOR_SPACE);
  // supported color primaries
  GET_SUPPORTED_CONFIG_METHOD(const AVAlphaMode *, AlphaMode,
                              AV_CODEC_CONFIG_ALPHA_MODE);

  void checkVideoBaseInfo(const VideoBaseInfo &info) const {
    const char *failedReason = nullptr;
    if (CodecUtils::isCodecSupported(info.pixFmt, getSupportedPixelFormat())) {
      failedReason = "Unsupported pixel format";
    } else if (!CodecUtils::isCodecSupported(info.colorRange,
                                             getSupportedColorRange())) {
      failedReason = "Unsupported color range";
    } else if (!CodecUtils::isCodecSupported(info.colorSpace,
                                             getSupportedColorSpace())) {
      failedReason = "Unsupported color space";
    } else if (!CodecUtils::isCodecSupported(info.frameRate,
                                             getSupportedFramerate())) {
      failedReason = "Unsupported frame rate";
    }
    if (!failedReason) return;
    throw std::runtime_error(std::string(failedReason) +
                             " for codec: " + std::string(getName()));
  }

  void checkAudioBaseInfo(const AudioBaseInfo &info) const {
    const char *failedReason = nullptr;
    if (!CodecUtils::isCodecSupported(info.sampleFmt,
                                      getSupportedSampleFormat())) {
      failedReason = "Unsupported sample format";
    } else if (!CodecUtils::isCodecSupported(info.sampleRate,
                                             getSupportedSampleRate())) {
      failedReason = "Unsupported sample rate";
    } else if (!CodecUtils::isCodecSupported(info.channelLayout,
                                             getSupportedChannelLayout())) {
      failedReason = "Unsupported channel layout";
    }
    if (!failedReason) return;
    throw std::runtime_error(std::string(failedReason) +
                             " for codec: " + std::string(getName()));
  }
};

}  // namespace FFmpegWrapper

#undef GET_SUPPORTED_CONFIG_METHOD
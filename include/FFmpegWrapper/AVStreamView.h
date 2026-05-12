#pragma once

#include "Common.h"

extern "C" {
#include <libavformat/avformat.h>
}

namespace FFmpegWrapper {

class AVStreamView : public detail::ViewBase<AVStream> {
 public:
  AVStreamView() = default;
  AVStreamView(AVStream* stream) : ViewBase(stream) {}
  ~AVStreamView() = default;

  void codecParametersCopy(const AVCodecParameters* src) {
    FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "Invalid AVStreamView", AVERROR(EINVAL));
    FFMPEG_WRAPPER_ERROR_CHECK(avcodec_parameters_copy(m_ptr->codecpar, src),
                               "Failed to copy codec parameters");
  }

  void codecParametersCopy(const AVStreamView& src) {
    FFMPEG_WRAPPER_TRUE_CHECK((!src || !m_ptr), "Invalid source AVStreamView",
                              AVERROR(EINVAL));
    codecParametersCopy(src->codecpar);
  }

  const char* getMediaTypeString() const {
    FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "Invalid AVStreamView", AVERROR(EINVAL));
    return av_get_media_type_string(m_ptr->codecpar->codec_type);
  }
};

}  // namespace FFmpegWrapper
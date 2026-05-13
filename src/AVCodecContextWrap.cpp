#include "FFmpegWrapper/AVCodecContextWrap.h"

#include "FFmpegWrapper/AVFrameWrap.h"
#include "FFmpegWrapper/AVPacketWrap.h"
#include "FFmpegWrapper/Common.h"

using namespace FFmpegWrapper;

void detail::AVCodecContextDeleter::operator()(AVCodecContext* ctx) const {
  if (!ctx) return;
  avcodec_free_context(&ctx);
}

AVCodecContextWrap::AVCodecContextWrap(AVCodecView codec)
    : WrapperBase(avcodec_alloc_context3(codec.get()),
                  detail::AVCodecContextDeleter()) {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "Failed to allocate AVCodecContext",
                            AVERROR(ENOMEM));
}

AVCodecContextWrap::AVCodecContextWrap(AVCodecContext* ctx)
    : WrapperBase(ctx, detail::AVCodecContextDeleter()) {}

AVCodecView AVCodecContextWrap::getCodec() const {
  return AVCodecView(m_ptr->codec);
}

void AVCodecContextWrap::open(AVDictionaryWrap* options) {
  FFMPEG_WRAPPER_TRUE_CHECK(isOpened(), "AVCodecContext is not initialized",
                            AVERROR(EINVAL));
  auto dictPtr = options ? options->get() : nullptr;
  FFMPEG_WRAPPER_ERROR_CHECK(
      avcodec_open2(m_ptr, nullptr, options ? &dictPtr : nullptr),
      "Failed to open avcodec context");
}

bool AVCodecContextWrap::isOpened() const { return avcodec_is_open(m_ptr); }

VideoBaseInfo AVCodecContextWrap::getVideoBaseInfo() const {
  VideoBaseInfo info;
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr || !isOpened(),
                            "AVCodecContext is not initialized",
                            AVERROR(EINVAL));
  info.pixFmt = m_ptr->pix_fmt;
  info.width = m_ptr->width;
  info.height = m_ptr->height;
  info.frameRate = m_ptr->framerate;
  info.colorRange = m_ptr->color_range;
  info.colorSpace = m_ptr->colorspace;
  return info;
}

AudioBaseInfo AVCodecContextWrap::getAudioBaseInfo() const {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr || !isOpened(),
                            "AVCodecContext is not initialized",
                            AVERROR(EINVAL));
  AudioBaseInfo info;
  av_channel_layout_copy(&info.channelLayout, &m_ptr->ch_layout);
  info.sampleFmt = m_ptr->sample_fmt;
  info.sampleRate = m_ptr->sample_rate;
  return info;
}

void AVCodecContextWrap::setVideoBaseInfo(const VideoBaseInfo& info) {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "AVCodecContext is not initialized",
                            AVERROR(EINVAL));
  m_ptr->pix_fmt = info.pixFmt;
  m_ptr->width = info.width;
  m_ptr->height = info.height;
  m_ptr->framerate = info.frameRate;
  m_ptr->color_range = info.colorRange;
  m_ptr->colorspace = info.colorSpace;
}

void AVCodecContextWrap::setAudioBaseInfo(const AudioBaseInfo& info) {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "AVCodecContext is not initialized",
                            AVERROR(EINVAL));
  m_ptr->sample_fmt = info.sampleFmt;
  m_ptr->sample_rate = info.sampleRate;
  av_channel_layout_copy(&m_ptr->ch_layout, &info.channelLayout);
}

void AVCodecContextWrap::parametersFromContext(AVCodecParameters* par) const {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "AVCodecContext is not initialized",
                            AVERROR(EINVAL));
  FFMPEG_WRAPPER_ERROR_CHECK(avcodec_parameters_from_context(par, m_ptr),
                             "Failed to copy parameters from context");
}

void AVCodecContextWrap::parametersToContext(const AVCodecParameters* par) {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "AVCodecContext is not initialized",
                            AVERROR(EINVAL));
  FFMPEG_WRAPPER_ERROR_CHECK(avcodec_parameters_to_context(m_ptr, par),
                             "Failed to copy parameters to context");
}

bool AVCodecContextWrap::sendPacket(const AVPacketWrap& packet) {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "AVCodecContext is not initialized",
                            AVERROR(EINVAL));
  int ret = avcodec_send_packet(m_ptr, packet.get());
  if (ret < 0) {
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return false;
    FFMPEG_WRAPPER_RET_ERROR_CHECK(ret, "Failed to send packet");
  }
  return true;
}

bool AVCodecContextWrap::receiveFrame(AVFrameWrap& frame) {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "AVCodecContext is not initialized",
                            AVERROR(EINVAL));
  int ret = avcodec_receive_frame(m_ptr, frame.get());
  if (ret < 0) {
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return false;
    FFMPEG_WRAPPER_RET_ERROR_CHECK(ret, "Failed to receive frame");
  }
  return true;
}

bool AVCodecContextWrap::sendFrame(const AVFrameWrap& packet) {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "AVCodecContext is not initialized",
                            AVERROR(EINVAL));
  int ret = avcodec_send_frame(m_ptr, packet.get());
  if (ret < 0) {
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return false;
    FFMPEG_WRAPPER_RET_ERROR_CHECK(ret, "Failed to send frame");
  }
  return true;
}

bool AVCodecContextWrap::receivePacket(AVPacketWrap& frame) {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "AVCodecContext is not initialized",
                            AVERROR(EINVAL));
  int ret = avcodec_receive_packet(m_ptr, frame.get());
  if (ret < 0) {
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return false;
    FFMPEG_WRAPPER_RET_ERROR_CHECK(ret, "Failed to receive packet");
  }
  return true;
}
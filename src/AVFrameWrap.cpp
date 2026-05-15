#include "FFmpegWrapper/AVFrameWrap.h"

#include "FFmpegWrapper/Common.h"

using namespace FFmpegWrapper;

void detail::AVFrameUndefHelper::operator()() const {
  if (frame) {
    av_frame_unref(frame);
  }
}

void detail::AVFrameDeleter::operator()(AVFrame*& frame) const {
  if (!frame) return;
  av_frame_free(&frame);
}

AVFrameWrap::AVFrameWrap()
    : WrapperBase(av_frame_alloc(), detail::AVFrameDeleter()) {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "Failed to allocate AVFrame",
                            AVERROR(ENOMEM));
}

AVFrameWrap::AVFrameWrap(AVFrame* ptr)
    : WrapperBase(ptr, detail::AVFrameDeleter()) {}

void AVFrameWrap::unref() {
  if (m_ptr) {
    av_frame_unref(m_ptr);
  }
}

[[nodiscard]] ScopeGuard<detail::AVFrameUndefHelper> AVFrameWrap::scopeUnref() {
  return ScopeGuard<detail::AVFrameUndefHelper>(
      detail::AVFrameUndefHelper(m_ptr));
}

void AVFrameWrap::setVideoBaseInfo(const VideoBaseInfo& info) {
  if (!m_ptr) return;
  m_ptr->format = info.pixFmt;
  m_ptr->width = info.width;
  m_ptr->height = info.height;
  m_ptr->color_range = info.colorRange;
}

void AVFrameWrap::setAudioBaseInfo(const AudioBaseInfo& info) {
  if (!m_ptr) return;
  m_ptr->format = info.sampleFmt;
  m_ptr->sample_rate = info.sampleRate;
  av_channel_layout_copy(&m_ptr->ch_layout, &info.channelLayout);
}

void AVFrameWrap::allocBuffer() {
  if (!m_ptr) return;
  FFMPEG_WRAPPER_ERROR_CHECK(av_frame_get_buffer(m_ptr, 0),
                             "Failed to allocate buffer for AVFrame");
}

void AVFrameWrap::dumpFrameToRawBytes(std::vector<uint8_t>& out) const {
  if (!m_ptr) return;
  bool isVideo = m_ptr->width > 0 && m_ptr->height > 0;
  bool isAudio = m_ptr->nb_samples > 0 && m_ptr->ch_layout.nb_channels > 0;
  if (isVideo) {
    int ret = CodecUtils::imageCopyToBuffer(
        out, m_ptr->data, m_ptr->linesize,
        static_cast<AVPixelFormat>(m_ptr->format), m_ptr->width, m_ptr->height);
    FFMPEG_WRAPPER_RET_ERROR_CHECK(ret,
                                   "Failed to dump video frame to raw bytes");
  } else if (isAudio) {
    const uint8_t* const* audioData =
        m_ptr->extended_data ? m_ptr->extended_data : m_ptr->data;
    int ret = CodecUtils::audioCopyToBuffer(
        out, audioData, m_ptr->linesize,
        static_cast<AVSampleFormat>(m_ptr->format), m_ptr->nb_samples,
        m_ptr->ch_layout.nb_channels);
    FFMPEG_WRAPPER_RET_ERROR_CHECK(ret,
                                   "Failed to dump audio frame to raw bytes");
  } else {
    FFMPEG_WRAPPER_RET_ERROR_CHECK(AVERROR(EINVAL), "Unsupported frame type");
  }
}
#include "FFmpegWrapper/AVFrameWrap.h"

#include "FFmpegWrapper/Common.h"
#include "libavutil/channel_layout.h"

using namespace FFmpegWrapper;

extern "C" {
#include <libavutil/frame.h>
}

void detail::AVFrameUndefHelper::operator()() const {
  if (frame) {
    av_frame_unref(frame);
  }
}

void detail::AVFrameDeleter::operator()(AVFrame* frame) const {
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

void AVFrameWrap::getBuffer() {
  if (!m_ptr) return;
  FFMPEG_WRAPPER_ERROR_CHECK(av_frame_get_buffer(m_ptr, 0),
                             "Failed to allocate buffer for AVFrame");
}
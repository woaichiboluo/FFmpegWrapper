#include "FFmpegWrapper/AVPacketWrap.h"

extern "C" {
#include "libavcodec/packet.h"
#include "libavutil/error.h"
}

using namespace FFmpegWrapper;

void detail::AVPacketUnrefHelper::operator()() const {
  av_packet_unref(packet);
}

void detail::AVPacketDeleter::operator()(AVPacket* pkt) const {
  if (pkt) av_packet_free(&pkt);
}

AVPacketWrap::AVPacketWrap()
    : WrapperBase(av_packet_alloc(), detail::AVPacketDeleter()) {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "Failed to allocate AVPacket",
                            AVERROR(ENOMEM));
}

AVPacketWrap::AVPacketWrap(AVPacket* ptr)
    : WrapperBase(ptr, detail::AVPacketDeleter()) {}

void AVPacketWrap::unref() { av_packet_unref(m_ptr); }

ScopeGuard<detail::AVPacketUnrefHelper> AVPacketWrap::scopeUnref() {
  return ScopeGuard<detail::AVPacketUnrefHelper>{
      detail::AVPacketUnrefHelper(m_ptr)};
}

void AVPacketWrap::rescaleTs(const AVRational& srcTb, const AVRational& dstTb) {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "Invalid AVPacketWrap", AVERROR(EINVAL));
  av_packet_rescale_ts(m_ptr, srcTb, dstTb);
}
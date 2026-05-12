#pragma once

#include "Common.h"

struct AVPacket;
struct AVRational;

namespace FFmpegWrapper {

namespace detail {

struct FFMPEG_WRAPPER_EXPORT AVPacketUnrefHelper {
  AVPacketUnrefHelper() = default;
  AVPacketUnrefHelper(AVPacket* pkt) : packet(pkt) {}

  void operator()() const;
  AVPacket* packet{nullptr};
};

struct FFMPEG_WRAPPER_EXPORT AVPacketDeleter {
  void operator()(AVPacket* pkt) const;
};

}  // namespace detail

class FFMPEG_WRAPPER_EXPORT AVPacketWrap
    : public detail::WrapperBase<AVPacket, detail::AVPacketDeleter> {
 public:
  AVPacketWrap();
  AVPacketWrap(AVPacket* ptr);
  ~AVPacketWrap() override = default;

  AVPacketWrap(AVPacketWrap&&) noexcept = default;
  AVPacketWrap& operator=(AVPacketWrap&&) noexcept = default;

  void rescaleTs(const AVRational& srcTb, const AVRational& dstTb);

  void unref();

  [[nodiscard]] ScopeGuard<detail::AVPacketUnrefHelper> scopeUnref();
};

}  // namespace FFmpegWrapper
#pragma once

#include "Common.h"
#include "Utils.h"

struct AVFrame;

namespace FFmpegWrapper {

namespace detail {

struct FFMPEG_WRAPPER_EXPORT AVFrameUndefHelper {
  AVFrameUndefHelper() = default;
  AVFrameUndefHelper(AVFrame* f) : frame(f) {}

  void operator()() const;

  AVFrame* frame{nullptr};
};

struct FFMPEG_WRAPPER_EXPORT AVFrameDeleter {
  void operator()(AVFrame* frame) const;
};

}  // namespace detail

class FFMPEG_WRAPPER_EXPORT AVFrameWrap
    : public detail::WrapperBase<AVFrame, detail::AVFrameDeleter> {
 public:
  AVFrameWrap();
  AVFrameWrap(AVFrame* ptr);
  ~AVFrameWrap() override = default;

  AVFrameWrap(AVFrameWrap&&) noexcept = default;
  AVFrameWrap& operator=(AVFrameWrap&&) noexcept = default;

  void setVideoBaseInfo(const VideoBaseInfo& info);
  void setAudioBaseInfo(const AudioBaseInfo& info);

  void getBuffer();

  void unref();

  [[nodiscard]] ScopeGuard<detail::AVFrameUndefHelper> scopeUnref();
};

}  // namespace FFmpegWrapper
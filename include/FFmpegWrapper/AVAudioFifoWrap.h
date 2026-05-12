#pragma once

#include "Common.h"
#include "Utils.h"

struct AVAudioFifo;

namespace FFmpegWrapper {
class AVFrameWrap;
}

namespace FFmpegWrapper {

namespace detail {

struct FFMPEG_WRAPPER_EXPORT AVAudioFifoDeleter {
  void operator()(AVAudioFifo* fifo) const;
};

}  // namespace detail

class FFMPEG_WRAPPER_EXPORT AVAudioFifoWrap
    : public detail::WrapperBase<AVAudioFifo, detail::AVAudioFifoDeleter> {
 public:
  AVAudioFifoWrap();

  AVAudioFifoWrap(const AudioBaseInfo& audioBaseInfo, int initialSize);

  explicit AVAudioFifoWrap(AVAudioFifo* fifo);

  ~AVAudioFifoWrap() override = default;

  AVAudioFifoWrap(AVAudioFifoWrap&&) noexcept = default;
  AVAudioFifoWrap& operator=(AVAudioFifoWrap&&) noexcept = default;

  int size() const;

  void reset();

  void ensureAdditionalCapacity(int additionalSamples);

  void ensureEnoughWrite(const void* const* data, int nbSamples);

  int write(const void* const* data, int nbSamples);

  int read(void** data, int nbSamples);

  int drain(int nbSamples);
};

}  // namespace FFmpegWrapper

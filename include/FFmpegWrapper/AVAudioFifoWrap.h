#pragma once

#include <algorithm>
#include <functional>
#include <memory>

#include "AVFrameWrap.h"
#include "Common.h"
#include "Utils.h"

struct AVAudioFifo;

namespace FFmpegWrapper {

namespace detail {

struct FFMPEG_WRAPPER_EXPORT AVAudioFifoDeleter {
  void operator()(AVAudioFifo*& fifo) const;
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

// ensure frame output at regular intervals
class FixedAudioSizeHelper {
 public:
  using Callback = std::function<void(AVFrameWrap&)>;

  FixedAudioSizeHelper() = default;
  ~FixedAudioSizeHelper() = default;

  FixedAudioSizeHelper(const FixedAudioSizeHelper&) = delete;
  FixedAudioSizeHelper& operator=(const FixedAudioSizeHelper&) = delete;
  FixedAudioSizeHelper(FixedAudioSizeHelper&&) noexcept = default;
  FixedAudioSizeHelper& operator=(FixedAudioSizeHelper&&) noexcept = default;

  void init(const AudioBaseInfo& audioBaseInfo, int frameSize) {
    m_frameSize = frameSize;
    m_fifo = std::make_unique<AVAudioFifoWrap>(audioBaseInfo, m_frameSize * 4);
    m_outFrame.setAudioBaseInfo(audioBaseInfo);
    m_outFrame->nb_samples = m_frameSize;
    m_outFrame.allocBuffer();
  }

  void push(const AVFrameWrap& frame) {
    push(reinterpret_cast<const void* const*>(frame->data), frame->nb_samples);
  }

  void push(const void* const* data, int nbSamples) {
    FFMPEG_WRAPPER_TRUE_CHECK(!m_fifo || m_frameSize == 0,
                              "FixedAudioSizeHelper not initialized",
                              AVERROR(EINVAL));
    if (!m_fifo) {
      return;
    }

    m_fifo->ensureEnoughWrite(data, nbSamples);
    while (m_fifo->size() >= m_frameSize) {
      m_outFrame->nb_samples = m_frameSize;
      m_fifo->read(reinterpret_cast<void**>(m_outFrame->data), m_frameSize);
      if (m_onFrameReady) {
        m_onFrameReady(m_outFrame);
      }
    }
  }

  void flush() {
    FFMPEG_WRAPPER_TRUE_CHECK(!m_fifo || m_frameSize == 0,
                              "FixedAudioSizeHelper not initialized",
                              AVERROR(EINVAL));
    if (!m_fifo) {
      return;
    }
    while (m_fifo->size() > 0) {
      int nbSamples = std::min(m_frameSize, m_fifo->size());
      m_outFrame->nb_samples = nbSamples;
      m_fifo->read(reinterpret_cast<void**>(m_outFrame->data), nbSamples);
      if (m_onFrameReady) {
        m_onFrameReady(m_outFrame);
      }
    }
  }

  void setOnFrameReady(Callback callback) {
    m_onFrameReady = std::move(callback);
  }

 private:
  std::unique_ptr<AVAudioFifoWrap> m_fifo;
  int m_frameSize{1024};
  AVFrameWrap m_outFrame;
  Callback m_onFrameReady;
};

}  // namespace FFmpegWrapper

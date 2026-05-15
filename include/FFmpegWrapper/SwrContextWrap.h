#pragma once

#include <functional>
#include <memory>
#include <utility>

#include "AVAudioFifoWrap.h"
#include "AVFrameWrap.h"
#include "Common.h"
#include "Utils.h"

struct SwrContext;

namespace FFmpegWrapper {

namespace detail {

struct FFMPEG_WRAPPER_EXPORT SwrContextDeleter {
  void operator()(SwrContext*& ctx) const;
};

}  // namespace detail

class FFMPEG_WRAPPER_EXPORT SwrContextWrap
    : public detail::WrapperBase<SwrContext, detail::SwrContextDeleter> {
 public:
  SwrContextWrap();
  SwrContextWrap(const AudioBaseInfo& src, const AudioBaseInfo& dst);
  ~SwrContextWrap() override = default;

  SwrContextWrap(SwrContextWrap&&) noexcept = default;
  SwrContextWrap& operator=(SwrContextWrap&&) noexcept = default;

  int convert(uint8_t* const* out, int outNbSamples, const uint8_t* const* in,
              int inNbSamples);

  int64_t getDelay(int64_t base) const;

  int getOutSamples(int inSamples) const;

  const AudioBaseInfo& getSrcAudioInfo() const { return m_srcAudioInfo; }
  const AudioBaseInfo& getDstAudioInfo() const { return m_dstAudioInfo; }

 private:
  AudioBaseInfo m_srcAudioInfo{};
  AudioBaseInfo m_dstAudioInfo{};
};

class AudioResampler {
 public:
  using Callback = std::function<void(AVFrameWrap&)>;

  AudioResampler() = default;
  ~AudioResampler() { freeOutputBuffer(); }

  AudioResampler(const AudioResampler&) = delete;
  AudioResampler& operator=(const AudioResampler&) = delete;

  AudioResampler(AudioResampler&& other) noexcept {
    moveFrom(std::move(other));
  }

  AudioResampler& operator=(AudioResampler&& other) noexcept {
    if (this != &other) {
      freeOutputBuffer();
      moveFrom(std::move(other));
    }
    return *this;
  }

  void initResampler(const AudioBaseInfo& src, const AudioBaseInfo& dst) {
    m_swrContext = std::make_unique<SwrContextWrap>(src, dst);
    m_fixedAudioSizeHelper.init(dst, m_frameSize);
    m_fixedAudioSizeHelper.setOnFrameReady(m_outputCb);
  }

  int getFrameSize() const { return m_frameSize; }
  void setFrameSize(int size) { m_frameSize = size; }

  void resample(const AVFrameWrap& inFrame) {
    FFMPEG_WRAPPER_TRUE_CHECK(!m_swrContext, "Resampler is not initialized",
                              AVERROR(EINVAL));

    if (!inFrame || inFrame->nb_samples <= 0) return;

    const auto& srcInfo = m_swrContext->getSrcAudioInfo();
    const auto& dstInfo = m_swrContext->getDstAudioInfo();
    const int64_t delay = m_swrContext->getDelay(srcInfo.sampleRate);
    const int outMax = static_cast<int>(
        av_rescale_rnd(delay + inFrame->nb_samples, dstInfo.sampleRate,
                       srcInfo.sampleRate, AV_ROUND_UP));
    FFMPEG_WRAPPER_TRUE_CHECK(outMax <= 0, "Invalid outMax", AVERROR(EINVAL));

    ensureOutputBuffer(outMax, dstInfo);

    const int outSamples = m_swrContext->convert(
        m_outData, outMax, const_cast<const uint8_t* const*>(inFrame->data),
        inFrame->nb_samples);
    if (outSamples <= 0) return;

    m_fixedAudioSizeHelper.push(reinterpret_cast<const void* const*>(m_outData),
                           outSamples);
  }

  void flush() {
    FFMPEG_WRAPPER_TRUE_CHECK(!m_swrContext, "Resampler is not initialized",
                              AVERROR(EINVAL));

    const auto& srcInfo = m_swrContext->getSrcAudioInfo();
    const auto& dstInfo = m_swrContext->getDstAudioInfo();

    for (;;) {
      const int64_t delay = m_swrContext->getDelay(srcInfo.sampleRate);
      if (delay <= 0) break;

      const int outMax = static_cast<int>(av_rescale_rnd(
          delay, dstInfo.sampleRate, srcInfo.sampleRate, AV_ROUND_UP));
      if (outMax <= 0) break;

      ensureOutputBuffer(outMax, dstInfo);

      const int outSamples =
          m_swrContext->convert(m_outData, outMax, nullptr, 0);
      if (outSamples <= 0) break;

      m_fixedAudioSizeHelper.push(reinterpret_cast<const void* const*>(m_outData),
                             outSamples);
    }

    m_fixedAudioSizeHelper.flush();
  }

  void setOutputCallback(Callback cb) {
    m_outputCb = std::move(cb);
    m_fixedAudioSizeHelper.setOnFrameReady(m_outputCb);
  }

 private:
  void moveFrom(AudioResampler&& other) noexcept {
    m_swrContext = std::move(other.m_swrContext);
    m_fixedAudioSizeHelper = std::move(other.m_fixedAudioSizeHelper);
    m_frameSize = other.m_frameSize;
    m_outData = other.m_outData;
    m_outLinesize = other.m_outLinesize;
    m_outCapacity = other.m_outCapacity;
    m_outputCb = std::move(other.m_outputCb);

    other.m_outData = nullptr;
    other.m_outLinesize = 0;
    other.m_outCapacity = 0;
  }

  void ensureOutputBuffer(int nbSamples, const AudioBaseInfo& info) {
    if (m_outData && nbSamples <= m_outCapacity) return;

    freeOutputBuffer();
    FFMPEG_WRAPPER_ERROR_CHECK(
        av_samples_alloc_array_and_samples(&m_outData, &m_outLinesize,
                                           info.getChannelCount(), nbSamples,
                                           info.sampleFmt, 0),
        "Failed to allocate resampler output buffer");
    m_outCapacity = nbSamples;
  }

  void freeOutputBuffer() noexcept {
    if (m_outData) {
      av_freep(&m_outData[0]);
      av_freep(&m_outData);
    }
    m_outLinesize = 0;
    m_outCapacity = 0;
  }

  std::unique_ptr<SwrContextWrap> m_swrContext;
  FixedAudioSizeHelper m_fixedAudioSizeHelper;
  int m_frameSize{1024};
  uint8_t** m_outData{nullptr};
  int m_outLinesize{0};
  int m_outCapacity{0};
  Callback m_outputCb;
};

}  // namespace FFmpegWrapper

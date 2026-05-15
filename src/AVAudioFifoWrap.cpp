#include "FFmpegWrapper/AVAudioFifoWrap.h"

#include "FFmpegWrapper/Common.h"

using namespace FFmpegWrapper;

void detail::AVAudioFifoDeleter::operator()(AVAudioFifo*& fifo) const {
  if (fifo) {
    av_audio_fifo_free(fifo);
  }
}

AVAudioFifoWrap::AVAudioFifoWrap()
    : WrapperBase(nullptr, detail::AVAudioFifoDeleter()) {}

AVAudioFifoWrap::AVAudioFifoWrap(const AudioBaseInfo& audioBaseInfo,
                                 int initialSize)
    : WrapperBase(
          av_audio_fifo_alloc(audioBaseInfo.sampleFmt,
                              audioBaseInfo.getChannelCount(), initialSize),
          detail::AVAudioFifoDeleter()) {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "Failed to allocate AVAudioFifo",
                            AVERROR(ENOMEM));
}

AVAudioFifoWrap::AVAudioFifoWrap(AVAudioFifo* fifo)
    : WrapperBase(fifo, detail::AVAudioFifoDeleter()) {}

int AVAudioFifoWrap::size() const {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "Invalid AVAudioFifoWrap", AVERROR(EINVAL));
  return av_audio_fifo_size(m_ptr);
}

void AVAudioFifoWrap::reset() {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "Invalid AVAudioFifoWrap", AVERROR(EINVAL));
  av_audio_fifo_reset(m_ptr);
}

void AVAudioFifoWrap::ensureAdditionalCapacity(int additionalSamples) {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "Invalid AVAudioFifoWrap", AVERROR(EINVAL));
  const int needed = size() + additionalSamples;
  FFMPEG_WRAPPER_ERROR_CHECK(av_audio_fifo_realloc(m_ptr, needed),
                             "Failed to grow AVAudioFifo");
}

void AVAudioFifoWrap::ensureEnoughWrite(const void* const* data,
                                        int nbSamples) {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "Invalid AVAudioFifoWrap", AVERROR(EINVAL));
  FFMPEG_WRAPPER_TRUE_CHECK(!data, "Invalid audio data", AVERROR(EINVAL));
  const int available = av_audio_fifo_space(m_ptr);
  if (available < nbSamples) {
    ensureAdditionalCapacity(nbSamples);
  }
  write(data, nbSamples);
}

int AVAudioFifoWrap::write(const void* const* data, int nbSamples) {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "Invalid AVAudioFifoWrap", AVERROR(EINVAL));
  FFMPEG_WRAPPER_TRUE_CHECK(!data, "Invalid audio data", AVERROR(EINVAL));
  // av_audio_fifo_write takes non-const void**.
  void* const* p = const_cast<void* const*>(data);
  int ret = av_audio_fifo_write(m_ptr, p, nbSamples);
  FFMPEG_WRAPPER_RET_ERROR_CHECK(ret, "Failed to write to AVAudioFifo");
  return ret;
}

int AVAudioFifoWrap::read(void** data, int nbSamples) {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "Invalid AVAudioFifoWrap", AVERROR(EINVAL));
  FFMPEG_WRAPPER_TRUE_CHECK(!data, "Invalid audio data", AVERROR(EINVAL));
  return av_audio_fifo_read(m_ptr, data, nbSamples);
}

int AVAudioFifoWrap::drain(int nbSamples) {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "Invalid AVAudioFifoWrap", AVERROR(EINVAL));
  return av_audio_fifo_drain(m_ptr, nbSamples);
}

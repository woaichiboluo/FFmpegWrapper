#include "FFmpegWrapper/SwrContextWrap.h"

#include "FFmpegWrapper/Common.h"

using namespace FFmpegWrapper;

void detail::SwrContextDeleter::operator()(SwrContext*& ctx) const {
  if (ctx) {
    swr_free(&ctx);
  }
}

SwrContextWrap::SwrContextWrap() : WrapperBase(detail::SwrContextDeleter()) {}

SwrContextWrap::SwrContextWrap(const AudioBaseInfo& src,
                               const AudioBaseInfo& dst)
    : WrapperBase(detail::SwrContextDeleter()),
      m_srcAudioInfo(src),
      m_dstAudioInfo(dst) {
  int ret = swr_alloc_set_opts2(
      &m_ptr, &m_dstAudioInfo.channelLayout, m_dstAudioInfo.sampleFmt,
      m_dstAudioInfo.sampleRate, &m_srcAudioInfo.channelLayout,
      m_srcAudioInfo.sampleFmt, m_srcAudioInfo.sampleRate, 0, nullptr);
  FFMPEG_WRAPPER_RET_ERROR_CHECK(ret, "Failed to allocate SwrContext");
  ret = swr_init(m_ptr);
  if (ret < 0) {
    swr_free(&m_ptr);
    FFMPEG_WRAPPER_RET_ERROR_CHECK(ret, "Failed to initialize SwrContext");
  }
}

int SwrContextWrap::convert(uint8_t* const* out, int outNbSamples,
                            const uint8_t* const* in, int inNbSamples) {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "SwrContext is not initialized",
                            AVERROR(EINVAL));
  int ret = swr_convert(m_ptr, out, outNbSamples, in, inNbSamples);
  FFMPEG_WRAPPER_RET_ERROR_CHECK(ret, "Failed to convert audio samples");
  return ret;
}

int64_t SwrContextWrap::getDelay(int64_t base) const {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "SwrContext is not initialized",
                            AVERROR(EINVAL));
  return swr_get_delay(m_ptr, base);
}

int SwrContextWrap::getOutSamples(int inSamples) const {
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "SwrContext is not initialized",
                            AVERROR(EINVAL));
  int ret = swr_get_out_samples(m_ptr, inSamples);
  FFMPEG_WRAPPER_RET_ERROR_CHECK(ret, "Failed to query out samples");
  return ret;
}

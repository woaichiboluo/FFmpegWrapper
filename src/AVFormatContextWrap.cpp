#include "AVFormatContextWrap.h"

#include "common.h"
#include "libavformat/avio.h"
#include "libavutil/avutil.h"

extern "C" {
#include <libavformat/avformat.h>
}

#define REQUIRE_NOT_EXPECTED_TYPE(type, expected)                    \
  do {                                                               \
    if ((type) == (expected)) {                                      \
      throw FFmpegException("Invalid format type", AVERROR(EINVAL)); \
    }                                                                \
  } while (0)

using namespace FFmpegWrapper;

AVFormatContextWrap::AVFormatContextWrap(bool alloc)
    : WrapperBase(nullptr, &avformat_free_context) {
  if (!alloc) return;
  m_ptr = avformat_alloc_context();
  FFMPEG_WRAPPER_TRUE_CHECK(!m_ptr, "Failed to allocate AVFormatContext",
                            AVERROR(ENOMEM));
}

AVFormatContextWrap::~AVFormatContextWrap() {
  if (!m_ptr) return;
  if (m_type == Output) {
    if (m_ptr->oformat && !(m_ptr->oformat->flags & AVFMT_NOFILE)) {
      avio_closep(&m_ptr->pb);
    }
  } else if (m_type == Input) {
    avformat_close_input(&m_ptr);
  }
}

AVFormatContextWrap AVFormatContextWrap::openOutput(const std::string& url) {
  AVFormatContextWrap wrap;
  FFMPEG_WRAPPER_ERROR_CHECK(
      avformat_alloc_output_context2(&wrap.m_ptr, nullptr, nullptr, url.data()),
      "Failed to allocate AVFormatContext");
  // 判断是否需要打开输出IO，如果oformat的flags中有AVFMT_NOFILE标志，说明不需要打开IO
  if (wrap->oformat && !(wrap->oformat->flags & AVFMT_NOFILE)) {
    FFMPEG_WRAPPER_ERROR_CHECK(
        avio_open2(&wrap->pb, url.data(), AVIO_FLAG_WRITE, nullptr, nullptr),
        "Failed to open output IO");
  }
  wrap.m_type = Output;
  return wrap;
}

AVFormatContextWrap AVFormatContextWrap::openInput(const std::string& url) {
  AVFormatContextWrap wrap(true);
  FFMPEG_WRAPPER_ERROR_CHECK(
      avformat_open_input(&wrap.m_ptr, url.data(), nullptr, nullptr),
      "Failed to open input");
  wrap.m_type = Input;
  return wrap;
}

void AVFormatContextWrap::dumpFormat(int index, const char* url) const {
  REQUIRE_NOT_EXPECTED_TYPE(m_type, NotSet);
  av_dump_format(m_ptr, index, url, m_type == Input ? 0 : 1);
}

int AVFormatContextWrap::getStreamSize() const {
  return m_ptr ? m_ptr->nb_streams : 0;
}

AVStreamView AVFormatContextWrap::getStream(int index) const {
  FFMPEG_WRAPPER_TRUE_CHECK(index >= getStreamSize(),
                            "Stream index out of range", AVERROR(EINVAL));
  return AVStreamView(m_ptr->streams[index]);
}

AVStreamView AVFormatContextWrap::newStream(const AVCodec* codec) {
  REQUIRE_NOT_EXPECTED_TYPE(m_type, NotSet);
  AVStream* stream = avformat_new_stream(m_ptr, codec);
  FFMPEG_WRAPPER_TRUE_CHECK(!stream, "Failed to create new stream",
                            AVERROR(ENOMEM));
  return AVStreamView(stream);
}

bool AVFormatContextWrap::readFrame(AVPacketWrap& packet) {
  REQUIRE_NOT_EXPECTED_TYPE(m_type, NotSet);
  int ret = av_read_frame(m_ptr, packet.get());
  if (ret == AVERROR_EOF) return false;
  FFMPEG_WRAPPER_RET_ERROR_CHECK(ret, "Failed to read frame");
  return true;
}

void AVFormatContextWrap::writeHeader(AVDictionaryWrap options) {
  REQUIRE_NOT_EXPECTED_TYPE(m_type, NotSet);
  auto ptr = options.get();
  FFMPEG_WRAPPER_ERROR_CHECK(avformat_write_header(m_ptr, ptr ? &ptr : nullptr),
                             "Failed to write header");
}

void AVFormatContextWrap::interleavedWriteFrame(AVPacketWrap& packet) {
  REQUIRE_NOT_EXPECTED_TYPE(m_type, NotSet);
  FFMPEG_WRAPPER_ERROR_CHECK(av_interleaved_write_frame(m_ptr, packet.get()),
                             "Failed to write frame");
}

void AVFormatContextWrap::writeTrailer() {
  REQUIRE_NOT_EXPECTED_TYPE(m_type, NotSet);
  FFMPEG_WRAPPER_ERROR_CHECK(av_write_trailer(m_ptr),
                             "Failed to write trailer");
}

void AVFormatContextWrap::seekFrame(int stream_index, int64_t timestamp,
                                    int flags) {
  REQUIRE_NOT_EXPECTED_TYPE(m_type, NotSet);
  FFMPEG_WRAPPER_ERROR_CHECK(
      av_seek_frame(m_ptr, stream_index, timestamp, flags),
      "Failed to seek frame");
}

void AVFormatContextWrap::seekFile(int stream_index, int64_t min_ts, int64_t ts,
                                   int64_t max_ts, int flags) {
  REQUIRE_NOT_EXPECTED_TYPE(m_type, NotSet);
  FFMPEG_WRAPPER_ERROR_CHECK(
      avformat_seek_file(m_ptr, stream_index, min_ts, ts, max_ts, flags),
      "Failed to seek file");
}

int AVFormatContextWrap::findBestStream(AVMediaType type) {
  REQUIRE_NOT_EXPECTED_TYPE(m_type, NotSet);
  int stream_index = av_find_best_stream(m_ptr, type, -1, -1, nullptr, 0);
  FFMPEG_WRAPPER_RET_ERROR_CHECK(stream_index, "Failed to find best stream");
  return stream_index;
}

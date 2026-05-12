#pragma once

#include "AVDictionaryWrap.h"
#include "AVPacketWrap.h"
#include "AVStreamView.h"
#include "Common.h"

struct AVFormatContext;

namespace FFmpegWrapper {

class FFMPEG_WRAPPER_EXPORT AVFormatContextWrap
    : public detail::WrapperBase<AVFormatContext, void (*)(AVFormatContext*)> {
 public:
  enum Type { Input, Output, NotSet };

  AVFormatContextWrap();
  explicit AVFormatContextWrap(AVFormatContext* ctx);
  ~AVFormatContextWrap() override;

  AVFormatContextWrap(AVFormatContextWrap&&) noexcept = default;
  AVFormatContextWrap& operator=(AVFormatContextWrap&&) noexcept = default;

  Type getType() const { return m_type; }

  static AVFormatContextWrap openOutput(const std::string& url);
  static AVFormatContextWrap openInput(const std::string& url);

  // avformat_find_stream_info()
  void findStreamInfo(AVDictionaryWrap* options = nullptr);

  // av_dump_format()
  void dumpFormat(int index, const char* url) const;

  // avformat_new_stream()中的nb_streams是当前流的数量，index是流的索引，必须小于nb_streams
  int getStreamSize() const;
  AVStreamView getStream(int index) const;

  // avformat_new_stream()
  AVStreamView newStream(const AVCodec* codec = nullptr);

  // av_read_frame()
  bool readFrame(AVPacketWrap& packet);

  // av_seek_frame
  void seekFrame(int streamIndex, int64_t timestamp, int flags);

  // avformat_seek_file()
  void seekFile(int streamIndex, int64_t minTs, int64_t ts, int64_t maxTs,
                int flags);

  void seekFile(int streamIndex, int64_t ts, int flags = 0) {
    seekFile(streamIndex, std::numeric_limits<int64_t>::min(), ts, ts, flags);
  }

  // avformat_write_header()
  void writeHeader(AVDictionaryWrap options = nullptr);

  // av_interleaved_write_frame()
  void interleavedWriteFrame(AVPacketWrap& packet);

  // av_write_trailer()
  void writeTrailer();

  // av_find_best_stream()
  int findBestStream(AVMediaType type);

 private:
  Type m_type{NotSet};
};

}  // namespace FFmpegWrapper

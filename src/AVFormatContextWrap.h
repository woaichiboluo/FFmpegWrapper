#pragma once

#include <vcruntime_new.h>

#include "AVDictionaryWrap.h"
#include "AVPacketWrap.h"
#include "AVStreamView.h"
#include "common.h"

struct AVFormatContext;

namespace FFmpegWrapper {

class FFMPEG_WRAPPER_EXPORT AVFormatContextWrap
    : public detail::WrapperBase<AVFormatContext, void (*)(AVFormatContext*)> {
 public:
  enum Type { Input, Output, NotSet };

  explicit AVFormatContextWrap(bool alloc = false);
  ~AVFormatContextWrap() override;

  AVFormatContextWrap(AVFormatContextWrap&&) noexcept = default;
  AVFormatContextWrap& operator=(AVFormatContextWrap&&) noexcept = default;

  Type getType() const { return m_type; }

  static AVFormatContextWrap openOutput(const std::string& url);
  static AVFormatContextWrap openInput(const std::string& url);

  void dumpFormat(int index, const char* url) const;

  size_t getStreamSize() const;
  AVStreamView getStream(size_t index) const;

  AVStreamView newStream(const AVCodec* codec = nullptr);

  bool readFrame(AVPacketWrap& packet);

  void writeHeader(AVDictionaryWrap options = nullptr);

  void interleavedWriteFrame(AVPacketWrap& packet);

  void writeTrailer();

 private:
  Type m_type{NotSet};
};

}  // namespace FFmpegWrapper
#pragma once

#include <functional>
#include <memory>

#include "AVCodecView.h"
#include "AVDictionaryWrap.h"
#include "AVFrameWrap.h"
#include "AVPacketWrap.h"
#include "Common.h"
#include "Utils.h"

struct AVCodecContext;

namespace FFmpegWrapper {

namespace detail {

struct FFMPEG_WRAPPER_EXPORT AVCodecContextDeleter {
  void operator()(AVCodecContext*& ctx) const;
};

}  // namespace detail

class FFMPEG_WRAPPER_EXPORT AVCodecContextWrap
    : public detail::WrapperBase<AVCodecContext,
                                 detail::AVCodecContextDeleter> {
 public:
  explicit AVCodecContextWrap(AVCodecView codec);
  explicit AVCodecContextWrap(AVCodecContext* ctx);
  ~AVCodecContextWrap() override = default;

  AVCodecContextWrap(AVCodecContextWrap&&) noexcept = default;
  AVCodecContextWrap& operator=(AVCodecContextWrap&&) noexcept = default;

  void open(AVDictionaryWrap* options = nullptr);

  void open(const AVCodecParameters* params,
            AVDictionaryWrap* options = nullptr) {
    parametersToContext(params);
    open(options);
  }

  VideoBaseInfo getVideoBaseInfo() const;
  AudioBaseInfo getAudioBaseInfo() const;
  void setVideoBaseInfo(const VideoBaseInfo& info);
  void setAudioBaseInfo(const AudioBaseInfo& info);

  bool isEncoder() const { return getCodec().isEncoder(); }
  bool isDecoder() const { return getCodec().isDecoder(); }
  bool isOpened() const;

  AVCodecView getCodec() const;

  // avcodec_parameters_from_context()
  void parametersFromContext(AVCodecParameters* par) const;
  // avcodec_parameters_to_context()
  void parametersToContext(const AVCodecParameters* par);

  // for decoder: send packet, receive frame
  bool sendPacket(const AVPacketWrap& packet);
  bool receiveFrame(AVFrameWrap& frame);

  // for encoder: send frame, receive packet
  bool sendFrame(const AVFrameWrap& frame);
  bool receivePacket(AVPacketWrap& packet);
};

namespace detail {

class CodecBase {
 public:
  CodecBase() = default;
  virtual ~CodecBase() = default;

  CodecBase(const CodecBase&) = delete;
  CodecBase& operator=(const CodecBase&) = delete;

  CodecBase(CodecBase&&) noexcept = default;
  CodecBase& operator=(CodecBase&&) noexcept = default;

  AVCodecContextWrap& getCodecContextMutable() {
    FFMPEG_WRAPPER_TRUE_CHECK(!m_ctx, "Codec context is not initialized",
                              AVERROR(EINVAL));
    return *m_ctx;
  }

  const AVCodecContextWrap& getCodecContext() const {
    FFMPEG_WRAPPER_TRUE_CHECK(!m_ctx, "Codec context is not initialized",
                              AVERROR(EINVAL));
    return *m_ctx;
  }

  struct CbData {
    friend class CodecBase;
    AVCodecContextWrap& ctx;
    AVPacketWrap& packet;
    AVFrameWrap& frame;

    CbData(AVCodecContextWrap& ctx, AVPacketWrap& packet, AVFrameWrap& frame)
        : ctx(ctx), packet(packet), frame(frame) {}

    bool isFlush() const { return m_flush; }

   private:
    bool m_flush{false};
  };

 protected:
  void initCodecContext(const AVCodecView& codec) {
    FFMPEG_WRAPPER_TRUE_CHECK(m_ctx, "Codec context is already initialized",
                              AVERROR(EINVAL));
    m_ctx = std::make_unique<AVCodecContextWrap>(codec);
  }

  void setCodecContextFlush(CbData& data, bool flush) { data.m_flush = flush; }

  void setCallback(std::function<void(CbData&)> callback) {
    m_callback = std::move(callback);
  }

  void doCallback(CbData& data) {
    if (m_callback) m_callback(data);
  }

  AVFrameWrap m_frame;
  AVPacketWrap m_packet;

 private:
  std::unique_ptr<AVCodecContextWrap> m_ctx;
  std::function<void(CbData&)> m_callback;
};

}  // namespace detail

class CodecDecoder : public detail::CodecBase {
 public:
  using DecodeData = detail::CodecBase::CbData;
  using Callback = std::function<void(DecodeData&)>;

  CodecDecoder() = default;

  CodecDecoder(CodecDecoder&&) noexcept = default;
  CodecDecoder& operator=(CodecDecoder&&) noexcept = default;

  void setDecodeCallback(std::function<void(CbData&)> callback) {
    setCallback(std::move(callback));
  }

  auto& initDecoderContext(const AVCodecView& codec) {
    FFMPEG_WRAPPER_TRUE_CHECK(
        !codec.isDecoder(), "Provided codec is not a decoder", AVERROR(EINVAL));
    initCodecContext(codec);
    return getCodecContextMutable();
  }

  // when in decode,should not modify packet
  void decode(const AVPacketWrap& packet) { decodeInternal(packet, false); }

  void flush() { decodeInternal(nullptr, true); }

 private:
  void decodeInternal(const AVPacketWrap& packet, bool isFlush) {
    auto& ctx = getCodecContextMutable();
    DecodeData cbData(ctx, const_cast<AVPacketWrap&>(packet), m_frame);
    ctx.sendPacket(packet);
    while (ctx.receiveFrame(m_frame)) {
      auto guard = m_frame.scopeUnref();
      doCallback(cbData);
    }
    setCodecContextFlush(cbData, isFlush);
    if (isFlush) {
      doCallback(cbData);
    }
  }
};

class CodecEncoder : public detail::CodecBase {
 public:
  using EncodeData = detail::CodecBase::CbData;
  using Callback = std::function<void(EncodeData&)>;

  CodecEncoder() = default;

  CodecEncoder(CodecEncoder&&) noexcept = default;
  CodecEncoder& operator=(CodecEncoder&&) noexcept = default;

  void setEncodeCallback(Callback callback) {
    setCallback(std::move(callback));
  }

  auto& initEncoder(const AVCodecView& codec) {
    FFMPEG_WRAPPER_TRUE_CHECK(!codec.isEncoder(),
                              "Provided codec is not an encoder",
                              AVERROR(EINVAL));
    initCodecContext(codec);
    return getCodecContextMutable();
  }

  void encode(const AVFrameWrap& frame) { encodeInternal(frame); }
  void flush() { encodeInternal(nullptr); }

 private:
  void encodeInternal(const AVFrameWrap& frame) {
    auto& ctx = getCodecContextMutable();
    AVPacketWrap packet;
    EncodeData cbData(ctx, packet, const_cast<AVFrameWrap&>(frame));
    ctx.sendFrame(frame);
    while (ctx.receivePacket(packet)) {
      auto guard = packet.scopeUnref();
      doCallback(cbData);
    }
  }
};

}  // namespace FFmpegWrapper

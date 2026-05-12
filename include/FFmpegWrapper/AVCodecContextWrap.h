#pragma once

#include <functional>

#include "AVCodecView.h"
#include "AVDictionaryWrap.h"
#include "AVFrameWrap.h"
#include "AVPacketWrap.h"
#include "Common.h"
#include "FFmpegWrapper/AVPacketWrap.h"
#include "FFmpegWrapper/Common.h"
#include "Utils.h"

struct AVCodecContext;

namespace FFmpegWrapper {

namespace detail {

struct FFMPEG_WRAPPER_EXPORT AVCodecContextDeleter {
  void operator()(AVCodecContext* ctx) const;
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

  void flush();
};

class DecodeHelper {
 public:
  struct DecodeContext {
    friend class DecodeHelper;
    AVCodecContextWrap& ctx;
    const AVPacketWrap& inPacket;
    AVFrameWrap& outFrame;

    DecodeContext(AVCodecContextWrap& ctx, const AVPacketWrap& packet,
                  AVFrameWrap& frame)
        : ctx(ctx), inPacket(packet), outFrame(frame) {}

    bool isFlush() const { return m_flush; }

   private:
    void setFlush(bool flush) { m_flush = flush; }
    bool m_flush{false};
  };

  DecodeHelper(AVCodecContextWrap& ctx) : m_ctx(ctx) {}

  void setDecodeCallback(std::function<void(DecodeContext&)> callback) {
    m_decodeCallback = std::move(callback);
  }

  void decode(const AVPacketWrap& packet) { decodeInternal(packet, false); }

  void flush() { decodeInternal(nullptr, true); }

 private:
  void decodeInternal(const AVPacketWrap& packet, bool isFlush) {
    AVFrameWrap frame;
    DecodeContext context(m_ctx, packet, frame);
    m_ctx.sendPacket(packet);
    while (m_ctx.receiveFrame(frame)) {
      auto guard = frame.scopeUnref();
      if (m_decodeCallback) m_decodeCallback(context);
    }
    context.setFlush(true);
    if (isFlush && m_decodeCallback) {
      m_decodeCallback(context);
    }
  }

  std::function<void(DecodeContext&)> m_decodeCallback;
  AVCodecContextWrap& m_ctx;
};

class EncoderHelper {
 public:
  struct EncodeContext {
    friend class EncoderHelper;
    AVCodecContextWrap& ctx;
    const AVFrameWrap& inFrame;
    AVPacketWrap& outPacket;

    EncodeContext(AVCodecContextWrap& ctx, const AVFrameWrap& frame,
                  AVPacketWrap& packet)
        : ctx(ctx), inFrame(frame), outPacket(packet) {}
  };

  EncoderHelper(AVCodecContextWrap& ctx) : m_ctx(ctx) {}

  void setEncodeCallback(std::function<void(EncodeContext&)> callback) {
    m_encodeCallback = std::move(callback);
  }

  void encode(const AVFrameWrap& frame) { encodeInternal(frame); }
  void flush() { encodeInternal(nullptr); }

 private:
  void encodeInternal(const AVFrameWrap& frame) {
    AVPacketWrap packet;
    EncodeContext context(m_ctx, frame, packet);
    m_ctx.sendFrame(frame);
    while (m_ctx.receivePacket(packet)) {
      auto guard = packet.scopeUnref();
      if (m_encodeCallback) m_encodeCallback(context);
    }
  }

  AVCodecContextWrap& m_ctx;
  std::function<void(EncodeContext&)> m_encodeCallback;
};

}  // namespace FFmpegWrapper
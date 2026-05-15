#include <cstdio>

#include "FFmpegWrapper/AVCodecContextWrap.h"
#include "FFmpegWrapper/AVFormatContextWrap.h"
#include "FFmpegWrapper/AVPacketWrap.h"

using namespace FFmpegWrapper;

class Decoder {
 public:
  Decoder(const char* inputFile, const char* outputYUV, const char* outputMP3)
      : m_url(inputFile), m_outputYUV(outputYUV), m_outputMP3(outputMP3) {
    m_decoderMap[0].first = -1;
    m_decoderMap[1].first = -1;
    openInput();
  }
  ~Decoder() {
    if (m_videoFile) fclose(m_videoFile);
    if (m_audioFile) fclose(m_audioFile);
  }

  Decoder(const Decoder&) = delete;
  Decoder& operator=(const Decoder&) = delete;

  void decode() {
    AVPacketWrap packet;
    while (m_formatContext.readFrame(packet)) {
      auto guard = packet.scopeUnref();
      int streamIndex = packet->stream_index;
      int decoderIndex = -1;
      if (streamIndex == m_decoderMap[0].first) {
        decoderIndex = 0;
      } else if (streamIndex == m_decoderMap[1].first) {
        decoderIndex = 1;
      }
      if (decoderIndex < 0) return;
      auto& decoder = m_decoderMap[decoderIndex].second;
      decoder.decode(packet);
    }
    for (int i = 0; i < 2; ++i) {
      if (m_decoderMap[i].first >= 0) {
        m_decoderMap[i].second.flush();
      }
    }
  }

 private:
  void openInput() {
    m_formatContext = AVFormatContextWrap::openInput(m_url);
    m_formatContext.findStreamInfo();
    m_formatContext.dumpFormat(0, m_url.data());
    for (int i = 0; i < m_formatContext.getStreamSize(); ++i) {
      if (m_formatContext->streams[i]->codecpar->codec_type ==
          AVMEDIA_TYPE_VIDEO) {
        m_decoderMap[0].first = i;
        printf("Found video stream at index %d\n", i);
      } else if (m_formatContext->streams[i]->codecpar->codec_type ==
                 AVMEDIA_TYPE_AUDIO) {
        m_decoderMap[1].first = i;
        printf("Found audio stream at index %d\n", i);
      }
    }
    initOutput(m_decoderMap[0], true);
    initOutput(m_decoderMap[1], false);
  }

  void initOutput(std::pair<int, CodecDecoder>& decoderPair, bool video) {
    const char* desc = video ? "video" : "audio";
    int index = decoderPair.first;
    if (index < 0) {
      fprintf(stderr, "No %s stream found\n", desc);
      return;
    }
    // open output file
    FILE** outputFile = video ? &m_videoFile : &m_audioFile;
    const char* fileName = video ? m_outputYUV.data() : m_outputMP3.data();
    *outputFile = fopen(fileName, "wb");
    if (!*outputFile) {
      fprintf(stderr, "Failed to open output file: %s\n", fileName);
      return;
    }
    fprintf(stdout, "Output %s stream to file:%s\n", desc, fileName);
    // find decoder
    auto inputStream = m_formatContext.getStream(index);
    auto codec = AVCodecView::findDecoder(inputStream->codecpar->codec_id);
    if (!codec) {
      fprintf(stderr, "Unsupported codec for %s stream \n", desc);
      return;
    }
    // open decoder context
    auto& decoder = decoderPair.second;
    auto& codecContext = decoder.initDecoderContext(codec);
    codecContext.open(inputStream->codecpar);
    printf("Stream base info:%s\n",
           video ? codecContext.getVideoBaseInfo().dump().data()
                 : codecContext.getAudioBaseInfo().dump().data());
    decoder.setDecodeCallback(
        [this, video](CodecDecoder::DecodeData& data) { decode(video, data); });
  }

  void decode(bool video, CodecDecoder::DecodeData& data) {
    if (data.isFlush()) {
      if (video) {
        auto videoInfo = data.ctx.getVideoBaseInfo();
        printf(
            "Using [ffplay -pixel_format %s -video_size %dx%d -framerate "
            "%d/%d %s] to play the raw video frames\n",
            videoInfo.getPixFmtName(), videoInfo.width, videoInfo.height,
            videoInfo.frameRate.num, videoInfo.frameRate.den,
            m_outputYUV.data());
      } else {
        auto audioInfo = data.ctx.getAudioBaseInfo();
        const char* ffplayFmt = audioInfo.getFFplayPackedPcmFormatName();
        printf(
            "Using [ffplay -f %s -ar %d -ch_layout %dc %s] to play the raw "
            "audio "
            "frames\n",
            ffplayFmt ? ffplayFmt : "<unknown>", audioInfo.sampleRate,
            audioInfo.channelLayout.nb_channels, m_outputMP3.data());
        if (!ffplayFmt) {
          printf(
              "Note: packed sample fmt is '%s'. Map it to ffplay raw PCM "
              "format "
              "(e.g. s16->s16le, flt->f32le).\n",
              audioInfo.getPackedSampleFmtName());
        }
      }
      return;
    } else {
      printf("%s decoder: Got decoded frame, pts=%" PRId64 "\n",
             video ? "video" : "audio", data.frame->pts);
      FILE* outputFile = video ? m_videoFile : m_audioFile;
      if (!outputFile) return;
      std::vector<uint8_t> buffer;
      auto& outFrame = data.frame;
      outFrame.dumpFrameToRawBytes(buffer);
      fwrite(buffer.data(), 1, buffer.size(), outputFile);
    }
  }

  std::string m_url;
  std::string m_outputYUV;
  std::string m_outputMP3;
  AVFormatContextWrap m_formatContext;
  FILE* m_videoFile = nullptr;
  FILE* m_audioFile = nullptr;
  CodecDecoder m_decoders[2];
  // [0] video decoder, [1] audio decoder
  std::pair<int, CodecDecoder> m_decoderMap[2];
};

int main(int argc, char** argv) {
  if (argc < 2) {
    printf("Usage: %s <input_file> [output.yuv] [output.pcm]\n", argv[0]);
    return -1;
  }
  const char* inputFile = argv[1];
  const char* outputYUV = argc > 2 ? argv[2] : "output.yuv";
  const char* outputMP3 = argc > 3 ? argv[3] : "output.pcm";

  try {
    Decoder decoder(inputFile, outputYUV, outputMP3);
    decoder.decode();
  } catch (const std::exception& e) {
    fprintf(stderr, "Error: %s\n", e.what());
    return -1;
  }

  return 0;
}
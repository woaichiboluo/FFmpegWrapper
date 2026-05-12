#include "FFmpegWrapper/AVAudioFifoWrap.h"
#include "FFmpegWrapper/AVCodecContextWrap.h"
#include "FFmpegWrapper/AVFormatContextWrap.h"
#include "FFmpegWrapper/AVFrameWrap.h"
#include "FFmpegWrapper/AVPacketWrap.h"
#include "FFmpegWrapper/Common.h"
#include "FFmpegWrapper/Utils.h"

using namespace FFmpegWrapper;

int main(int argc, char** argv) {
  const char* defaultInputAudio = "FurElise.ogg";
  if (argc > 1) {
    defaultInputAudio = argv[1];
  }
  try {
    auto inputContext = AVFormatContextWrap::openInput(defaultInputAudio);
    inputContext.findStreamInfo();
    auto outputContext = AVFormatContextWrap::openOutput("output.aac");

    auto audioStreamIndex = inputContext.findBestStream(AVMEDIA_TYPE_AUDIO);
    printf("Input Stream Count: %d, Audio Stream Index: %d\n",
           inputContext.getStreamSize(), audioStreamIndex);
    auto inputStream = inputContext.getStream(audioStreamIndex);

    // prepare decoder
    auto inputDecoderCodec =
        AVCodecView::findDecoder(inputStream->codecpar->codec_id);
    auto inputDecoderContext = AVCodecContextWrap(inputDecoderCodec);
    inputDecoderContext.open(inputStream->codecpar);
    auto inputAudioBaseInfo = inputDecoderContext.getAudioBaseInfo();
    printf("Input: %s\n", inputAudioBaseInfo.dump().data());

    // prepare encoder
    auto outputEncoderCodec = AVCodecView::findEncoder(AV_CODEC_ID_AAC);
    auto outputEncoderContext = AVCodecContextWrap(outputEncoderCodec);
    auto outputAudioBaseInfo = inputAudioBaseInfo;
    outputEncoderCodec.checkAudioBaseInfo(outputAudioBaseInfo);

    printf("Output: %s\n", outputAudioBaseInfo.dump().data());
    outputEncoderContext.setAudioBaseInfo(outputAudioBaseInfo);
    // Encoder timestamps are in samples when using 1/sampleRate time_base.
    outputEncoderContext->time_base = {1, outputAudioBaseInfo.sampleRate};
    outputEncoderContext.open();
    printf("Output Encoder frameSize:%d\n", outputEncoderContext->frame_size);

    // prepare output stream
    auto outputStream = outputContext.newStream();
    outputEncoderContext.parametersFromContext(outputStream->codecpar);
    outputStream->time_base = {1, outputAudioBaseInfo.sampleRate};
    outputStream->codecpar->codec_tag = 0;
    outputContext.writeHeader();

    // set encoder callbacks
    EncoderHelper encoder(outputEncoderContext);
    encoder.setEncodeCallback([&](EncoderHelper::EncodeContext& context) {
      context.outPacket->stream_index = outputStream->index;
      outputContext.interleavedWriteFrame(context.outPacket);
    });

    // prepare fifo
    AVAudioFifoWrap audioFifo(outputAudioBaseInfo,
                              outputEncoderContext->frame_size * 8);
    // prepare frame for fifo, use encoder frame size as fifo write size
    AVFrameWrap frameForFifo;
    frameForFifo.setAudioBaseInfo(outputAudioBaseInfo);
    frameForFifo->nb_samples = outputEncoderContext->frame_size;
    frameForFifo.getBuffer();
    int64_t writedSamples = 0;

    DecodeHelper decoder(inputDecoderContext);
    decoder.setDecodeCallback([&](DecodeHelper::DecodeContext& context) {
      if (!context.isFlush()) {
        printf("Decoded frame, nb_samples: %d\n", context.outFrame->nb_samples);
        void** inData = reinterpret_cast<void**>(context.outFrame->data);
        audioFifo.ensureEnoughWrite(inData, context.outFrame->nb_samples);
        while (audioFifo.size() >= outputEncoderContext->frame_size) {
          void** outData = reinterpret_cast<void**>(frameForFifo->data);
          audioFifo.read(outData, frameForFifo->nb_samples);
          frameForFifo->pts = writedSamples;
          writedSamples += frameForFifo->nb_samples;
          encoder.encode(frameForFifo);
        }
      } else {
        // flush decoder
        printf("Flushing decoder...\n");
        // drain audio fifo
        while (audioFifo.size() > 0) {
          void** outData = reinterpret_cast<void**>(frameForFifo->data);
          int nbSamples =
              std::min(audioFifo.size(), outputEncoderContext->frame_size);
          audioFifo.read(outData, nbSamples);
          frameForFifo->nb_samples = nbSamples;
          frameForFifo->pts = writedSamples;
          writedSamples += nbSamples;
          encoder.encode(frameForFifo);
        }
        encoder.flush();
      }
    });

    // read frames from input and send to decoder
    AVPacketWrap packet;
    while (inputContext.readFrame(packet)) {
      auto guard = packet.scopeUnref();
      if (packet->stream_index == audioStreamIndex) {
        decoder.decode(packet);
      }
    }

    decoder.flush();

    outputContext.writeTrailer();

    printf("Transcoding completed successfully.\n");

  } catch (const std::exception& e) {
    printf("Error: %s\n", e.what());
  }
}
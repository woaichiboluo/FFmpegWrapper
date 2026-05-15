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
    CodecDecoder inputDecoder;
    auto& inputDecoderContext =
        inputDecoder.initDecoderContext(inputDecoderCodec);
    inputDecoderContext.open(inputStream->codecpar);
    auto inputAudioBaseInfo = inputDecoderContext.getAudioBaseInfo();
    printf("Input: %s\n", inputAudioBaseInfo.dump().data());

    // // prepare encoder
    auto outputEncoderCodec = AVCodecView::findEncoder(AV_CODEC_ID_AAC);
    CodecEncoder outputEncoder;
    outputEncoder.initEncoder(outputEncoderCodec);
    auto outputAudioBaseInfo = inputAudioBaseInfo;
    outputEncoderCodec.checkAudioBaseInfo(outputAudioBaseInfo);
    auto& outputEncoderContext = outputEncoder.getCodecContextMutable();
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

    // // set encoder callbacks
    outputEncoder.setEncodeCallback([&](CodecEncoder::EncodeData& cbData) {
      cbData.packet->stream_index = outputStream->index;
      outputContext.interleavedWriteFrame(cbData.packet);
    });

    FixedAudioSizeHelper fixedAudioSizeHelper;
    fixedAudioSizeHelper.init(outputAudioBaseInfo,
                              outputEncoderContext->frame_size);
    int64_t writedSamples = 0;
    fixedAudioSizeHelper.setOnFrameReady([&](AVFrameWrap& frame) {
      frame->pts = writedSamples;
      writedSamples += frame->nb_samples;
      outputEncoder.encode(frame);
    });

    inputDecoder.setDecodeCallback([&](CodecDecoder::DecodeData& cbData) {
      if (!cbData.isFlush()) {
        auto& outFrame = cbData.frame;
        printf("Decoded frame, nb_samples: %d\n", outFrame->nb_samples);
        fixedAudioSizeHelper.push(outFrame);
      } else {
        // flush decoder
        printf("Flushing decoder...\n");
        fixedAudioSizeHelper.flush();
        outputEncoder.flush();
      }
    });

    // read frames from input and send to decoder
    AVPacketWrap packet;
    while (inputContext.readFrame(packet)) {
      auto guard = packet.scopeUnref();
      if (packet->stream_index == audioStreamIndex) {
        inputDecoder.decode(packet);
      }
    }

    inputDecoder.flush();

    outputContext.writeTrailer();

    printf("Transcoding completed successfully.\n");
  } catch (const std::exception& e) {
    printf("Error: %s\n", e.what());
  }
}
#include <exception>
#include <string>
#include <unordered_map>

#include "FFmpegWrapper/AVFormatContextWrap.h"
#include "FFmpegWrapper/AVPacketWrap.h"
#include "FFmpegWrapper/Utils.h"

using namespace FFmpegWrapper;

int main(int argc, char** argv) {
  if (argc != 5) {
    printf("Usage: %s <input> <output> <start_second> <duration_second>\n",
           argv[0]);
    return -1;
  }

  try {
    const char* inputUrl = argv[1];
    const char* outputUrl = argv[2];
    const double startSecond = std::stod(argv[3]);
    const double durationSecond = std::stod(argv[4]);

    if (startSecond < 0 || durationSecond <= 0) {
      printf("start_second >= 0 and duration_second > 0 are required\n");
      return -1;
    }

    printf("Clipping: %s -> %s  [%.2fs - %.2fs]\n", inputUrl, outputUrl,
           startSecond, startSecond + durationSecond);

    auto inputContext = AVFormatContextWrap::openInput(inputUrl);
    inputContext.findStreamInfo();
    auto outputContext = AVFormatContextWrap::openOutput(outputUrl);
    std::unordered_map<int, std::pair<int, bool>>
        streamIndexMap;  // input stream index -> {output stream index,started}

    for (int i = 0; i < inputContext.getStreamSize(); ++i) {
      auto inputStream = inputContext.getStream(i);
      if (inputStream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
          inputStream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
          inputStream->codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
        streamIndexMap[i] = {-1, false};  // mark as invalid stream
        continue;
      }
      auto outputStream = outputContext.newStream();
      outputStream.codecParametersCopy(inputStream);
      outputStream->codecpar->codec_tag = 0;
      // important !!!: set output time_base the same as input
      outputStream->time_base = inputStream->time_base;
      streamIndexMap[i] = {outputStream->index, false};
    }

    // av_time_base = 1000000 / 1, av_time_base_q = 1 / 1000000
    auto startTs = TimeBaseUtils::secondsToTs(startSecond, AV_TIME_BASE_Q);
    auto durationTs =
        TimeBaseUtils::secondsToTs(durationSecond, AV_TIME_BASE_Q);
    printf("Seeking to start position: %lld (%.2fs)\n", (long long)startTs,
           TimeBaseUtils::tsToSeconds(startTs, AV_TIME_BASE_Q));

    // streamId为-1的时候，timebase使用AV_TIME_BASE，ffmpeg会自动将seek_target转换为对应流的timebase
    inputContext.seekFile(-1, startTs, 0);

    outputContext.writeHeader();

    AVPacketWrap packet;
    while (inputContext.readFrame(packet)) {
      auto guard = packet.scopeUnref();
      auto inStreamIndex = packet->stream_index;
      auto inputStream = inputContext.getStream(inStreamIndex);
      auto outStreamIndex = streamIndexMap[inStreamIndex].first;
      bool& started = streamIndexMap[inStreamIndex].second;
      if (outStreamIndex == -1) continue;  // invalid stream, skip

      if (!started) {
        // not started, check keyframe and start
        if ((packet->flags & AV_PKT_FLAG_KEY) == 0)
          continue;      // not keyframe, skip
        started = true;  // start copying this stream
      }
      // 用 seek 起始位置做偏移，使输出时间戳从 0 开始
      auto startTsInStream = TimeBaseUtils::rescaleTs(startTs, AV_TIME_BASE_Q,
                                                      inputStream->time_base);

      auto durationTsInStream = TimeBaseUtils::rescaleTs(
          durationTs, AV_TIME_BASE_Q, inputStream->time_base);

      packet->pts -= startTsInStream;
      packet->dts -= startTsInStream;

      if (packet->dts != AV_NOPTS_VALUE && packet->dts >= durationTsInStream) {
        if (inputStream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) break;
      }

      packet->stream_index = streamIndexMap[inStreamIndex].first;
      packet.rescaleTs(inputContext.getStream(inStreamIndex)->time_base,
                       outputContext.getStream(outStreamIndex)->time_base);
      outputContext.interleavedWriteFrame(packet);
    }
    outputContext.writeTrailer();
    printf("Clipping finished successfully.\n");

    return 0;
  } catch (const std::exception& e) {
    printf("Error: %s\n", e.what());
    return -1;
  }
}
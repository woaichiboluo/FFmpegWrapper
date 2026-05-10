#include <exception>
#include <unordered_map>

#include "AVFormatContextWrap.h"
#include "AVPacketWrap.h"
#include "libavutil/avutil.h"

using namespace FFmpegWrapper;

int main(int argc, char** argv) {
  if (argc != 3) {
    printf("Usage: %s <input> <output>\n", argv[0]);
    return -1;
  }

  try {
    const char* inputUrl = argv[1];
    const char* outputUrl = argv[2];
    auto inputFormatContext = AVFormatContextWrap::openInput(inputUrl);
    auto outputFormatContext = AVFormatContextWrap::openOutput(outputUrl);
    inputFormatContext.dumpFormat(0, inputUrl);
    // inputStreamId -> outputStreamId
    std::unordered_map<int, int> streamIndexMap;
    int outputStreamId = 0;
    for (int i = 0; i < inputFormatContext.getStreamSize(); ++i) {
      auto stream = inputFormatContext.getStream(i);
      auto inStreamMediaType = stream->codecpar->codec_type;
      if (inStreamMediaType != AVMEDIA_TYPE_AUDIO &&
          inStreamMediaType != AVMEDIA_TYPE_VIDEO &&
          inStreamMediaType != AVMEDIA_TYPE_SUBTITLE) {
        streamIndexMap[i] = -1;
        continue;
      }
      streamIndexMap[i] = outputStreamId++;
      auto outputStream = outputFormatContext.newStream();
      outputStream.codecParametersCopy(stream);
      // some container format (like mp4) require codec_tag to be 0
      outputStream->codecpar->codec_tag = 0;
    }
    outputFormatContext.writeHeader();
    outputFormatContext.dumpFormat(0, outputUrl);

    AVPacketWrap packet;
    while (inputFormatContext.readFrame(packet)) {
      auto guard = packet.scopeUnref();
      auto inputStreamId = packet->stream_index;
      if (streamIndexMap[inputStreamId] < 0) {
        continue;
      }
      packet->stream_index = streamIndexMap[inputStreamId];
      packet.rescaleTs(
          inputFormatContext.getStream(inputStreamId)->time_base,
          outputFormatContext.getStream(packet->stream_index)->time_base);
      outputFormatContext.interleavedWriteFrame(packet);
    }
    outputFormatContext.writeTrailer();
    printf("Remuxing completed successfully. saving at %s\n", outputUrl);

  } catch (const std::exception& e) {
    printf("Error: %s\n", e.what());
  }
}
#include <cinttypes>
#include <cstdio>
#include <string_view>

#include "FFmpegWrapper/AVFormatContextWrap.h"
#include "FFmpegWrapper/AVPacketWrap.h"
#include "FFmpegWrapper/Utils.h"

using namespace FFmpegWrapper;

AVMediaType parseMediaTypeOrDefault(std::string_view arg) {
  if (arg == "0" || arg == "video") return AVMEDIA_TYPE_VIDEO;
  if (arg == "1" || arg == "audio") return AVMEDIA_TYPE_AUDIO;
  if (arg == "2" || arg == "subtitle") return AVMEDIA_TYPE_SUBTITLE;
  return AVMEDIA_TYPE_VIDEO;
}

AVMediaType parseMediaTypeOrDefault(const char* arg) {
  if (!arg || !*arg) return AVMEDIA_TYPE_VIDEO;
  return parseMediaTypeOrDefault(std::string_view(arg));
}

int main(int argc, char** argv) {
  if (argc < 2) {
    printf(
        "Usage: %s <input> [type]\n"
        "  type (optional): video|audio|subtitle or 0|1|2 (default: video)\n",
        argv[0]);
    return -1;
  }

  try {
    const char* inputUrl = argv[1];
    const char* typeArg = (argc >= 3) ? argv[2] : "video";
    const AVMediaType mediaType = parseMediaTypeOrDefault(typeArg);

    auto inputFormatContext = AVFormatContextWrap::openInput(inputUrl);
    inputFormatContext.findStreamInfo();
    const int streamIndex = inputFormatContext.findBestStream(mediaType);
    auto stream = inputFormatContext.getStream(streamIndex);
    auto timeBase = stream->time_base;

    printf("Selected %s stream index: %d, time_base: %d/%d\n",
           stream.getMediaTypeString(), streamIndex, timeBase.num,
           timeBase.den);

    AVPacketWrap packet;
    int64_t frameIndex = 0;

    while (inputFormatContext.readFrame(packet)) {
      if (packet->stream_index != streamIndex) continue;
      ++frameIndex;
      bool isKeyFrame = (packet->flags & AV_PKT_FLAG_KEY) != 0;
      double ptsSeconds = TimeBaseUtils::tsToSeconds(packet->pts, timeBase);
      double dtsSeconds = TimeBaseUtils::tsToSeconds(packet->dts, timeBase);
      double durationSeconds =
          TimeBaseUtils::tsToSeconds(packet->duration, timeBase);

      // Keep the original single-line style, but align fields with widths.
      // If timestamp is missing, print "NA" for the seconds column.
      printf("Frame:%6" PRId64 " | pts:%12" PRId64
             " (%9.3f s)"
             " | dts:%12" PRId64
             " (%9.3f s)"
             " | dur:%8" PRId64
             " (%8.3f s)"
             " | size:%7d | tb:%d/%-5d | key:%c\n",
             frameIndex, packet->pts,
             (packet->pts == AV_NOPTS_VALUE) ? -1.0 : ptsSeconds, packet->dts,
             (packet->dts == AV_NOPTS_VALUE) ? -1.0 : dtsSeconds,
             packet->duration,
             (packet->duration == AV_NOPTS_VALUE) ? -1.0 : durationSeconds,
             packet->size, timeBase.num, timeBase.den, isKeyFrame ? 'Y' : 'N');
    }

  } catch (const std::exception& e) {
    printf("Error: %s\n", e.what());
    return -1;
  }

  return 0;
}
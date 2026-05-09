#include <exception>

#include "AVFormatContextWrap.h"

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
    // int startSecond = std::stoi(argv[3]);
    // int durationSecond = std::stoi(argv[4]);
    auto inputFormatContext = AVFormatContextWrap::openInput(inputUrl);
    auto outputFormatContext = AVFormatContextWrap::openOutput(outputUrl);
    inputFormatContext.dumpFormat(0, inputUrl);
    for (size_t i = 0; i < inputFormatContext.getStreamSize(); ++i) {
      auto stream = inputFormatContext.getStream(i);
    }
  } catch (const std::exception& e) {
    printf("Error: %s\n", e.what());
  }
}
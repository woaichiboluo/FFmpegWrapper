#include "FFmpegWrapper/Common.h"

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/error.h>
}

using namespace FFmpegWrapper;

namespace {
struct FFmpegInitializer {
  FFmpegInitializer() { avdevice_register_all(); }
};

static FFmpegInitializer g_ffmpegInitializer;
}  // namespace

FFmpegException::FFmpegException(const std::string& msg, int errCode)
    : m_errCode(errCode) {
  // char buf[512],errbuf[256];
  char errbuf[256];
  av_strerror(errCode, errbuf, sizeof(errbuf));
  snprintf(m_msg, sizeof(m_msg), "[%s] code: %d errorMsg:%s", msg.data(),
           errCode, errbuf);
}

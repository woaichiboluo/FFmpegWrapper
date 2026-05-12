#pragma once

#include <exception>
#include <string>
#include <utility>

#if defined(_WIN32)
#define FFMPEG_WRAPPER_EXPORT __declspec(dllexport)
#else
#define FFMPEG_WRAPPER_EXPORT
#endif

namespace FFmpegWrapper {

template <typename T>
class ScopeGuard {
 public:
  explicit ScopeGuard(T func) : m_func(std::move(func)) {}
  ~ScopeGuard() {
    if (m_dismissed) return;
    m_func();
  }

  ScopeGuard(const ScopeGuard&) = delete;
  ScopeGuard& operator=(const ScopeGuard&) = delete;

  void dismiss() { m_dismissed = true; }

 private:
  bool m_dismissed = false;
  T m_func;
};

class FFMPEG_WRAPPER_EXPORT FFmpegException : public std::exception {
 public:
  explicit FFmpegException(const std::string& msg, int errCode);

  const char* what() const noexcept override { return m_msg; }

 private:
  char m_msg[512]{};
  int m_errCode;
};

#define FFMPEG_WRAPPER_ERROR_CHECK(expr, msg) \
  do {                                        \
    int ret = (expr);                         \
    if (ret < 0) {                            \
      throw FFmpegException((msg), ret);      \
    }                                         \
  } while (0)

#define FFMPEG_WRAPPER_RET_ERROR_CHECK(value, msg) \
  do {                                             \
    if ((value) < 0) {                             \
      throw FFmpegException((msg), (value));       \
    }                                              \
  } while (0)

#define FFMPEG_WRAPPER_TRUE_CHECK(value, msg, err) \
  do {                                             \
    if ((value)) {                                 \
      throw FFmpegException((msg), (err));         \
    }                                              \
  } while (0)

namespace detail {

template <typename T, typename Deleter>
class WrapperBase {
 public:
  using element_type = T;
  using deleter_type = Deleter;

  WrapperBase(Deleter deleter) : m_deleter(deleter) {}

  explicit WrapperBase(T* ptr, Deleter deleter)
      : m_ptr(ptr), m_deleter(deleter) {}

  virtual ~WrapperBase() {
    if (m_ptr) {
      m_deleter(m_ptr);
    }
  }

  WrapperBase(const WrapperBase&) = delete;
  WrapperBase& operator=(const WrapperBase&) = delete;

  WrapperBase(WrapperBase&& other) noexcept
      : m_ptr(other.m_ptr), m_deleter(other.m_deleter) {
    other.m_ptr = nullptr;
  }

  WrapperBase& operator=(WrapperBase&& other) noexcept {
    if (this != &other) {
      if (m_ptr) {
        m_deleter(m_ptr);
      }
      m_ptr = other.m_ptr;
      m_deleter = other.m_deleter;
      other.m_ptr = nullptr;
    }
    return *this;
  }

  T* get() noexcept { return m_ptr; }
  const T* get() const noexcept { return m_ptr; }

  T* operator->() noexcept { return m_ptr; }
  const T* operator->() const noexcept { return m_ptr; }

  explicit operator bool() const noexcept { return m_ptr != nullptr; }

 protected:
  T* m_ptr{nullptr};
  Deleter m_deleter{};
};

template <typename T>
class ViewBase {
 public:
  ViewBase() = default;
  explicit ViewBase(T* ptr) : m_ptr(ptr) {}

  T* get() noexcept { return m_ptr; }
  const T* get() const noexcept { return m_ptr; }

  T* operator->() noexcept { return m_ptr; }
  const T* operator->() const noexcept { return m_ptr; }

  explicit operator bool() const noexcept { return m_ptr != nullptr; }

 protected:
  T* m_ptr{nullptr};
};

}  // namespace detail
}  // namespace FFmpegWrapper
#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>

#include "Common.h"

extern "C" {
#include <libavutil/dict.h>
}

struct AVDictionary;
struct AVDictionaryEntry;

namespace FFmpegWrapper {

namespace detail {

class FFMPEG_WRAPPER_EXPORT AVDitionaryIterator {
  friend class AVDictionaryWrap;

 public:
  using iterator_category = std::input_iterator_tag;
  using value_type = const AVDictionaryEntry*;
  using difference_type = std::ptrdiff_t;
  using pointer = void;
  using reference = value_type;

  AVDitionaryIterator() = default;

  AVDitionaryIterator(const AVDictionary* dict,
                      const AVDictionaryEntry* entry) noexcept
      : m_dict(dict), m_entry(entry) {}

  reference operator*() const noexcept { return m_entry; }

  AVDitionaryIterator& operator++() noexcept {
    if (!m_dict || !m_entry) {
      m_entry = nullptr;
      return *this;
    }
    m_entry = av_dict_iterate(m_dict, m_entry);
    return *this;
  }

  AVDitionaryIterator operator++(int) noexcept {
    AVDitionaryIterator copy = *this;
    ++(*this);
    return copy;
  }

  bool operator==(const AVDitionaryIterator& other) const noexcept {
    return m_dict == other.m_dict && m_entry == other.m_entry;
  }

  bool operator!=(const AVDitionaryIterator& other) const noexcept {
    return !(*this == other);
  }

 private:
  const AVDictionary* m_dict{nullptr};
  const AVDictionaryEntry* m_entry{nullptr};
};

struct FFMPEG_WRAPPER_EXPORT AVDictionaryDeleter {
  void operator()(AVDictionary* dict) const noexcept {
    if (dict) av_dict_free(&dict);
  }
};

}  // namespace detail

class FFMPEG_WRAPPER_EXPORT AVDictionaryWrap
    : public detail::WrapperBase<AVDictionary, detail::AVDictionaryDeleter> {
 public:
  AVDictionaryWrap() : WrapperBase(deleter_type{}) {}
  AVDictionaryWrap(AVDictionary* ptr) : WrapperBase(ptr, deleter_type{}) {}
  ~AVDictionaryWrap() override = default;

  AVDictionaryWrap(AVDictionaryWrap&&) noexcept = default;
  AVDictionaryWrap& operator=(AVDictionaryWrap&&) noexcept = default;

  void set(const char* key, const char* value, int flags = 0) {
    FFMPEG_WRAPPER_ERROR_CHECK(av_dict_set(&m_ptr, key, value, flags),
                               "Failed to set dictionary entry");
  }

  void setInt(const char* key, int64_t value, int flags = 0) {
    FFMPEG_WRAPPER_ERROR_CHECK(av_dict_set_int(&m_ptr, key, value, flags),
                               "Failed to set dictionary int entry");
  }

  AVDictionaryWrap clone() const {
    AVDictionary* new_dict = nullptr;
    FFMPEG_WRAPPER_ERROR_CHECK(av_dict_copy(&new_dict, m_ptr, 0),
                               "Failed to clone dictionary");
    return AVDictionaryWrap(new_dict);
  }

  AVDictionaryEntry* getValue(const char* key,
                              const AVDictionaryEntry* prev = nullptr,
                              int flags = 0) const {
    return av_dict_get(m_ptr, key, prev, flags);
  }

  detail::AVDitionaryIterator begin() const noexcept;
  detail::AVDitionaryIterator end() const noexcept;
};

}  // namespace FFmpegWrapper
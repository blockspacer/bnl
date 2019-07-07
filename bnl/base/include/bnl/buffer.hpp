#pragma once

#include <bnl/base/export.hpp>

#include <bnl/class/macro.hpp>

#include <bnl/buffer_view.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

namespace bnl {

class BNL_BASE_EXPORT buffer {
public:
  class lookahead;

  buffer() noexcept;
  explicit buffer(size_t size);
  buffer(const uint8_t *data, size_t size);

  template <size_t Size>
  buffer(const char (&data)[Size]) noexcept // NOLINT
      : buffer(reinterpret_cast<const uint8_t *>(data), Size - 1)
  {}

  BNL_CUSTOM_COPY(buffer);
  BNL_CUSTOM_MOVE(buffer);

  ~buffer() noexcept;

  uint8_t *data() noexcept;
  const uint8_t *data() const noexcept;

  const uint8_t *begin() const noexcept;
  const uint8_t *end() const noexcept;

  uint8_t *begin() noexcept;
  uint8_t *end() noexcept;

  uint8_t operator[](size_t index) const noexcept;
  uint8_t operator*() const noexcept;

  uint8_t &operator[](size_t index) noexcept;
  uint8_t &operator*() noexcept;

  size_t size() const noexcept;
  bool empty() const noexcept;

  void consume(size_t size) noexcept;
  size_t consumed() const noexcept;

  // Returns a buffer to the next `size` bytes of this buffer and consumes
  // `size` bytes from this buffer. Both this buffer and the returned buffer
  // will manage different slices of the same memory block.
  //
  // Conceptually, this method transfers ownership of a slice of the memory
  // block to a new buffer. Because this method consumes `size` bytes, the
  // returned buffer will be the sole owner of the slice of memory.
  buffer slice(size_t size) noexcept;

  // Copies the next `size` bytes to a new buffer and returns it. In contrast to
  // `slice`, this method does not consume any bytes of this buffer.
  buffer copy(size_t size) noexcept;

  operator buffer_view() const noexcept; // NOLINT

  static buffer concat(const buffer &first, const buffer &second);

private:
  buffer(std::shared_ptr<uint8_t> data, size_t size) noexcept;

  void upgrade() noexcept;

  void destroy() noexcept;

private:
  enum class type { sso, unique, shared };

  type type_;
  size_t size_ = 0;
  size_t position_ = 0;

  static constexpr size_t SSO_THRESHOLD = 20;

  union {
    std::array<uint8_t, SSO_THRESHOLD> sso_;
    // Type erase `std::unique_ptr` deleter so any kind of deleter can be stored
    // in `buffer`.
    std::unique_ptr<uint8_t[]> unique_;
    // `std::shared_ptr<uint8_t[]>` requires C++17.
    std::shared_ptr<uint8_t> shared_;
  };
};

class BNL_BASE_EXPORT buffer::lookahead {
public:
  lookahead(const buffer &buffer) noexcept; // NOLINT

  size_t size() const noexcept;
  bool empty() const noexcept;

  uint8_t operator[](size_t index) const noexcept;
  uint8_t operator*() const noexcept;

  void consume(size_t size) noexcept;
  size_t consumed() const noexcept;

  buffer copy(size_t size) const;

private:
  const buffer &buffer_;
  size_t position_ = 0;
};

} // namespace bnl
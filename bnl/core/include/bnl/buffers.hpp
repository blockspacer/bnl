#pragma once

#include <bnl/buffer.hpp>

#include <bnl/core/export.hpp>

#include <bnl/class/macro.hpp>

#include <deque>

namespace bnl {

class BNL_CORE_EXPORT buffers {
public:
  buffers() = default;

  class anchor;
  class discarder;

  size_t size() const noexcept;
  bool empty() const noexcept;

  uint8_t operator[](size_t index) const noexcept;
  uint8_t operator*() const noexcept;

  buffer slice(size_t size) const;

  void push(buffer buffer);

  void consume(size_t size) noexcept;
  buffers &operator+=(size_t size) noexcept;

  size_t consumed() const noexcept;

  void undo(size_t size) noexcept;

  void discard();

private:
  std::deque<buffer> buffers_;

  buffer concat(size_t start, size_t end, size_t left) const;
};

class BNL_CORE_EXPORT buffers::anchor {
public:
  explicit anchor(buffers &buffers) noexcept;

  BNL_NO_COPY(anchor);
  BNL_NO_MOVE(anchor);

  ~anchor() noexcept;

  void relocate() noexcept;

  void release() noexcept;

private:
  buffers &buffers_;

  bool released_ = false;
  size_t position_ = 0;
};

class BNL_CORE_EXPORT buffers::discarder {
public:
  explicit discarder(buffers &buffers) noexcept;

  BNL_NO_COPY(discarder);
  BNL_NO_MOVE(discarder);

  ~discarder() noexcept;

private:
  buffers &buffers_;
};

} // namespace bnl
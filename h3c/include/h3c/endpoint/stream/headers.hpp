#pragma once

#include <h3c/event.hpp>
#include <h3c/frame.hpp>
#include <h3c/header.hpp>
#include <h3c/qpack.hpp>
#include <h3c/util/class.hpp>

#include <queue>

namespace h3c {

class logger;

namespace stream {
namespace headers {

class encoder {
public:
  H3C_EXPORT explicit encoder(logger *logger) noexcept;

  H3C_MOVE_ONLY(encoder);

  ~encoder() = default;

  H3C_EXPORT void add(header_view header, std::error_code &ec);
  H3C_EXPORT void fin(std::error_code &ec) noexcept;

  bool finished() const noexcept;

  H3C_EXPORT buffer encode(std::error_code &ec) noexcept;

private:
  logger *logger_;

  frame::encoder frame_;
  qpack::encoder qpack_;

  enum class state : uint8_t { idle, frame, qpack, fin, error };

  state state_ = state::idle;
  std::queue<buffer> buffers_;
};

class decoder {
public:
  H3C_EXPORT explicit decoder(logger *logger) noexcept;

  H3C_MOVE_ONLY(decoder);

  ~decoder() = default;

  bool started() const noexcept;

  bool finished() const noexcept;

  H3C_EXPORT header decode(buffers &encoded, std::error_code &ec) noexcept;

private:
  logger *logger_;

  frame::decoder frame_;
  qpack::decoder qpack_;

  enum class state : uint8_t { frame, qpack, fin, error };

  state state_ = state::frame;
  uint64_t headers_size_ = 0;
};

} // namespace headers
} // namespace stream
} // namespace h3c

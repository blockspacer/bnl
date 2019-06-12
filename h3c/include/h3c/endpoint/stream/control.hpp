#pragma once

#include <h3c/event.hpp>
#include <h3c/frame.hpp>
#include <h3c/quic.hpp>

#include <h3c/util/class.hpp>

namespace h3c {

class logger;

namespace stream {
namespace control {

class H3C_EXPORT sender {
public:
  sender(uint64_t id, const logger *logger) noexcept;

  H3C_MOVE_ONLY(sender);

  ~sender() = default;

  quic::data send(std::error_code &ec) noexcept;

private:
  uint64_t id_;
  const logger *logger_;

  frame::encoder frame_;

  enum class state : uint8_t { settings, idle, error };

  state state_ = state::settings;
  settings settings_;
};

class H3C_EXPORT receiver {
public:
  receiver(uint64_t id, const logger *logger) noexcept;

  H3C_MOVE_ONLY(receiver);

  virtual ~receiver() noexcept;

  void recv(quic::data data, event::handler handler, std::error_code &ec);

protected:
  virtual event process(frame frame, std::error_code &ec) = 0;

private:
  event process(std::error_code &ec) noexcept;

  uint64_t id_;
  const logger *logger_;

  frame::decoder frame_;

  enum class state : uint8_t { settings, active, error };

  state state_ = state::settings;

  buffers buffers_;
};

} // namespace control
} // namespace stream
} // namespace h3c

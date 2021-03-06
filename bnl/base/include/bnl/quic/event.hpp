#pragma once

#include <bnl/base/buffer.hpp>
#include <bnl/base/export.hpp>
#include <bnl/quic/result.hpp>

#include <cstdint>
#include <iosfwd>

namespace bnl {
namespace quic {

struct data {
  uint64_t id = UINT64_MAX;
  bool fin = false;
  base::buffer buffer;
};

class BNL_BASE_EXPORT event {
public:
  enum class type { data, error };

  struct payload {
    using data = quic::data;
    using error = quic::application::error;
  };

  event(payload::data data) noexcept;   // NOLINT
  event(payload::error error) noexcept; // NOLINT

  event(event &&other) noexcept;
  event &operator=(event &&) = delete;

  ~event() noexcept;

  uint64_t id() const noexcept;

  operator type() const noexcept; // NOLINT

private:
  const type type_;

public:
  union {
    payload::data data;
    payload::error error;
  };
};

BNL_BASE_EXPORT
std::ostream &
operator<<(std::ostream &os, const event &event);

}
}

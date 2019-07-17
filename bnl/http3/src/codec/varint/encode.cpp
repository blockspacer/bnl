#include <bnl/http3/codec/varint.hpp>

#include <bnl/base/error.hpp>
#include <bnl/http3/error.hpp>
#include <bnl/util/error.hpp>

namespace bnl {
namespace http3 {
namespace varint {

encoder::encoder(const log::api *logger) noexcept
  : logger_(logger)
{}

base::result<size_t>
encoder::encoded_size(uint64_t varint) const noexcept
{
  if (varint < 0x40U) {
    return sizeof(uint8_t);
  }

  if (varint < (0x40U << 8U)) {
    return sizeof(uint16_t);
  }

  if (varint < (0x40U << 24U)) {
    return sizeof(uint32_t);
  }

  if (varint < (0x40ULL << 56U)) {
    return sizeof(uint64_t);
  }

  THROW(error::varint_overflow);
}

// All encode functions convert from host to network byte order (big-endian)
// and insert the varint header.

static constexpr uint8_t UINT8_HEADER = 0x00;
static constexpr uint8_t UINT16_HEADER = 0x40;
static constexpr uint8_t UINT32_HEADER = 0x80;
static constexpr uint8_t UINT64_HEADER = 0xc0;

static void
uint8_encode(uint8_t *dest, uint8_t number)
{
  dest[0] = static_cast<uint8_t>(number >> 0U);

  dest[0] |= UINT8_HEADER;
}

static void
uint16_encode(uint8_t *dest, uint16_t number)
{
  dest[0] = static_cast<uint8_t>(number >> 8U);
  dest[1] = static_cast<uint8_t>(number >> 0U);

  dest[0] |= UINT16_HEADER;
}

static void
uint32_encode(uint8_t *dest, uint32_t number)
{
  dest[0] = static_cast<uint8_t>(number >> 24U);
  dest[1] = static_cast<uint8_t>(number >> 16U);
  dest[2] = static_cast<uint8_t>(number >> 8U);
  dest[3] = static_cast<uint8_t>(number >> 0U);

  dest[0] |= UINT32_HEADER;
}

static void
uint64_encode(uint8_t *dest, uint64_t number)
{
  dest[0] = static_cast<uint8_t>(number >> 56U);
  dest[1] = static_cast<uint8_t>(number >> 48U);
  dest[2] = static_cast<uint8_t>(number >> 40U);
  dest[3] = static_cast<uint8_t>(number >> 32U);
  dest[4] = static_cast<uint8_t>(number >> 24U);
  dest[5] = static_cast<uint8_t>(number >> 16U);
  dest[6] = static_cast<uint8_t>(number >> 8U);
  dest[7] = static_cast<uint8_t>(number >> 0U);

  dest[0] |= UINT64_HEADER;
}

base::result<size_t>
encoder::encode(uint8_t *dest, uint64_t varint) const noexcept
{
  CHECK(dest != nullptr, base::error::invalid_argument);

  size_t varint_size = TRY(this->encoded_size(varint));

  switch (varint_size) {
    case sizeof(uint8_t):
      uint8_encode(dest, static_cast<uint8_t>(varint));
      break;
    case sizeof(uint16_t):
      uint16_encode(dest, static_cast<uint16_t>(varint));
      break;
    case sizeof(uint32_t):
      uint32_encode(dest, static_cast<uint32_t>(varint));
      break;
    case sizeof(uint64_t):
      uint64_encode(dest, varint);
      break;
    default:
      NOTREACHED();
  }

  return varint_size;
}

base::result<base::buffer>
encoder::encode(uint64_t varint) const
{
  size_t encoded_size = TRY(this->encoded_size(varint));
  base::buffer encoded(encoded_size);

  size_t actual_encoded_size = TRY(encode(encoded.data(), varint));
  ASSERT(encoded_size == actual_encoded_size);

  return encoded;
}

}
}
}

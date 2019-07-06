#include <bnl/http3/codec/qpack/literal.hpp>

#include <bnl/util/error.hpp>

#include <algorithm>

namespace bnl {
namespace http3 {
namespace qpack {
namespace literal {

encoder::encoder(const log::api *logger) noexcept
    : logger_(logger), prefix_int_(logger), huffman_(logger)
{}

size_t encoder::encoded_size(buffer_view literal, uint8_t prefix) const noexcept
{
  size_t huffman_encoded_size = huffman_.encoded_size(literal);
  size_t literal_encoded_size = huffman_encoded_size < literal.size()
                                    ? huffman_encoded_size
                                    : literal.size();

  return prefix_int_.encoded_size(literal_encoded_size, prefix) +
         literal_encoded_size;
}

size_t encoder::encode(uint8_t *dest, buffer_view literal, uint8_t prefix) const
    noexcept
{
  uint8_t *begin = dest;

  size_t huffman_encoded_size = huffman_.encoded_size(literal);
  size_t literal_encoded_size = huffman_encoded_size < literal.size()
                                    ? huffman_encoded_size
                                    : literal.size();

  if (literal_encoded_size < literal.size()) {
    *dest = static_cast<uint8_t>(*dest | static_cast<uint8_t>(1U << prefix));
    dest += prefix_int_.encode(dest, literal_encoded_size, prefix);
    dest += huffman_.encode(dest, literal);
  } else {
    dest += prefix_int_.encode(dest, literal_encoded_size, prefix);
    std::copy_n(literal.data(), literal.size(), dest);
    dest += literal.size();
  }

  return static_cast<size_t>(dest - begin);
}

buffer encoder::encode(buffer_view literal, uint8_t prefix) const
{
  size_t encoded_size = this->encoded_size(literal, prefix);
  buffer encoded(encoded_size);

  ASSERT(encoded_size == encode(encoded.data(), literal, prefix));

  return encoded;
}

} // namespace literal
} // namespace qpack
} // namespace http3
} // namespace bnl
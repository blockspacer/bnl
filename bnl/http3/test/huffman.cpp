#include <doctest/doctest.h>

#include <bnl/http3/error.hpp>

#include <bnl/http3/codec/qpack/huffman.hpp>

#include <bnl/log.hpp>

#include <random>

using namespace bnl;

static string random_string(size_t length)
{
  static const char characters[] = "0123456789"
                                   "abcdefghijklmnopqrstuvwxyz"
                                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

  static std::mt19937 rg{ std::random_device{}() };
  static std::uniform_int_distribution<size_t> pick(0, sizeof(characters) - 1);

  bnl::string string;
  string.resize(length);

  for (char &character : string) {
    character = characters[pick(rg)];
  }

  return string;
}

static void encode_and_decode(string_view string,
                              const http3::qpack::huffman::encoder &encoder,
                              const http3::qpack::huffman::decoder &decoder)
{
  buffer encoded = encoder.encode(string);

  std::error_code ec;
  bnl::string decoded = decoder.decode(encoded, encoded.size(), ec);

  REQUIRE(!ec);
  REQUIRE(decoded.size() == string.size());
  REQUIRE(string == decoded);
}

TEST_CASE("huffman")
{
  log::api logger;

  http3::qpack::huffman::encoder encoder(&logger);
  http3::qpack::huffman::decoder decoder(&logger);

  SUBCASE("random")
  {
    for (size_t i = 0; i < 1000; i++) {
      bnl::string string = random_string(20);
      encode_and_decode(string, encoder, decoder);
    }
  }

  SUBCASE("incomplete")
  {
    bnl::string data("abcde");
    buffer encoded = encoder.encode(data);

    buffer incomplete = encoded.copy(2);

    std::error_code ec;
    decoder.decode(incomplete, encoded.size(), ec);

    REQUIRE(ec == http3::error::incomplete);
    REQUIRE(incomplete.size() == 2);

    decoder.decode(encoded, encoded.size(), ec);

    REQUIRE(!ec);
    REQUIRE(encoded.empty());
  }
}

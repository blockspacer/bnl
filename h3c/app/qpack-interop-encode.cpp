#include <algorithm>
#include <fstream>
#include <queue>
#include <vector>

#include <h3c/error.hpp>
#include <h3c/log/fprintf.hpp>
#include <h3c/qpack.hpp>

#include <util.hpp>

static h3c::log::impl::fprintf logger; // NOLINT

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress"

static h3c::buffer id_encode(uint64_t id)
{
  h3c::mutable_buffer encoded(sizeof(uint64_t));

  encoded[0] = static_cast<uint8_t>(id >> 56U);
  encoded[1] = static_cast<uint8_t>(id >> 48U);
  encoded[2] = static_cast<uint8_t>(id >> 40U);
  encoded[3] = static_cast<uint8_t>(id >> 32U);
  encoded[4] = static_cast<uint8_t>(id >> 24U);
  encoded[5] = static_cast<uint8_t>(id >> 16U);
  encoded[6] = static_cast<uint8_t>(id >> 8U);
  encoded[7] = static_cast<uint8_t>(id >> 0U);

  return std::move(encoded);
}

static h3c::buffer size_encode(uint32_t encoded_size)
{
  h3c::mutable_buffer encoded(sizeof(uint32_t));

  encoded[0] = static_cast<uint8_t>(encoded_size >> 24U);
  encoded[1] = static_cast<uint8_t>(encoded_size >> 16U);
  encoded[2] = static_cast<uint8_t>(encoded_size >> 8U);
  encoded[3] = static_cast<uint8_t>(encoded_size >> 0U);

  return std::move(encoded);
}

void write(std::ostream &dest, const h3c::buffer &encoded)
{
  dest.write(reinterpret_cast<const char *>(encoded.data()),
             static_cast<int32_t>(encoded.size()));
}

int main(int argc, char *argv[])
{
  if (argc < 3) {
    return 1;
  }

  std::ifstream input;
  input.exceptions(std::ifstream::failbit | std::ifstream::badbit);

  // Open input file

  try {
    input.open(argv[1], std::ios::binary);
  } catch (const std::ios_base::failure &e) {
    H3C_LOG_ERROR(&logger, "Error opening input file: {}", e.what());
    return 1;
  }

  // `std::getline` sets `failbit` when it doesn't read any characters.
  input.exceptions(std::ifstream::badbit);

  std::ofstream output;
  output.exceptions(std::ofstream::failbit | std::ofstream::badbit);

  // Open output file

  try {
    output.open(argv[2], std::ios::trunc | std::ios::binary);
  } catch (const std::ios_base::failure &e) {
    H3C_LOG_ERROR(&logger, "Error opening output file: {}", e.what());
    return 1;
  }

  // Read input

  std::string line;
  uint64_t id = 1;
  std::vector<h3c::header> headers;

  h3c::qpack::encoder qpack(&logger);
  std::error_code ec;

  while (std::getline(input, line)) {
    if (line.empty()) {
      if (headers.empty()) {
        continue;
      }

      // Encode header block and write to output

      try {
        std::queue<h3c::buffer> buffers;

        for (const h3c::header &header : headers) {
          h3c::buffer encoded = TRY(qpack.encode(header, ec));
          buffers.emplace(std::move(encoded));
        }

        if (qpack.count() > UINT32_MAX) {
          H3C_LOG_ERROR(&logger, "Headers encoded size does not fit in an "
                                 "unsigned 32-bit integer");
        }

        write(output, id_encode(id));
        write(output, size_encode(static_cast<uint32_t>(qpack.count())));

        while (!buffers.empty()) {
          h3c::buffer encoded = std::move(buffers.front());
          buffers.pop();
          write(output, encoded);
        }
      } catch (const std::ios_base::failure &e) {
        H3C_LOG_ERROR(&logger, "Error writing to output file: {}", e.what());
        return 1;
      }

      id++;
      headers.clear();

      continue;
    }

    if (line[0] == '#') {
      continue;
    }

    size_t i = line.find('\t');
    if (i == std::string::npos) {
      H3C_LOG_ERROR(&logger, "Missing TAB character");
      return 1;
    }

    h3c::mutable_buffer name(i);
    std::copy_n(line.substr(0, i).data(), name.size(), name.data());

    h3c::mutable_buffer value(line.size() - (i + 1));
    std::copy_n(line.substr(i + 1).data(), value.size(), value.data());

    headers.emplace_back(h3c::header{ std::move(name), std::move(value) });
  }

  return 0;
}

#pragma GCC diagnostic pop

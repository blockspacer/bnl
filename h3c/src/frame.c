#include <h3c/frame.h>
#include <h3c/varint.h>

#include <assert.h>
#include <string.h>

// Setting identifiers

#define SETTINGS_MAX_HEADER_LIST_SIZE 0x6U
#define SETTINGS_NUM_PLACEHOLDERS 0x9U

// We use a lot of macros in this file because after each serialize/parse, we
// have to check the result and update a lot of values. Macros provide us with a
// concise way to accomplish this. Using functions instead of macros doesn't
// work because we would still have to check the return value of the function
// each time we call it, where macros allow us to return directly from the
// parent function.

#define TRY_VARINT_PARSE_1(value)                                              \
  {                                                                            \
    size_t rv = h3c_varint_parse(src, size, &(value));                         \
    if (rv == 0) {                                                             \
      return error;                                                            \
    }                                                                          \
                                                                               \
    src += rv;                                                                 \
    size -= rv;                                                                \
    *bytes_read += rv;                                                         \
  }                                                                            \
  (void) 0

// Additionally checks and updates `frame_length` compared to
// `TRY_VARINT_PARSE_1`.
#define TRY_VARINT_PARSE_2(value)                                              \
  {                                                                            \
    size_t rv = h3c_varint_parse(src, size, &(value));                         \
    if (rv == 0) {                                                             \
      return error;                                                            \
    }                                                                          \
                                                                               \
    if (rv > frame_length) {                                                   \
      return error;                                                            \
    }                                                                          \
                                                                               \
    src += rv;                                                                 \
    size -= rv;                                                                \
    *bytes_read += rv;                                                         \
    frame_length -= rv;                                                        \
  }                                                                            \
  (void) 0

// Casting `frame_length` to `size_t` in `BUFFER_PARSE` is safe since we don't
// continue parsing in `h3c_frame_parse` if `frame_length > size`, so when
// `BUFFER_PARSE` is first used, we're guaranteed that `frame_length <= size`
// (both are always subtracted by the same amount) and `frame_length` fits
// inside `size_t`.

#define BUFFER_PARSE(buffer)                                                   \
  (buffer).data = src;                                                         \
  (buffer).size = (size_t) frame_length;                                       \
  src += frame_length;                                                         \
  size -= (size_t) frame_length;                                               \
  *bytes_read += (size_t) frame_length;                                        \
  frame_length -= frame_length;                                                \
  (void) 0

H3C_FRAME_PARSE_ERROR
h3c_frame_parse(const uint8_t *src,
                size_t size,
                h3c_frame_t *frame,
                size_t *bytes_read)
{
  *bytes_read = 0;

  // Before parsing the frame's length, incomplete varints only indicate the
  // full frame is not yet available.
  H3C_FRAME_PARSE_ERROR error = H3C_FRAME_PARSE_INCOMPLETE;

  TRY_VARINT_PARSE_1(frame->type);

  uint64_t frame_length = 0;
  TRY_VARINT_PARSE_1(frame_length);

  // We only continue parsing if the full frame payload is available in `src`.
  if (frame_length > size) {
    return error;
  }

  // From now on, if the frame's content size exceeds the frame's length or a
  // varint's size goes out of bounds of `src`, we're dealing with a malformed
  // frame.
  error = H3C_FRAME_PARSE_MALFORMED;

  switch (frame->type) {
  case H3C_DATA:
    BUFFER_PARSE(frame->data.payload);
    break;
  case H3C_HEADERS:
    BUFFER_PARSE(frame->headers.header_block);
    break;
  case H3C_PRIORITY:
    if (size == 0 || frame_length == 0) {
      return error;
    }

    frame->priority.prioritized_element_type = *src >> 6;
    frame->priority.element_dependency_type = (*src >> 4) & 0x03;
    src++;
    size--;
    frame_length--;

    TRY_VARINT_PARSE_2(frame->priority.prioritized_element_id);
    TRY_VARINT_PARSE_2(frame->priority.element_dependency_id);

    if (size == 0 || frame_length == 0) {
      return error;
    }

    frame->priority.weight = *src;
    src++;
    size--;
    frame_length--;
    break;
  case H3C_CANCEL_PUSH:
    TRY_VARINT_PARSE_2(frame->cancel_push.push_id);
    break;
  case H3C_SETTINGS:
    while (frame_length > 0) {
      uint64_t id = 0;
      TRY_VARINT_PARSE_2(id);
      uint64_t value = 0;
      TRY_VARINT_PARSE_2(value);

      switch (id) {
      case SETTINGS_MAX_HEADER_LIST_SIZE:
        frame->settings.max_header_list_size = value;
        break;
      case SETTINGS_NUM_PLACEHOLDERS:
        frame->settings.num_placeholders = value;
        break;
      }
    }
    break;
  case H3C_PUSH_PROMISE:
    TRY_VARINT_PARSE_2(frame->push_promise.push_id);
    BUFFER_PARSE(frame->push_promise.header_block);
    break;
  case H3C_GOAWAY:
    TRY_VARINT_PARSE_2(frame->goaway.stream_id);
    break;
  case H3C_MAX_PUSH_ID:
    TRY_VARINT_PARSE_2(frame->max_push_id.push_id);
    break;
  case H3C_DUPLICATE_PUSH:
    TRY_VARINT_PARSE_2(frame->duplicate_push.push_id);
    break;
  }

  if (frame_length > 0) {
    return error;
  }

  error = H3C_FRAME_PARSE_SUCCESS;

  return error;
}

#define TRY_VARINT_SIZE(value)                                                 \
  {                                                                            \
    size_t rv = h3c_varint_size((value));                                      \
    if (rv == 0) {                                                             \
      return 0;                                                                \
    }                                                                          \
                                                                               \
    size += rv;                                                                \
  }                                                                            \
  (void) 0

static uint64_t frame_payload_size(const h3c_frame_t *frame)
{
  uint64_t size = 0;

  switch (frame->type) {
  case H3C_DATA:
    size += frame->data.payload.size;
    break;
  case H3C_HEADERS:
    size += frame->headers.header_block.size;
    break;
  case H3C_PRIORITY:
    size++; // PT size + DT size + Empty size = 1 byte. See
            // https://quicwg.org/base-drafts/draft-ietf-quic-http.html#frame-priority
    TRY_VARINT_SIZE(frame->priority.prioritized_element_id);
    TRY_VARINT_SIZE(frame->priority.element_dependency_id);
    size++; // Weight
    break;
  case H3C_CANCEL_PUSH:
    TRY_VARINT_SIZE(frame->cancel_push.push_id);
    break;
  case H3C_SETTINGS:
    TRY_VARINT_SIZE(SETTINGS_MAX_HEADER_LIST_SIZE);
    TRY_VARINT_SIZE(frame->settings.max_header_list_size);
    TRY_VARINT_SIZE(SETTINGS_NUM_PLACEHOLDERS);
    TRY_VARINT_SIZE(frame->settings.num_placeholders);
    break;
  case H3C_PUSH_PROMISE:
    TRY_VARINT_SIZE(frame->push_promise.push_id);
    size += frame->push_promise.header_block.size;
    break;
  case H3C_GOAWAY:
    TRY_VARINT_SIZE(frame->goaway.stream_id);
    break;
  case H3C_MAX_PUSH_ID:
    TRY_VARINT_SIZE(frame->max_push_id.push_id);
    break;
  case H3C_DUPLICATE_PUSH:
    TRY_VARINT_SIZE(frame->duplicate_push.push_id);
    break;
  }

  return size;
}

#define TRY_VARINT_SERIALIZE(value)                                            \
  {                                                                            \
    size_t rv = h3c_varint_serialize(dest, size, (value));                     \
    if (rv == 0) {                                                             \
      return error;                                                            \
    }                                                                          \
                                                                               \
    dest += rv;                                                                \
    size -= rv;                                                                \
    *bytes_written += rv;                                                      \
  }                                                                            \
  (void) 0

#define TRY_BUFFER_SERIALIZE(buffer)                                           \
  if ((buffer).size > size) {                                                  \
    return error;                                                              \
  }                                                                            \
                                                                               \
  memcpy(dest, (buffer).data, (buffer).size);                                  \
  dest += (buffer).size;                                                       \
  size -= (buffer).size;                                                       \
  *bytes_written += (buffer).size;                                             \
  (void) 0

H3C_FRAME_SERIALIZE_ERROR h3c_frame_serialize(uint8_t *dest,
                                              size_t size,
                                              const h3c_frame_t *frame,
                                              size_t *bytes_written)
{
  *bytes_written = 0;

  H3C_FRAME_SERIALIZE_ERROR error = H3C_FRAME_SERIALIZE_BUF_TOO_SMALL;

  TRY_VARINT_SERIALIZE(frame->type);

  uint64_t frame_length = frame_payload_size(frame);
  if (frame_length == 0) {
    return H3C_FRAME_SERIALIZE_VARINT_OVERFLOW;
  }

  TRY_VARINT_SERIALIZE(frame_length);

  switch (frame->type) {
  case H3C_DATA:
    TRY_BUFFER_SERIALIZE(frame->data.payload);
    break;
  case H3C_HEADERS:
    TRY_BUFFER_SERIALIZE(frame->headers.header_block);
    break;
  case H3C_PRIORITY:
    if (size == 0) {
      return error;
    }

    *dest = (uint8_t)(*dest | frame->priority.prioritized_element_type << 6);
    *dest = (uint8_t)(*dest | frame->priority.element_dependency_type << 4);
    *dest &= 0xf0;
    dest++;
    size--;

    TRY_VARINT_SERIALIZE(frame->priority.prioritized_element_id);
    TRY_VARINT_SERIALIZE(frame->priority.element_dependency_id);

    if (size == 0) {
      return error;
    }

    *dest = frame->priority.weight;
    dest++;
    size--;
    break;
  case H3C_CANCEL_PUSH:
    TRY_VARINT_SERIALIZE(frame->cancel_push.push_id);
    break;
  case H3C_SETTINGS:
    TRY_VARINT_SERIALIZE(SETTINGS_MAX_HEADER_LIST_SIZE);
    TRY_VARINT_SERIALIZE(frame->settings.max_header_list_size);
    TRY_VARINT_SERIALIZE(SETTINGS_NUM_PLACEHOLDERS);
    TRY_VARINT_SERIALIZE(frame->settings.num_placeholders);
    break;
  case H3C_PUSH_PROMISE:
    TRY_VARINT_SERIALIZE(frame->push_promise.push_id);
    TRY_BUFFER_SERIALIZE(frame->push_promise.header_block);
    break;
  case H3C_GOAWAY:
    TRY_VARINT_SERIALIZE(frame->goaway.stream_id);
    break;
  case H3C_MAX_PUSH_ID:
    TRY_VARINT_SERIALIZE(frame->max_push_id.push_id);
    break;
  case H3C_DUPLICATE_PUSH:
    TRY_VARINT_SERIALIZE(frame->duplicate_push.push_id);
    break;
  }

  error = H3C_FRAME_SERIALIZE_SUCCESS;

  return error;
}

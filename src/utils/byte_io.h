#pragma once
#include <memory>

namespace qosrtp {
// Write the B least significant bytes of data of data type T into the buffer
template <typename T, uint32_t B = sizeof(T)>
class ByteWriter {
 public:
  static void WriteBigEndian(uint8_t* buffer, T data) {
    static_assert(B <= sizeof(T),
                  "The data type size is smaller than the set write size.");
    uint8_t* data_byte = reinterpret_cast<uint8_t*>(&data);
    for (int i = 0; i < B; i++) {
      *(buffer + (B - 1) - i) = *(data_byte + i);
    }
  }
  static void WriteLittleEndian(uint8_t* buffer, T data) {
    static_assert(B <= sizeof(T),
                  "The data type size is smaller than the set write size.");
    uint8_t* data_byte = reinterpret_cast<uint8_t*>(&data);
    for (int i = 0; i < B; i++) {
      *(buffer + i) = *(data_byte + i);
    }
  }
};
// Read data from a big-endian or little-endian buffer of length B
template <typename T, uint32_t B = sizeof(T)>
class ByteReader {
 public:
  static T ReadBigEndian(const uint8_t* buffer) {
    T out_data;
    uint8_t* data_byte = reinterpret_cast<uint8_t*>(&out_data);
    for (int i = 0; i < B; i++) {
      *(data_byte + i) = *(buffer + (B - 1) - i);
    }
    if (std::numeric_limits<T>::is_signed) {
      for (int i = B; i < sizeof(T); ++i) {
        *(data_byte + i) = ((*(data_byte + B)) & 0x80) ? 0xff : 0x00;
      }
    }
    return out_data;
  }
  static T ReadLittleEndian(const uint8_t* buffer) {
    T out_data;
    uint8_t* data_byte = reinterpret_cast<uint8_t*>(&out_data);
    for (int i = 0; i < B; i++) {
      *(data_byte + i) = *(buffer + i);
    }
    if (std::numeric_limits<T>::is_signed) {
      for (int i = B; i < sizeof(T); i++) {
        *(data_byte + i) = ((*(data_byte + B)) & 0x80) ? 0xff : 0x00;
      }
    }
    return out_data;
  }
};
}  // namespace qosrtp
#pragma once
#include <memory>

#include "define.h"

namespace qosrtp {
class QOSRTP_API DataBuffer {
 public:
  static std::unique_ptr<DataBuffer> Create(uint32_t capacity);
  DataBuffer();
  virtual ~DataBuffer();
  virtual uint32_t Append(uint32_t size_appended) = 0;
  virtual uint32_t CutTail(uint32_t size_cut) = 0;
  virtual uint32_t SetSize(uint32_t size) = 0;
  // Only allow to modify the position smaller than size
  virtual bool ModifyAt(uint32_t pos, const uint8_t* data,
                        uint32_t size_modified) = 0;
  virtual bool MemSet(uint32_t pos, uint8_t value, uint32_t size_set) = 0;
  // Only allow to get pointer whose position smaller than size
  virtual const uint8_t* At(uint32_t pos) const = 0;
  virtual const uint8_t* Get() const = 0;
  virtual uint8_t* GetW() = 0;
  virtual uint32_t size() const = 0;
  virtual uint32_t capacity() const = 0;
};
}  // namespace qosrtp
#pragma once
#include <memory>

#include "../include/data_buffer.h"

namespace qosrtp {
class DataBufferImpl : public DataBuffer {
 public:
  DataBufferImpl() = delete;
  DataBufferImpl(uint32_t capacity)
      : DataBuffer(), buffer_(capacity > 0 ? new uint8_t[capacity] : nullptr) {
    capacity_ = capacity;
    size_ = 0;
  }
  virtual ~DataBufferImpl() override;
  virtual uint32_t Append(uint32_t size_appended) override;
  virtual uint32_t CutTail(uint32_t size_cut) override;
  virtual uint32_t SetSize(uint32_t size) override;
  // Only allow to modify the position smaller than size
  virtual bool ModifyAt(uint32_t pos, const uint8_t* data,
                        uint32_t size_modified) override;
  virtual bool MemSet(uint32_t pos, uint8_t value, uint32_t size_set) override;
  // Only allow to get pointer whose position smaller than size
  virtual const uint8_t* At(uint32_t pos) const override;
  virtual const uint8_t* Get() const override;
  virtual uint8_t* GetW() override;
  virtual uint32_t size() const override;
  virtual uint32_t capacity() const override;

 private:
  uint32_t size_;
  uint32_t capacity_;
  std::unique_ptr<uint8_t> buffer_;
};
}  // namespace qosrtp
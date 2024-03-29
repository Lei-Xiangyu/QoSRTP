#include "./data_buffer_impl.h"

namespace qosrtp {
DataBufferImpl::~DataBufferImpl() {}

uint32_t DataBufferImpl::Append(uint32_t size_appended) {
  uint32_t size_appended_real =
      (size_ + size_appended) > capacity_ ? (capacity_ - size_) : size_appended;
  size_ += size_appended_real;
  return size_appended_real;
}

uint32_t DataBufferImpl::CutTail(uint32_t size_cut) {
  uint32_t size_cut_real = (size_ - size_cut) < 0 ? size_ : size_cut;
  size_ -= size_cut_real;
  return size_cut_real;
}

uint32_t DataBufferImpl::SetSize(uint32_t size) {
  size_ = size > capacity_ ? capacity_ : size;
  return size_;
}

// Only allow to modify the position smaller than size
bool DataBufferImpl::ModifyAt(uint32_t pos, const uint8_t* data,
                              uint32_t size_modified) {
  if ((pos > size_) || ((pos + size_modified) > size_)) {
    return false;
  }
  std::memcpy(buffer_.get() + pos, data, size_modified);
  return true;
}

bool DataBufferImpl::MemSet(uint32_t pos, uint8_t value, uint32_t size_set) {
  if ((pos > size_) || ((pos + size_set) > size_)) {
    return false;
  }
  std::memset(buffer_.get() + pos, value, size_set);
  return true;
}

// Only allow to get pointer whose position smaller than size
const uint8_t* DataBufferImpl::At(uint32_t pos) const {
  if (pos > size_) {
    return nullptr;
  }
  return buffer_.get() + pos;
}

const uint8_t* DataBufferImpl::Get() const { return buffer_.get(); }

uint8_t* DataBufferImpl::GetW() { return buffer_.get(); }

uint32_t DataBufferImpl::size() const { return size_; }

uint32_t DataBufferImpl::capacity() const { return capacity_; }
}  // namespace qosrtp
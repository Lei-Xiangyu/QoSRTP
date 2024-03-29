#include "../include/data_buffer.h"
#include "data_buffer_impl.h"
using namespace qosrtp;

std::unique_ptr<DataBuffer> DataBuffer::Create(uint32_t capacity) {
  return std::make_unique<DataBufferImpl>(capacity);
}

DataBuffer::DataBuffer() = default;

DataBuffer::~DataBuffer() = default;
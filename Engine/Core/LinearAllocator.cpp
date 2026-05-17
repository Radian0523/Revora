#include "LinearAllocator.h"

#include <cstdlib>
#include <cassert>

namespace Revora {

LinearAllocator::~LinearAllocator()
{
    std::free(buffer_);
    buffer_ = nullptr;
}

void LinearAllocator::Initialize(std::size_t capacity)
{
    assert(buffer_ == nullptr && "LinearAllocator is already initialized");

    buffer_   = static_cast<uint8_t*>(std::malloc(capacity));
    capacity_ = capacity;
    offset_   = 0;
}

void* LinearAllocator::Allocate(std::size_t size, std::size_t alignment)
{
    // アライメントに合わせてオフセットを切り上げる
    // alignment は 2 の累乗であることを前提とする
    std::size_t alignedOffset = (offset_ + alignment - 1) & ~(alignment - 1);

    if (alignedOffset + size > capacity_) {
        return nullptr;
    }

    void* ptr = buffer_ + alignedOffset;
    offset_ = alignedOffset + size;
    return ptr;
}

void LinearAllocator::Reset()
{
    offset_ = 0;
}

} // namespace Revora

// provide new/delete operators for the kernel
#ifndef MM_ALLOCATOR_H
#define MM_ALLOCATOR_H

#include <new>

void* operator new(std::size_t size);

void* operator new[](std::size_t size);

void* operator new(std::size_t size, const std::nothrow_t&) noexcept;

void operator delete(void* ptr);

void operator delete(void* ptr, std::size_t size);

void operator delete[](void* ptr);

#endif // MM_ALLOCATOR_H
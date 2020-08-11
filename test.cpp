#include "allocator.h"
#include "size_allocator.h"

#include <iostream>
#include <sys/mman.h>

#include <fcntl.h>

struct S {
    char t[128];
};

int main() {
    using namespace slab;
    auto mem = open("/dev/zero", O_RDWR);
    auto * ptr = (std::byte*)mmap(nullptr, 1 << 20, PROT_READ | PROT_WRITE, MAP_PRIVATE, mem, 0);
    auto fd = open("/dev/zero", O_RDWR);
    auto slab = new (mmap(nullptr, 1 << 20, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0)) UniquePtrWrap<DynamicLockLessAllocator>(ptr, ptr + (1 << 30), sizeof(S));
    auto x = slab->alloc();
    std::cout << sizeof(x) << std::endl;

    auto alloc = createVarAllocator();
    auto y = alloc.alloc<S>();
}
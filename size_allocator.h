//
// Created by Maurice Bailleu on 10/08/2020.
//

#pragma  once

#include "allocator.h"

#include <cstddef>
#include <memory>
#include <cassert>
#include <atomic>
#include <iostream>
#include <map>


namespace slab {

#if 1
class VarAllocator {
public:
  using Allocator = DynamicLockLessAllocator;

  template<class T>
  struct Deleter {
    using Base = VarAllocator;
    Base * base;
    Deleter(Base * base) : base(base) {}
    void operator() (std::remove_extent_t<T> * ptr) {
      assert(base != nullptr);
      assert(ptr != nullptr);
      using element_t = std::remove_extent_t<T>;
      ptr->~element_t();
      base->dealloc(reinterpret_cast<std::byte *>(ptr));
    }
  };

  std::map<size_t, std::unique_ptr<Allocator>> allocators;

  VarAllocator() = default;

  void add(std::unique_ptr<Allocator> allocator) {
    allocators.insert({allocator->element_size, std::move(allocator)});
  }

  void * alloc(std::size_t size) noexcept {
    auto it = allocators.lower_bound(size);
    if (it == allocators.end()) {
      return nullptr;
    }
    auto & allocator = (*it).second;
    auto * buf = allocator->alloc();
    if (buf == nullptr) {
      return nullptr;
    }
    return buf;
  }

  template<class T, class ... Args>
  std::unique_ptr<T, Deleter<T>> create(Args && ... args) {
    auto buf = alloc(sizeof(T));
    T * res = new (buf) T(std::forward<Args>(args)...);
    return {res, this};
  }

  template<class T>
  void dealloc(T * value) {
    auto it = allocators.lower_bound(sizeof(T));
    if (it == allocators.end()) {
      assert(false); //This should never happen
    }
    auto & allocator = (*it).second;
    allocator->dealloc(reinterpret_cast<std::byte *>(value));
  }

  template<class T>
  void dealloc(std::unique_ptr<T, Deleter<T>> value) {
    value.reset();
  }
};

#if 0
static VarAllocator createVarAllocator() {
  VarAllocator res;
  for (auto size : {8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384}) {
    auto buf_size = VarAllocator::Allocator::dataSize(10000, size);
    auto start = new std::byte[buf_size];
    auto alloc = new DynamicLockLessAllocator(start, start + buf_size, size);
    res.add(alloc);
  }
  return res;
}

static void destroyAllocator(VarAllocator & allocator) {
  (void) allocator;
}
#endif

#endif
} //namespace slab

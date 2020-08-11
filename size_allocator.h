//
// Created by Maurice Bailleu on 10/08/2020.
//

#pragma  once

#include <set>
#include <memory>
#include <cassert>

#include "allocator.h"

class VarAllocator {
  using Allocator = DynamicLockLessAllocator;

  template<class T>
  struct Deleter {
    Allocator * base;
    void operator() (T * ptr) {
      assert(base != nullptr);
      assert(ptr != nullptr);
      ptr->~T();
      base->dealloc(ptr);
    }
  };

  std::map<size_t, Allocator> allocators;

  VarAllocator() = default;

  void add(Allocator && allocator) {
    allocators.insert(allocator.element_size, std::move(allocator));
  }

  template<class T, class ... Args>
  std::unique_ptr<T, Deleter<T>> alloc(Args && ... args) {
    auto allocator = allocators.lower_bound(sizeof(T));
    if (alloctor == allocators.end()) {
      return {nullptr, nullptr};
    }
    auto * buf = allocator->alloc();
    if (buf == nullptr) {
      return {nullptr, &(*allocator)};
    }
    T * res = new (buf) T(std::forward<Args>(args)...);
    return {res, &(*allocator)};
  }

  template<class T>
  void dealloc(std::unqiue_ptr<T, Deleter<T>> value) {
    value.reset();
  }
};

VarAllocator createVarAllocator() {
  VarAllocator res;
  for (auto size : {8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384}) {
    auto buf_size = VarAllocator::Allocator::dataSize(10000, size);
    auto start = new std::byte[buf_size];
    DynamicLockLessAllocator alloc(start, start + buf_size, size);
    res.add(std::move(alloc));
  }
  return res;
}

void destroyAllocator(VarAllocator & allocator) {
  //TODO
}
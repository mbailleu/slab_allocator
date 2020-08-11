//
// Created by Maurice Bailleu on 10/08/2020.
//

#pragma  once

#include <cstddef>
#include <set>
#include <memory>
#include <cassert>
#include <atomic>

#include "allocator.h"

class VarAllocator {
  using Allocator = DynamicLockLessAllocator;

#if ALLOCATE_STATS
  std::atomic<size_t> max_heap = 0;
  std::atomic<size_t> current_heap = 0;
#endif

  template<class T>
  struct Deleter {
#if !ALLOCATE_STATS
    Allocator * base;
#else
    VarAllocator * base;
#endif
    void operator() (T * ptr) {
      assert(base != nullptr);
      assert(ptr != nullptr);
      ptr->~T();
      base->dealloc(reinterpret_cast<std::byte *>(ptr));
    }
  };

  std::map<size_t, Allocator> allocators;

  VarAllocator() = default;

  void add(Allocator && allocator) {
    allocators.insert(allocator.element_size, std::move(allocator));
  }

  template<class T, class ... Args>
  std::unique_ptr<T, Deleter<T>> alloc(Args && ... args) {
    auto it = allocators.lower_bound(sizeof(T));
    if (it == allocators.end()) {
      return {nullptr, nullptr};
    }
    auto & allocator = (*it).second;
    auto * buf = allocator.alloc();
    if (buf == nullptr) {
#if !ALLOCATE_STATS
      return {nullptr, &allocator};
#else
      return {nullptr, this}
#endif
    }
#if ALLOCATE_STATS
    auto current_heap = this->current_heap.fetch_add(allocator.element_size, std::memory_order_relaxed);
    current_heap += allocator.element_size;
    auto max = max_heap.load(std::memory_order_relaxed);
    while (max < current_heap && !max_heap.compare_exchange_weak(max, current_heap, std::memory_order_release, std::memory_order_relaxed)) {}
#endif
    T * res = new (buf) T(std::forward<Args>(args)...);
#if !ALLOCATE_STATS
    return {res, &allocator};
#else
    return {res, this};
#endif
  }

  template<class T>
  void dealloc(T * value) {
    auto allocator = allocators.lower_bound(sizeof(T));
    if (it == allocators.end()) {
      assert(false); //This should never happen
    }
    auto & allocator = (*it).second;
    allocator.dealloc(reinterpret_cast<std::byte *>(value));
#if ALLOCATE_STATS
    current_heap.fetch_add(-allocator.element_size, std::memory_order_relaxed);
#endif
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
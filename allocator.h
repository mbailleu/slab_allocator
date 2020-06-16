#pragma once

#include <thread>
#include <mutex>
#include <cassert>
#include <memory>
#include <atomic>

namespace slab {

template<class T>
class Allocator {

  T * start_region;
  T * current_start;
  T * end_region;
  std::mutex m;
  T ** stack;
  size_t pos = 0;

public:

  static constexpr metaDataSize(size_t n_elements) {
    return sizeof(Allocator) + n_elements * sizeof(T*);
  }

  static constexpr dataSize(size_t n_elements) {
    return sizeof(T) * n_elements;
  }

  Allocator(T * start, T * end) : start_region(start), current_start(start), end_region(end) {
    stack = reinterpret_cast<T**>(this + 1);
  }

  T * alloc() {
    std::lock_guard const l(m);
    assert(pos >= 0);
    if (pos == 0) {
      if (current_start < end_region) {
        auto res = current_start;
        ++current_start;
        return res;
      }
      return nullptr;
    }
    --pos;
    return stack[pos];
  }


  void dealloc(T * ptr) {
    std::lock_guard const l(m);
    assert(pos >= 0);
    stack[pos] = ptr;
    ++pos;
  }
};

template<class T>
class LockLessAllocator {
    T * start_region;
    T * end_region;

    struct Node {
      T ptr;
      Node * prev;
    };

    std::atomic<Node *> head;
    std::atomic<Node *> next;
    static_assert(std::atomic<Node *>::is_always_lock_free);

public:

    using Value_t = T;

    static constexpr metaDataSize(size_t n_elements) {
      return sizeof(LockLessAllocator);
    }

    static constexpr dataSize(size_t n_elements) {
      return n_elements * sizeof(Node);
    }

    LockLessAllocator(T * start, T * end) : start_region(start), end_region(end), head(nullptr), next(start) {}

    T * alloc() {
        if (auto h = head.load(std::memory_order_relaxed); h != nullptr) {
            while (!head.compare_exchange_weak(h, h->prev, std::memory_order_release, std::memory_order_relaxed) && h != nullptr) {}
            if (h != nullptr) {
                h->prev = nullptr;
                return reinterpret_cast<T*>(h);
            }
        }
        Node * n = next.fetch_add(1, std::memory_order_relaxed);
        return reinterpret_cast<T*>(n);
    }

    void dealloc(T * ptr) {
        auto n = reinterpret_cast<Node *>(ptr);
        n->prev = head.load(std::memory_order_relaxed);
        while (!head.compare_exchange_weak(n->prev, n, std::memory_order_release, std::memory_order_relaxed)) {}
    }
};

template<class Allocator>
class UniquePtrWrap : public Allocator {
public:
    using Value_t = Allocator::Value_t;

    struct Deleter {
       Allocator * base;
       void operator(Value_t * ptr) {
           base->dealloc(ptr);
       }
    } d;

    using Ptr_t = std::unique_ptr<Value_t *, Deleter>;

    template<class ... Args>
    UniquePtrWrap(Args && ... args) : Allocator(std::forward<Args>(args)...), d(this) {}

    Ptr_t alloc() {
        return {Allocator::alloc(), d};
    }

    void dealloc(Ptr_t value) {
        value.reset();
    }
}

} //namespace buddy

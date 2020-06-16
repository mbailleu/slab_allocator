#pragma once

#include <thread>
#include <mutex>
#include <cassert>
#include <memory>
#include <atomic>
#include <cstddef>

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

  static constexpr size_t metaDataSize(size_t n_elements) {
    return sizeof(Allocator) + n_elements * sizeof(T*);
  }

  static constexpr size_t dataSize(size_t n_elements) {
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

    static constexpr size_t metaDataSize(size_t n_elements) {
      return sizeof(LockLessAllocator);
    }

    static constexpr size_t dataSize(size_t n_elements) {
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

class DynamicLockLessAllocator {
    std::byte * const start_region;
    std::byte * const end_region;

    struct Node {
        Node * prev;
        std::byte val[];
    };

    std::atomic<Node *>      head;
    std::atomic<std::byte *> next;
    static_assert(std::atomic<Node *>::is_always_lock_free);
    static_assert(std::atomic<std::byte *>::is_always_lock_free);

    size_t const element_size;

public:

    static constexpr size_t metaDataSize(size_t n_elements) {
        return sizeof(DynamicLockLessAllocator);
    }

    size_t dataSize(size_t n_elements) const {
        return n_elements * (element_size + sizeof(Node));
    }



    DynamicLockLessAllocator(std::byte * start, std::byte * end, size_t element_size) : start_region(start), end_region(end), head(nullptr), next(start), element_size([] (auto x) {
              auto remainder = x % 8;
              if (remainder == 0) return x;
              return x + (8 - remainder);
            }(element_size)) {}

    inline std::byte * alloc() {
        if (auto h = head.load(std::memory_order_relaxed); h != nullptr) {
            while (!head.compare_exchange_weak(h, h->prev, std::memory_order_release, std::memory_order_relaxed) && h != nullptr) {}
            if (h != nullptr) {
                h->prev = nullptr;
                return h->val;
            }
        }
        auto * buf = next.fetch_add(sizeof(Node) + element_size, std::memory_order_relaxed);
        auto n = reinterpret_cast<Node *>(buf);
        n->prev = nullptr;
        return n->val;
    }

    void dealloc(std::byte * ptr) {
        auto n = reinterpret_cast<Node *>(ptr - offsetof(Node, val));
        n->prev = head.load(std::memory_order_relaxed);
        while(!head.compare_exchange_weak(n->prev, n, std::memory_order_release, std::memory_order_relaxed)) {}
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

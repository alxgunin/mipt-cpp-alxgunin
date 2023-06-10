#pragma once
#include <memory>
#include <type_traits>
#include <iostream>

template <typename T>
class WeakPtr;

template <typename T>
class EnableSharedFromThis;

namespace blocks {
struct BaseControlBlock {
  int weak_count;
  int shared_count;
  BaseControlBlock(int weak, int shared)
      : weak_count(weak),
        shared_count(shared) {
  }
  virtual void useDeleter() = 0;
  virtual void dealloc() = 0;
  virtual ~BaseControlBlock() = default;
};

template <typename U, typename Deleter = std::default_delete<U>,
          typename Alloc = std::allocator<U>>
struct BaseControlBlockRegular : BaseControlBlock {
  U* object;
  Deleter deleter;
  Alloc alloc;
  BaseControlBlockRegular(int shared_count, int weak_count, U* pt)
      : BaseControlBlock(weak_count, shared_count),
        object(pt) {
  }

  BaseControlBlockRegular(int shared_count, int weak_count, U* pt,
                          const Deleter& deleter, const Alloc& alloc)
      : BaseControlBlock(weak_count, shared_count),
        object(pt),
        deleter(deleter),
        alloc(alloc) {
  }

  virtual void useDeleter() final {
    deleter(object);
  }
  using AllocBaseControlBlock = typename std::allocator_traits<
      Alloc>::template rebind_alloc<BaseControlBlockRegular>;
  using AllocTraits = std::allocator_traits<AllocBaseControlBlock>;
  AllocBaseControlBlock newAlloc = alloc;
  virtual void dealloc() final {
    AllocTraits::deallocate(newAlloc, this, 1);
  }
  ~BaseControlBlockRegular() = default;
};

template <typename T, typename Alloc = std::allocator<T>>
struct BaseControlBlockMakeShared : BaseControlBlock {
  T object;
  Alloc alloc;
  using AllocT =
      typename std::allocator_traits<Alloc>::template rebind_alloc<T>;
  using TTraits = std::allocator_traits<AllocT>;
  AllocT allocT = alloc;
  template <typename... Args>
  BaseControlBlockMakeShared(int shared_count, int weak_count,
                             const Alloc& alloc, Args&&... args)
      : BaseControlBlock(weak_count, shared_count),
        object(std::forward<Args>(args)...),
        alloc(alloc) {
  }

  virtual void useDeleter() final {
    TTraits::destroy(allocT, &object);
  }

  using AllocBaseControlBlock = typename std::allocator_traits<
      Alloc>::template rebind_alloc<BaseControlBlockMakeShared>;
  using AllocTraits = std::allocator_traits<AllocBaseControlBlock>;
  AllocBaseControlBlock newAlloc = alloc;
  virtual void dealloc() final {
    AllocTraits::deallocate(newAlloc, this, 1);
  }
  ~BaseControlBlockMakeShared() = default;
};
}  // namespace blocks

template <typename T>
class SharedPtr {
 private:
  template <typename U>
  friend class WeakPtr;

  template <typename U>
  friend class SharedPtr;

  T* ptr = nullptr;
  blocks::BaseControlBlock* cb = nullptr;

  void update_wptr() {
    if (cb == nullptr)
      return;
    if constexpr (std::is_base_of_v<EnableSharedFromThis<T>, T>) {
      ptr->wptr = *this;
    }
  }

  void add() {
    if (cb != nullptr)
      cb->shared_count++;
  }

  SharedPtr(const WeakPtr<T>& other)
      : ptr(other.ptr),
        cb(other.cb) {
    add();
  }

  void destructor() {
    if (cb == nullptr)
      return;
    cb->shared_count--;
    if (cb->shared_count == 0) {
      cb->useDeleter();
      if (cb->weak_count == 0) {
        cb->dealloc();
      }
    }
  }

  SharedPtr(blocks::BaseControlBlock* cb)
      : cb(cb) {
  }

 public:
  void swap(SharedPtr& other) {
    std::swap(ptr, other.ptr);
    std::swap(cb, other.cb);
  }

  SharedPtr() = default;

  template <typename U = T, typename Deleter = std::default_delete<U>,
            typename Alloc = std::allocator<U>>
  SharedPtr(U* ptr, const Deleter& deleter, const Alloc& alloc)
      : ptr(static_cast<T*>(ptr)) {
    using AllocBaseControlBlock =
        typename std::allocator_traits<Alloc>::template rebind_alloc<
            blocks::BaseControlBlockRegular<U, Deleter, Alloc>>;
    using AllocTraits = std::allocator_traits<AllocBaseControlBlock>;
    AllocBaseControlBlock newAlloc = alloc;
    cb = AllocTraits::allocate(newAlloc, 1);
    AllocTraits::construct(newAlloc, cb,
                           blocks::BaseControlBlockRegular<U, Deleter, Alloc>(
                               1, 0, ptr, deleter, alloc));
    update_wptr();
  }

  SharedPtr(const SharedPtr& other)
      : ptr(other.ptr),
        cb(other.cb) {
    add();
  }

  template <typename U>
  SharedPtr(const SharedPtr<U>& other)
      : ptr(static_cast<T*>(other.ptr)),
        cb(other.cb) {
    add();
  }

  SharedPtr& operator=(const SharedPtr& other) {
    SharedPtr copy = other;
    swap(copy);
    return *this;
  }

  template <typename U>
  SharedPtr& operator=(const SharedPtr<U>& other) {
    SharedPtr copy = other;
    swap(copy);
    return *this;
  }

  SharedPtr(SharedPtr&& other)
      : ptr(other.ptr),
        cb(other.cb) {
    update_wptr();
    other.ptr = nullptr;
    other.cb = nullptr;
  }

  template <typename U>
  SharedPtr(SharedPtr<U>&& other)
      : ptr(other.ptr),
        cb(other.cb) {
    update_wptr();
    other.ptr = nullptr;
    other.cb = nullptr;
  }

  SharedPtr& operator=(SharedPtr&& other) {
    SharedPtr copy = std::move(other);
    swap(copy);
    return *this;
  }

  template <typename U>
  SharedPtr& operator=(SharedPtr<U>&& other) {
    SharedPtr copy = std::move(other);
    swap(copy);
    return *this;
  }

  ~SharedPtr() {
    destructor();
  }

  int use_count() const {
    return cb->shared_count;
  }

  template <typename U = T>
  void reset(U* pt = nullptr) {
    destructor();
    cb = nullptr;
    ptr = nullptr;
    if (pt) {
      *this = SharedPtr<U>(pt);
    }
  }

  T& operator*() const {
    if (ptr) {
      return *ptr;
    }
    return (static_cast<blocks::BaseControlBlockMakeShared<T>*>(cb)->object);
  }

  T* operator->() const {
    if (ptr) {
      return ptr;
    }
    return &(static_cast<blocks::BaseControlBlockMakeShared<T>*>(cb)->object);
  }

  T* get() const noexcept {
    return ptr;
  }
};

template <typename T>
class WeakPtr {
 private:
  template <typename U>
  friend class SharedPtr;

  template <typename U>
  friend class WeakPtr;

  T* ptr = nullptr;
  blocks::BaseControlBlock* cb = nullptr;

  void add() {
    if (cb != nullptr)
      cb->weak_count++;
  }

 public:
  void swap(WeakPtr& other) {
    std::swap(ptr, other.ptr);
    std::swap(cb, other.cb);
  }

  WeakPtr() = default;

  WeakPtr(const SharedPtr<T>& other)
      : ptr(other.ptr),
        cb(other.cb) {
    add();
  }

  template <typename U>
  WeakPtr(const SharedPtr<U>& other)
      : ptr(static_cast<T*>(other.ptr)),
        cb(other.cb) {
    add();
  }

  WeakPtr(const WeakPtr& other)
      : ptr(other.ptr),
        cb(other.cb) {
    add();
  }

  template <typename U>
  WeakPtr(const WeakPtr<U>& other)
      : ptr(static_cast<T*>(other.ptr)),
        cb(other.cb) {
    add();
  }

  WeakPtr& operator=(const WeakPtr& other) {
    WeakPtr copy = other;
    swap(copy);
    return *this;
  }

  template <typename U>
  WeakPtr& operator=(WeakPtr<U> other) {
    WeakPtr copy = other;
    swap(copy);
    return *this;
  }

  WeakPtr(WeakPtr&& other)
      : ptr(other.ptr),
        cb(other.cb) {
    add();
    other.ptr = nullptr;
    other.cb = nullptr;
  }

  template <typename U>
  WeakPtr(WeakPtr<U>&& other)
      : ptr(static_cast<T*>(other.ptr)),
        cb(other.cb) {
    add();
    other.ptr = nullptr;
    other.cb = nullptr;
  }

  WeakPtr& operator=(WeakPtr&& other) {
    WeakPtr copy = std::move(other);
    swap(copy);
    return *this;
  }

  template <typename U>
  WeakPtr& operator=(WeakPtr<U>&& other) {
    WeakPtr copy = std::move(other);
    swap(copy);
    return *this;
  }

  bool expired() const noexcept {
    return cb->shared_count == 0;
  }

  SharedPtr<T> lock() const noexcept {
    if (expired())
      return SharedPtr<T>();
    return SharedPtr<T>(*this);
  }

  ~WeakPtr() {
    if (cb == nullptr)
      return;
    cb->weak_count--;
    if (cb->weak_count == 0 && cb->shared_count == 0) {
      cb->dealloc();
    }
  }

  T& operator*() const {
    if (ptr) {
      return *ptr;
    }
    return (static_cast<blocks::BaseControlBlockMakeShared<T>*>(cb)->object);
  }

  T* operator->() const {
    if (ptr) {
      return ptr;
    }
    return &(static_cast<blocks::BaseControlBlockMakeShared<T>*>(cb)->object);
  }

  int use_count() const {
    return cb->shared_count;
  }
};

template <typename T, typename... Args>
SharedPtr<T> makeShared(Args&&... args) {
  blocks::BaseControlBlockMakeShared<T>* cb =
      new blocks::BaseControlBlockMakeShared<T>(1, 0, std::allocator<T>(),
                                                std::forward<Args>(args)...);
  return SharedPtr<T>(static_cast<blocks::BaseControlBlock*>(cb));
}

template <typename T, typename Alloc, typename... Args>
SharedPtr<T> allocateShared(const Alloc& alloc, Args&&... args) {
  using AllocBaseControlBlockMakeShared =
      typename std::allocator_traits<Alloc>::template rebind_alloc<
          blocks::BaseControlBlockMakeShared<T, Alloc>>;
  using AllocTraits = std::allocator_traits<AllocBaseControlBlockMakeShared>;
  AllocBaseControlBlockMakeShared newAlloc = alloc;
  blocks::BaseControlBlockMakeShared<T, Alloc>* cb =
      AllocTraits::allocate(newAlloc, 1);
  AllocTraits::construct(newAlloc, cb, 1, 0, alloc,
                         std::forward<Args>(args)...);
  return SharedPtr<T>(static_cast<blocks::BaseControlBlock*>(cb));
}

template <typename T>
class EnableSharedFromThis {
 private:
  WeakPtr<T> wptr;

 public:
  SharedPtr<T> shared_from_this() const noexcept {
    return wptr.lock();
  }
};
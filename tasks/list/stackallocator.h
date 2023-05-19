#pragma once
#include <iostream>
#include <memory>
#include <algorithm>
#include <utility>

template <size_t N>
class StackStorage {
 public:
  void* arr[N];
  size_t shift = 0;
  StackStorage() = default;
  ~StackStorage() = default;
  StackStorage(const StackStorage& other) = delete;
  StackStorage& operator=(StackStorage other) = delete;
};

template <typename T, size_t N>
class StackAllocator {
 public:
  StackStorage<N>* pt_arr;

  using value_type = T;
  using propagate_on_container_copy_assignment = std::true_type;

  T* allocate(size_t count) {
    size_t space = N - pt_arr->shift;
    void* start = reinterpret_cast<void*>(
        reinterpret_cast<size_t>(pt_arr->arr) + pt_arr->shift);
    start = std::align(alignof(T), count * sizeof(T), start, space);
    pt_arr->shift = count * sizeof(T) + reinterpret_cast<size_t>(start) -
                    reinterpret_cast<size_t>(pt_arr->arr);
    return static_cast<T*>(start);
  }

  StackAllocator()
      : pt_arr(nullptr) {
  }
  StackAllocator(StackStorage<N>& other)
      : pt_arr(&other) {
  }
  ~StackAllocator() = default;
  template <typename U>
  StackAllocator& operator=(const StackAllocator<U, N>& other) {
    pt_arr = other.pt_arr;
    return *this;
  }
  StackAllocator(const StackAllocator& other) = default;
  StackAllocator& select_on_container_copy_construction() {
    return *this;
  }

  bool operator==(const StackAllocator& other) {
    return pt_arr == other.pt_arr;
  }

  bool operator!=(const StackAllocator& other) {
    return !(*this == other);
  }

  void deallocate(T*, size_t) {
  }

  template <typename U>
  struct rebind {
    using other = StackAllocator<U, N>;
  };

  template <typename U>
  StackAllocator(const StackAllocator<U, N>& other)
      : pt_arr(other.pt_arr) {
  }

  void swap(StackAllocator& other) {
    std::swap(other.pt_arr, pt_arr);
  }
};

template <typename T, typename Alloc = std::allocator<T>>
class List {
 private:
  using AllocTraits = std::allocator_traits<Alloc>;
  size_t size_ = 0;
  struct BaseNode {
    BaseNode* next = nullptr;
    BaseNode* prev = nullptr;
    BaseNode() = default;
    BaseNode(const BaseNode& other) = default;
    BaseNode(BaseNode* a, BaseNode* b)
        : next(a),
          prev(b) {
    }
    virtual ~BaseNode() = default;
  };

  struct Node : BaseNode {
    T value;
    Node() = default;
    Node(const T& value)
        : value(value) {
    }
    ~Node() = default;
    Node(const Node& other) = default;
  };

  using NodeAlloc = typename AllocTraits::template rebind_alloc<Node>;
  [[no_unique_address]] NodeAlloc node_alloc_;
  BaseNode fake_node_ = BaseNode(&fake_node_, &fake_node_);

 public:
  Alloc get_allocator() const {
    return static_cast<Alloc>(node_alloc_);
  }

  List(const Alloc& alloc = Alloc())
      : size_(0),
        node_alloc_(alloc) {
  }

  size_t size() const {
    return size_;
  }

  template <bool is_const>
  struct common_iterator {
    BaseNode* node = nullptr;
    using pointer = std::conditional_t<is_const, const T*, T*>;
    using reference = std::conditional_t<is_const, const T&, T&>;
    using value_type = std::conditional_t<is_const, const T, T>;
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = ptrdiff_t;

    common_iterator() = default;
    common_iterator(BaseNode* ptr)
        : node(ptr) {
    }

    common_iterator operator++(int) {
      common_iterator copy = *this;
      node = node->next;
      return copy;
    }
    common_iterator& operator++() {
      node = node->next;
      return *this;
    }
    common_iterator operator--(int) {
      common_iterator copy = *this;
      node = node->prev;
      return copy;
    }
    common_iterator& operator--() {
      node = node->prev;
      return *this;
    }
    reference operator*() {
      Node* temp = static_cast<Node*>(node);
      return temp->value;
    }
    pointer operator->() const {
      return node;
    }
    bool operator==(const common_iterator& other) const {
      return node == other.node;
    }
    bool operator!=(const common_iterator& other) const {
      return !(*this == other);
    }

    operator common_iterator<true>() const {
      return common_iterator<true>(node);
    }
  };

  typedef common_iterator<false> iterator;
  typedef common_iterator<true> const_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

  void replace_pointers(Node* new_node, const_iterator it) {
    new_node->next = it.node;
    new_node->prev = it.node->prev;
    const_iterator prev_it = (--it);
    ++it;
    it.node->prev = new_node;
    if (size_ == 0) {
      it.node->next = new_node;
    } else {
      prev_it.node->next = new_node;
    }
    size_++;
  }


  void insert(const_iterator it) {
    Node* new_node = std::allocator_traits<NodeAlloc>::allocate(node_alloc_, 1);
    try {
      std::allocator_traits<NodeAlloc>::construct(node_alloc_, new_node);

    } catch (...) {
      std::allocator_traits<NodeAlloc>::deallocate(node_alloc_, new_node, 1);
      throw;
    }
    replace_pointers(new_node, it);
  }

  void insert(const_iterator it, const T& value) {
    Node* new_node = std::allocator_traits<NodeAlloc>::allocate(node_alloc_, 1);
    try {
      std::allocator_traits<NodeAlloc>::construct(node_alloc_, new_node, value);
    } catch (...) {
      std::allocator_traits<NodeAlloc>::deallocate(node_alloc_, new_node, 1);
      throw;
    }
    replace_pointers(new_node, it);
  }

  void erase(const_iterator it) {
    Node* erasing_ptr = static_cast<Node*>(it.node);
    const_iterator prev_it = (--it);
    ++it;
    ++it;
    prev_it.node->next = it.node;
    it.node->prev = prev_it.node;
    --size_;
    std::allocator_traits<NodeAlloc>::destroy(node_alloc_, erasing_ptr);
    std::allocator_traits<NodeAlloc>::deallocate(node_alloc_, erasing_ptr, 1);
  }

  iterator begin() {
    return iterator(fake_node_.next);
  }
  const_iterator begin() const {
    return const_iterator(fake_node_.next);
  }

  iterator end() {
    return iterator((fake_node_.next)->prev);
  }
  const_iterator end() const {
    return const_iterator((fake_node_.next)->prev);
  }

  const_iterator cbegin() const {
    return const_iterator(begin());
  }
  const_iterator cend() const {
    return const_iterator(end());
  }

  reverse_iterator rbegin() {
    return reverse_iterator(end());
  }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }

  reverse_iterator rend() {
    return reverse_iterator(begin());
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }

  const_reverse_iterator crbegin() const {
    return const_reverse_iterator(cend());
  }
  const_reverse_iterator crend() const {
    return const_reverse_iterator(cbegin());
  }

  void push_back(const T& value) {
    insert(end(), value);
  }
  void push_front(const T& value) {
    insert(begin(), value);
  }
  void pop_back() {
    erase(--end());
  }
  void pop_front() {
    erase(begin());
  }

  List(size_t sz, const Alloc& alloc = Alloc())
      : node_alloc_(alloc) {
    size_t i = 0;
    try {
      for (; i < sz; ++i) {
        insert(begin());
      }
    } catch (...) {
      for (size_t j = 0; j < i; ++j) {
        pop_back();
      }
      throw;
    }
  }

  ~List() {
    while (size_ != 0) {
      pop_front();
    }
  }

  List(size_t sz, const T& value, const Alloc& alloc = Alloc())
      : node_alloc_(alloc),
        size_(sz) {
    size_t i = 0;
    try {
      for (; i < size_; ++i) {
        insert(begin(), value, false);
      }
    } catch (...) {
      for (size_t j = 0; j < i; ++j) {
        erase(begin());
      }
      throw;
    }
  }

  List(const List& other)
      : node_alloc_(AllocTraits::select_on_container_copy_construction(
            other.node_alloc_)) {
    size_t i = 0;
    const_iterator it = other.begin();
    try {
      for (; i < other.size_; ++i) {
        push_back(*it);
        (++it);
      }
    } catch (...) {
      for (size_t j = 0; j < i; ++j) {
        pop_front();
      }
      throw;
    }
  }

  void swap(List& other) {
    std::swap(other.fake_node_, fake_node_);
    std::swap(other.size_, size_);
    std::swap(node_alloc_, other.node_alloc_);
    fake_node_.next->prev = fake_node_.prev->next = &fake_node_;
    other.fake_node_.next->prev = other.fake_node_.prev->next =
        &other.fake_node_;
  }

  List& operator=(const List& other) {
    List copy(AllocTraits::propagate_on_container_copy_assignment::value
                  ? other.node_alloc_
                  : node_alloc_);
    const_iterator it = other.begin();
    try {
      while (it != other.end()) {
        copy.push_back(*it);
        it++;
      }
    } catch (...) {
      while (copy.size_ != 0) {
        copy.pop_back();
      }
      throw;
    }
    swap(copy);
    return *this;
  }
};
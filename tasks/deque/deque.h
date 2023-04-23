// #pragma once

#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <iostream>

template <typename T>
class Deque {
 private:
  static const size_t BUCKET_SIZE = 32;
  size_t size_ = 0;
  size_t chain_size = 0;
  ssize_t first_index = -1;
  T** chain = nullptr;

  static size_t get_num(ssize_t index) {
    return index / BUCKET_SIZE;
  }
  static size_t get_pos(ssize_t index) {
    return index % BUCKET_SIZE;
  }
  void delete_buckets() {
    for (size_t i = 0; i < chain_size; ++i) {
      delete[] reinterpret_cast<char*>(chain[i]);
    }
  }
  void set_default() {
    delete_buckets();
    delete[] chain;
    size_ = 0;
    chain_size = 0;
    first_index = -1;
  }

 public:
  void swap(Deque& another);

  Deque() = default;
  Deque(const Deque& another);
  Deque(size_t size);
  Deque(size_t size, const T&);
  Deque& operator=(Deque another);

  size_t size() const {
    return size_;
  }

  T& operator[](size_t index);
  const T& operator[](size_t index) const;
  T& at(size_t index);
  const T& at(size_t index) const;

  void push_back(const T& value);
  void pop_back();
  void push_front(const T& value);
  void pop_front();

  template <bool is_const>
  class common_iterator {
   private:
    ssize_t pos = -1;
    T** it_chain = nullptr;

   public:
    using pointer = typename std::conditional<is_const, const T*, T*>::type;
    using reference = typename std::conditional<is_const, const T&, T&>::type;
    using value_type = T;
    using iterator_category = std::random_access_iterator_tag;
    using difference_type = std::ptrdiff_t;

    common_iterator() = default;
    common_iterator(ssize_t pos, T** pt)
        : pos(pos),
          it_chain(pt) {
    }

    common_iterator& operator++();
    common_iterator operator++(int);
    common_iterator& operator--();
    common_iterator operator--(int);

    common_iterator& operator+=(difference_type delta);
    friend common_iterator operator+(common_iterator copy,
                                     difference_type delta) {
      copy += delta;
      return copy;
    }
    common_iterator& operator-=(difference_type delta);
    friend common_iterator operator-(common_iterator copy,
                                     difference_type delta) {
      copy -= delta;
      return copy;
    }
    friend common_iterator operator+(difference_type delta,
                                     common_iterator copy) {
      return copy + delta;
    }

    bool operator<(const common_iterator& another) const {
      return pos < another.pos;
    }
    bool operator<=(const common_iterator& another) const {
      return !(another < *this);
    }
    bool operator>(const common_iterator& another) const {
      return another < *this;
    }
    bool operator>=(const common_iterator& another) const {
      return !(*this < another);
    }
    bool operator==(const common_iterator& another) const {
      return pos == another.pos;
    }
    bool operator!=(const common_iterator& another) const {
      return !(*this == another);
    }

    difference_type operator-(const common_iterator& b) const {
      return pos - b.pos;
    }

    reference operator*() const {
      return *(it_chain[get_num(pos)] + get_pos(pos));
    }

    pointer operator->() const {
      return (it_chain[get_num(pos)] + get_pos(pos));
    }

    operator common_iterator<true>() const {
      return common_iterator<true>(pos, it_chain);
    }
  };

  typedef common_iterator<false> iterator;
  typedef common_iterator<true> const_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

  void insert(iterator it, T value);
  void erase(iterator it);

  iterator begin() {
    return iterator(first_index, chain);
  }
  const_iterator begin() const {
    return const_iterator(first_index, chain);
  }

  iterator end() {
    return iterator(first_index, chain) + size_;
  }
  const_iterator end() const {
    return const_iterator(first_index, chain) + size_;
  }

  const_iterator cbegin() const {
    return const_iterator(first_index, chain);
  }
  const_iterator cend() const {
    return const_iterator(first_index, chain) + size_;
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

  ~Deque() {
    for (size_t i = first_index; i < first_index + size_; ++i) {
      size_t row = get_num(i), index = get_pos(i);
      (chain[row] + index)->~T();
    }
    delete_buckets();
    delete[] chain;
  }
};

template <typename T>
void Deque<T>::swap(Deque<T>& another) {
  std::swap(size_, another.size_);
  std::swap(first_index, another.first_index);
  std::swap(chain_size, another.chain_size);
  std::swap(chain, another.chain);
}

template <typename T>
Deque<T>::Deque(const Deque<T>& another)
    : size_(another.size_),
      chain_size(another.chain_size),
      first_index(another.first_index),
      chain(new T*[chain_size]) {
  size_t i = 0;
  try {
    for (; i < chain_size; ++i) {
      chain[i] = reinterpret_cast<T*>(new char[BUCKET_SIZE * sizeof(T)]);
    }
  } catch (...) {
    for (size_t j = 0; j < i; ++j) {
      delete[] chain[j];
    }
    delete[] chain;
    throw;
  }
  i = first_index;
  try {
    for (; i < first_index + size_; ++i) {
      size_t row = get_num(i), index = get_pos(i);
      new (chain[row] + index) T(another.chain[row][index]);
    }
  } catch (...) {
    for (size_t j = first_index; j < i; ++j) {
      size_t row = get_num(i), index = get_pos(i);
      (chain[row] + index)->~T();
    }
    set_default();
    throw;
  }
}

template <typename T>
Deque<T>::Deque(size_t size)
    : size_(size),
      chain_size((size / BUCKET_SIZE) + 1),
      first_index(0),
      chain(new T*[chain_size]) {
  size_t i = 0;
  try {
    for (; i < chain_size; ++i) {
      chain[i] =
          reinterpret_cast<T*>(new char[BUCKET_SIZE * sizeof(T)]);  // ASK
    }
  } catch (...) {
    for (size_t j = 0; j < i; ++j) {
      delete[] chain[j];
    }
    delete[] chain;
    throw;
  }
  i = first_index;
  try {
    for (; i < first_index + size_; ++i) {
      size_t row = get_num(i), index = get_pos(i);
      new (chain[row] + index) T();
    }
  } catch (...) {
    for (size_t j = first_index; j < i; ++j) {
      size_t row = get_num(i), index = get_pos(i);
      (chain[row] + index)->~T();
    }
    set_default();
    throw;
  }
}

template <typename T>
Deque<T>::Deque(size_t size, const T& value)
    : size_(size),
      chain_size((size / BUCKET_SIZE) + 1),
      first_index(0),
      chain(new T*[chain_size]) {
  size_t i = 0;
  try {
    for (; i < chain_size; ++i) {
      chain[i] =
          reinterpret_cast<T*>(new char[BUCKET_SIZE * sizeof(T)]);  // ASK
    }
  } catch (...) {
    for (size_t j = 0; j < i; ++j) {
      delete[] chain[j];
    }
    delete[] chain;
    throw;
  }
  i = first_index;
  try {
    for (; i < first_index + size_; ++i) {
      size_t row = get_num(i), index = get_pos(i);
      new (chain[row] + index) T(value);
    }
  } catch (...) {
    for (size_t j = first_index; j < i; ++j) {
      size_t row = get_num(i), index = get_pos(i);
      (chain[row] + index)->~T();
    }
    set_default();
    throw;
  }
}

template <typename T>
Deque<T>& Deque<T>::operator=(Deque<T> another) {
  Deque::swap(another);
  return *this;
}

template <typename T>
T& Deque<T>::operator[](size_t index) {
  return chain[get_num(first_index + index)][get_pos(first_index + index)];
}

template <typename T>
const T& Deque<T>::operator[](size_t index) const {
  return chain[get_num(first_index + index)][get_pos(first_index + index)];
}

template <typename T>
T& Deque<T>::at(size_t index) {
  if (index >= size_) {
    throw std::out_of_range("out_of_range");
  }
  return (*this)[index];
}

template <typename T>
const T& Deque<T>::at(size_t index) const {
  if (index >= size_) {
    throw std::out_of_range("out_of_range");
  }
  return (*this)[index];
}

template <typename T>
void Deque<T>::push_back(const T& value) {
  if (size_ == 0) {
    T** newchain = new T*[1];
    try {
      newchain[0] = reinterpret_cast<T*>(new char[BUCKET_SIZE * sizeof(T)]);
    } catch (...) {
      delete[] newchain;
      throw;
    }
    try {
      new (newchain[0]) T(value);
    } catch (...) {
      delete[] reinterpret_cast<char*>(newchain[0]);
      delete[] newchain;
      throw;
    }
    chain = newchain;
    size_ = 1;
    first_index = 0;
    chain_size = 1;
    return;
  }
  if (first_index + size_ == chain_size * BUCKET_SIZE) {
    T** newchain = new T*[chain_size * 2];
    size_t i = 0;
    try {
      for (; i < chain_size * 2; ++i) {
        newchain[i] = reinterpret_cast<T*>(new char[BUCKET_SIZE * sizeof(T)]);
      }
    } catch (...) {
      for (size_t j = 0; j < i; ++j) {
        delete[] reinterpret_cast<char*>(newchain[j]);
      }
      delete[] newchain;
      throw;
    }
    for (size_t j = 0; j < chain_size; ++j) {
      newchain[j] = chain[j];
    }
    try {
      new (newchain[get_num(first_index + size_)] +
           get_pos(first_index + size_)) T(value);
    } catch (...) {
      delete[] newchain;
      throw;
    }
    ++size_;
    delete[] chain;
    chain = newchain;
    chain_size *= 2;
    return;
  }
  new (chain[get_num(first_index + size_)] + get_pos(first_index + size_))
      T(value);
  ++size_;
}

template <typename T>
void Deque<T>::pop_back() {
  size_--;
  (chain[get_num(first_index + size_)] + get_pos(first_index + size_))->~T();
  if (size_ == 0) {
    first_index = -1;
  }
}

template <typename T>
void Deque<T>::push_front(const T& value) {
  if (size_ == 0) {
    T** newchain = new T*[1];
    try {
      newchain[0] = reinterpret_cast<T*>(new char[BUCKET_SIZE * sizeof(T)]);
    } catch (...) {
      delete[] newchain;
      throw;
    }
    try {
      new (newchain[0]) T(value);
    } catch (...) {
      delete[] reinterpret_cast<char*>(newchain[0]);
      delete[] newchain;
      throw;
    }
    chain = newchain;
    size_ = 1;
    first_index = 0;
    chain_size = 1;
    return;
  }
  if (first_index == 0) {
    T** newchain = new T*[chain_size * 2];
    size_t i = 0;
    try {
      for (; i < chain_size * 2; ++i) {
        newchain[i] = reinterpret_cast<T*>(new char[BUCKET_SIZE * sizeof(T)]);
      }
    } catch (...) {
      for (size_t j = 0; j < i; ++j) {
        delete[] reinterpret_cast<char*>(newchain[j]);
      }
      delete[] newchain;
      throw;
    }
    for (size_t j = 0; j < chain_size; ++j) {
      newchain[j + chain_size] = chain[j];
    }
    try {
      new (newchain[chain_size - 1] + BUCKET_SIZE - 1) T(value);
    } catch (...) {
      delete[] newchain;
      throw;
    }
    ++size_;
    delete[] chain;
    chain = newchain;
    first_index = chain_size * BUCKET_SIZE - 1;
    chain_size *= 2;
    return;
  }
  --first_index;
  new (chain[get_num(first_index)] + get_pos(first_index)) T(value);
  ++size_;
}

template <typename T>
void Deque<T>::pop_front() {
  size_--;
  (chain[get_num(first_index)] + get_pos(first_index))->~T();
  first_index++;
  if (size_ == 0) {
    first_index = -1;
  }
}

template <typename T>
template <bool is_const>
typename Deque<T>::template common_iterator<is_const>&
Deque<T>::common_iterator<is_const>::operator++() {
  ++pos;
  return *this;
}

template <typename T>
template <bool is_const>
typename Deque<T>::template common_iterator<is_const>
Deque<T>::common_iterator<is_const>::operator++(int) {
  Deque::common_iterator copy = *this;
  ++pos;
  return copy;
}

template <typename T>
template <bool is_const>
typename Deque<T>::template common_iterator<is_const>&
Deque<T>::common_iterator<is_const>::operator--() {
  --pos;
  return *this;
}

template <typename T>
template <bool is_const>
typename Deque<T>::template common_iterator<is_const>
Deque<T>::common_iterator<is_const>::operator--(int) {
  Deque::common_iterator copy = *this;
  --pos;
  return copy;
}

template <typename T>
template <bool is_const>
typename Deque<T>::template common_iterator<is_const>&
Deque<T>::common_iterator<is_const>::operator+=(difference_type delta) {
  pos += delta;
  return *this;
}

// template<typename T>
// template<bool is_const>
// typename Deque<T>::common_iterator<is_const>
// operator+(Deque<T>::common_iterator<is_const> copy, difference_type delta) {
//   copy += delta;
//   return copy;
// }

template <typename T>
template <bool is_const>
typename Deque<T>::template common_iterator<is_const>&
Deque<T>::common_iterator<is_const>::operator-=(difference_type delta) {
  pos -= delta;
  return *this;
}

// template<typename T>
// template<bool is_const>
// typename Deque<T>::common_iterator<is_const>
// operator-(Deque<T>::common_iterator<is_const> copy, difference_type delta) {
//   copy -= delta;
//   return copy;
// }

template <typename T>
void Deque<T>::insert(Deque<T>::iterator it, T value) {
  while (it != end()) {
    std::swap(*it, value);
    it++;
  }
  push_back(value);
}

template <typename T>
void Deque<T>::erase(Deque<T>::iterator it) {
  while (it + 1 != end()) {
    std::swap(*it, *(it + 1));
    ++it;
  }
  pop_back();
}

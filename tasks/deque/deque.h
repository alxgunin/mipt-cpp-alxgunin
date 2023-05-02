// #pragma once

#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <iostream>

template <typename T>
class Deque {
 private:
  static const size_t bucket_size = 32;
  size_t size_ = 0;
  size_t chain_size_ = 0;
  ssize_t first_index_ = -1;
  T** chain_ = nullptr;

  static size_t get_num(ssize_t index) {
    return index / bucket_size;
  }
  static size_t get_pos(ssize_t index) {
    return index % bucket_size;
  }
  void delete_buckets() {
    for (size_t i = 0; i < chain_size_; ++i) {
      delete[] reinterpret_cast<char*>(chain_[i]);
    }
  }
  void set_default() {
    delete_buckets();
    delete[] chain_;
    size_ = 0;
    chain_size_ = 0;
    first_index_ = -1;
  }

  void realloc(T** newchain, int shift=0) {
    size_t i = 0;
    try {
      for (; i < chain_size_ * 2; ++i) {
        newchain[i] = reinterpret_cast<T*>(new char[bucket_size * sizeof(T)]);
      }
    } catch (...) {
      for (size_t j = 0; j < i; ++j) {
        delete[] reinterpret_cast<char*>(newchain[j]);
      }
      delete[] newchain;
      throw;
    }
    for (size_t j = 0; j < chain_size_; ++j) {
      newchain[j + shift] = chain_[j];
    }
  }

  void null_alloc(const T& value) {
    T** newchain = new T*[1];
    try {
      newchain[0] = reinterpret_cast<T*>(new char[bucket_size * sizeof(T)]);
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
    chain_ = newchain;
    size_ = 1;
    first_index_ = 0;
    chain_size_ = 1; 
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
    ssize_t pos_ = -1;
    T** it_chain_ = nullptr;

   public:
    using pointer = typename std::conditional<is_const, const T*, T*>::type;
    using reference = typename std::conditional<is_const, const T&, T&>::type;
    using value_type = T;
    using iterator_category = std::random_access_iterator_tag;
    using difference_type = std::ptrdiff_t;

    common_iterator() = default;
    common_iterator(ssize_t pos, T** pt)
        : pos_(pos),
          it_chain_(pt) {
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
      return pos_ < another.pos_;
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
      return pos_ == another.pos_;
    }
    bool operator!=(const common_iterator& another) const {
      return !(*this == another);
    }

    difference_type operator-(const common_iterator& b) const {
      return pos_ - b.pos_;
    }

    reference operator*() const {
      return *(it_chain_[get_num(pos_)] + get_pos(pos_));
    }

    pointer operator->() const {
      return (it_chain_[get_num(pos_)] + get_pos(pos_));
    }

    operator common_iterator<true>() const {
      return common_iterator<true>(pos_, it_chain_);
    }
  };

  typedef common_iterator<false> iterator;
  typedef common_iterator<true> const_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

  void insert(iterator it, T value);
  void erase(iterator it);

  iterator begin() {
    return iterator(first_index_, chain_);
  }
  const_iterator begin() const {
    return const_iterator(first_index_, chain_);
  }

  iterator end() {
    return iterator(first_index_, chain_) + size_;
  }
  const_iterator end() const {
    return const_iterator(first_index_, chain_) + size_;
  }

  const_iterator cbegin() const {
    return const_iterator(first_index_, chain_);
  }
  const_iterator cend() const {
    return const_iterator(first_index_, chain_) + size_;
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
    for (size_t i = first_index_; i < first_index_ + size_; ++i) {
      size_t row = get_num(i), index = get_pos(i);
      (chain_[row] + index)->~T();
    }
    delete_buckets();
    delete[] chain_;
  }
};

template <typename T>
void Deque<T>::swap(Deque<T>& another) {
  std::swap(size_, another.size_);
  std::swap(first_index_, another.first_index_);
  std::swap(chain_size_, another.chain_size_);
  std::swap(chain_, another.chain_);
}

template <typename T>
Deque<T>::Deque(const Deque<T>& another)
    : size_(another.size_),
      chain_size_(another.chain_size_),
      first_index_(another.first_index_),
      chain_(new T*[chain_size_]) {
  size_t i = 0;
  try {
    for (; i < chain_size_; ++i) {
      chain_[i] = reinterpret_cast<T*>(new char[bucket_size * sizeof(T)]);
    }
  } catch (...) {
    for (size_t j = 0; j < i; ++j) {
      delete[] chain_[j];
    }
    delete[] chain_;
    throw;
  }
  i = first_index_;
  try {
    for (; i < first_index_ + size_; ++i) {
      size_t row = get_num(i), index = get_pos(i);
      new (chain_[row] + index) T(another.chain_[row][index]);
    }
  } catch (...) {
    for (size_t j = first_index_; j < i; ++j) {
      size_t row = get_num(i), index = get_pos(i);
      (chain_[row] + index)->~T();
    }
    set_default();
    throw;
  }
}

template <typename T>
Deque<T>::Deque(size_t size)
    : size_(size),
      chain_size_((size / bucket_size) + 1),
      first_index_(0),
      chain_(new T*[chain_size_]) {
  size_t i = 0;
  try {
    for (; i < chain_size_; ++i) {
      chain_[i] = new T[bucket_size];
    }
  } catch (...) {
    for (size_t j = 0; j < i; ++j) {
      delete[] chain_[j];
    }
    delete[] chain_;
    throw;
  }
  i = first_index_;
  try {
    for (; i < first_index_ + size_; ++i) {
      size_t row = get_num(i), index = get_pos(i);
      new (chain_[row] + index) T();
    }
  } catch (...) {
    for (size_t j = first_index_; j < i; ++j) {
      size_t row = get_num(i), index = get_pos(i);
      (chain_[row] + index)->~T();
    }
    set_default();
    throw;
  }
}

template <typename T>
Deque<T>::Deque(size_t size, const T& value)
    : size_(size),
      chain_size_((size / bucket_size) + 1),
      first_index_(0),
      chain_(new T*[chain_size_]) {
  size_t i = 0;
  try {
    for (; i < chain_size_; ++i) {
      chain_[i] =
          reinterpret_cast<T*>(new char[bucket_size * sizeof(T)]);  // ASK
    }
  } catch (...) {
    for (size_t j = 0; j < i; ++j) {
      delete[] chain_[j];
    }
    delete[] chain_;
    throw;
  }
  i = first_index_;
  try {
    for (; i < first_index_ + size_; ++i) {
      size_t row = get_num(i), index = get_pos(i);
      new (chain_[row] + index) T(value);
    }
  } catch (...) {
    for (size_t j = first_index_; j < i; ++j) {
      size_t row = get_num(i), index = get_pos(i);
      (chain_[row] + index)->~T();
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
  return chain_[get_num(first_index_ + index)][get_pos(first_index_ + index)];
}

template <typename T>
const T& Deque<T>::operator[](size_t index) const {
  return chain_[get_num(first_index_ + index)][get_pos(first_index_ + index)];
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
    try {
      null_alloc(value);
    } catch(...) {
      throw;
    }
    return;
  }
  if (first_index_ + size_ == chain_size_ * bucket_size) {
    T** newchain = new T*[chain_size_ * 2];
    try {
      realloc(newchain);
    } catch(...) {
      throw;
    }
    try {
      new (newchain[get_num(first_index_ + size_)] +
           get_pos(first_index_ + size_)) T(value);
    } catch (...) {
      delete[] newchain;
      throw;
    }
    ++size_;
    delete[] chain_;
    chain_ = newchain;
    chain_size_ *= 2;
    return;
  }
  new (chain_[get_num(first_index_ + size_)] + get_pos(first_index_ + size_))
      T(value);
  ++size_;
}

template <typename T>
void Deque<T>::pop_back() {
  size_--;
  (chain_[get_num(first_index_ + size_)] + get_pos(first_index_ + size_))->~T();
  if (size_ == 0) {
    first_index_ = -1;
  }
}

template <typename T>
void Deque<T>::push_front(const T& value) {
  if (size_ == 0) {
    try {
      null_alloc(value);
    } catch(...) {
      throw;
    }
    return;
  }
  if (first_index_ == 0) {
    T** newchain = new T*[chain_size_ * 2];
    try {
      realloc(newchain, chain_size_);
    } catch(...) {
      throw;
    }
    try {
      new (newchain[chain_size_ - 1] + bucket_size - 1) T(value);
    } catch (...) {
      delete[] newchain;
      throw;
    }
    ++size_;
    delete[] chain_;
    chain_ = newchain;
    first_index_ = chain_size_ * bucket_size - 1;
    chain_size_ *= 2;
    return;
  }
  --first_index_;
  new (chain_[get_num(first_index_)] + get_pos(first_index_)) T(value);
  ++size_;
}

template <typename T>
void Deque<T>::pop_front() {
  size_--;
  (chain_[get_num(first_index_)] + get_pos(first_index_))->~T();
  first_index_++;
  if (size_ == 0) {
    first_index_ = -1;
  }
}

template <typename T>
template <bool is_const>
typename Deque<T>::template common_iterator<is_const>&
Deque<T>::common_iterator<is_const>::operator++() {
  ++pos_;
  return *this;
}

template <typename T>
template <bool is_const>
typename Deque<T>::template common_iterator<is_const>
Deque<T>::common_iterator<is_const>::operator++(int) {
  Deque::common_iterator copy = *this;
  ++pos_;
  return copy;
}

template <typename T>
template <bool is_const>
typename Deque<T>::template common_iterator<is_const>&
Deque<T>::common_iterator<is_const>::operator--() {
  --pos_;
  return *this;
}

template <typename T>
template <bool is_const>
typename Deque<T>::template common_iterator<is_const>
Deque<T>::common_iterator<is_const>::operator--(int) {
  Deque::common_iterator copy = *this;
  --pos_;
  return copy;
}

template <typename T>
template <bool is_const>
typename Deque<T>::template common_iterator<is_const>&
Deque<T>::common_iterator<is_const>::operator+=(difference_type delta) {
  pos_ += delta;
  return *this;
}

template <typename T>
template <bool is_const>
typename Deque<T>::template common_iterator<is_const>&
Deque<T>::common_iterator<is_const>::operator-=(difference_type delta) {
  pos_ -= delta;
  return *this;
}

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
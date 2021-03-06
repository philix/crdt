// Copyright (C) 2020 Felipe O. Carvalho
#pragma once

#include <string>
#include <utility>
#include <vector>

// STL helpers

template <typename MapType>
typename MapType::mapped_type *lookup(MapType &m, const typename MapType::key_type &k) {
  auto it = m.find(k);
  if (it != m.end()) {
    return &it->second;
  }
  return nullptr;
}

template <typename MapType>
const typename MapType::mapped_type *lookup(const MapType &m, const typename MapType::key_type &k) {
  auto it = m.find(k);
  if (it != m.end()) {
    return &it->second;
  }
  return nullptr;
}

template <typename Container>
bool contains(const Container &s, const typename Container::value_type &v) {
  return s.find(v) != s.end();
}

template <typename Container>
bool linearContains(const Container &s, const typename Container::value_type &v) {
  return std::find(s.begin(), s.end(), v) != s.end();
}

template <typename T>
void hash_combine(std::size_t &seed, const T &val) {
  seed ^= std::hash<T>()(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// Hashers

namespace std {

template <typename A, typename B>
struct hash<std::pair<A, B>> {
  size_t operator()(const std::pair<A, B> &k) const {
    size_t h = 0;
    hash_combine(h, k.first);
    hash_combine(h, k.second);
    return h;
  }
};

template <>
struct hash<vector<string>> {
  size_t operator()(const vector<string> &v) const {
    size_t h = 0;
    for (const auto &s : v) {
      hash_combine(h, s);
    }
    return h;
  }
};

template <typename T>
struct hash<unordered_set<T>> {
  size_t operator()(const unordered_set<T> &v) const {
    hash<T> elem_hasher;
    size_t h = 0;
    for (const auto &elem : v) {
      h ^= elem_hasher(elem);
    }
    return h;
  }
};

template <typename K, typename V>
struct hash<unordered_map<K, V>> {
  size_t operator()(const unordered_map<K, V> &v) const {
    size_t ret = 0;
    for (const auto & [ key, value ] : v) {
      size_t h = 0;
      hash_combine(h, key);
      hash_combine(h, value);
      ret ^= h;
    }
    return ret;
  }
};

}  // namespace std

template <typename T>
struct ValuePrinter {
  void print(const T &) { assert(false && "ValuePrinter not implemented"); }
};

template <>
struct ValuePrinter<std::string> {
  void print(const std::string &value) { printf("'%s'", value.c_str()); }
};

template <typename T>
struct ValuePrinter<std::optional<T>> {
  void print(const std::optional<T> &value) {
    if (value.has_value()) {
      ValuePrinter<T> printer;
      printf("Some(");
      printer.print(*value);
      putchar(')');
    } else {
      printf("None");
    }
  }
};

template <typename T>
struct ValuePrinter<std::unordered_set<T>> {
  void print(const std::unordered_set<T> &value) {
    ValuePrinter<T> printer;
    putchar('{');
    bool first = true;
    for (auto &elem : value) {
      if (!first) {
        printf(", ");
      } else {
        first = false;
      }
      printer.print(elem);
    }
    putchar('}');
  }
};

#ifndef KLEE_SERIALIZE_H
#define KLEE_SERIALIZE_H
#include <iostream>
#include "klee/Internal/Support/SerializableTypes.h"
/*
 * Serialize and Deserialize
 */
namespace klee {
  // char serializer
  template <typename T> // ostream
  inline static void serialize(T &os, const char c) {
    os.put(c);
  }
  template <typename T> // istream
  inline static void deserialize(T &is, char &c) {
    is.get(c);
  }
  template <typename T> // istream
  inline static void skip(T &is, const char &c) {
    is.seekg(sizeof(c), std::ios::cur);
  }
  // std::string serializer
  template <typename T> // ostream
  inline static void serialize(T &os, const std::string &str) {
    std::string::size_type size = str.size();
    os.write(reinterpret_cast<const char*>(&size), sizeof(size));
    os.write(reinterpret_cast<const char*>(str.c_str()), str.size());
  }
  template <typename T> // istream
  inline static void deserialize(T &is, std::string &str) {
    std::string::size_type size;
    is.read(reinterpret_cast<char*>(&size), sizeof(size));
    str.resize(size);
    is.read(&str[0], size);
  }
  template <typename T> // istream
  inline static void skip(T &is, const std::string&) {
    std::string::size_type size;
    is.read(reinterpret_cast<char*>(&size), sizeof(size));
    is.seekg(size, std::ios::cur);
  }
  // uint64_t serializer
  template <typename T> // ostream
  inline static void serialize(T &os, const uint64_t uit) {
    os.write(reinterpret_cast<const char*>(&uit), sizeof(uit));
  }
  template <typename T> // istream
  inline static void deserialize(T &is, uint64_t &uit) {
    is.read(reinterpret_cast<char*>(&uit), sizeof(uit));
  }
  template <typename T> // istream
  inline static void skip(T &is, const uint64_t &uit) {
    is.seekg(sizeof(uit), std::ios::cur);
  }
  template <typename T> // ostream
  inline static void serialize(T &os, const int64_t it) {
    os.write(reinterpret_cast<const char*>(&it), sizeof(it));
  }
  template <typename T> // istream
  inline static void deserialize(T &is, int64_t &it) {
    is.read(reinterpret_cast<char*>(&it), sizeof(it));
  }
  template <typename T> // istream
  inline static void skip(T &is, const int64_t &it) {
    is.seekg(sizeof(it), std::ios::cur);
  }

  // vector recording related serializer
  template <typename T, typename V> // ostream
  inline static void serialize(T &os, const std::vector<V> &v) {
    serialize(os, v.size());
    for (auto i: v) {
      serialize(os, i);
    }
  }
  template <typename T, typename V> // istream
  inline static void deserialize(T &is, std::vector<V> &v) {
    typename std::vector<V>::size_type size;
    deserialize(is, size);
    v.resize(size);
    for (typename std::vector<V>::size_type i=0; i < size; ++i) {
      deserialize(is, v[i]);
    }
  }
  template <typename T, typename V> // istream
  inline static void skip(T &is, const std::vector<V> &v) {
    typename std::vector<V>::size_type size;
    deserialize(is, size);
    for (typename std::vector<V>::size_type i=0; i < size; ++i) {
      skip(is, V());
    }
  }

  // Execution Statistics related serializer
  template <typename T> // ostream
  inline static void serialize(T &os, const struct ExecutionStats &exstats) {
    serialize(os, exstats.llvm_inst_str);
    serialize(os, exstats.file_loc);
    serialize(os, exstats.instructions_cnt);
    
    serialize(os, exstats.queryCost_us);
    serialize(os, exstats.queryCost_increment_us);
  }
  template <typename T> // istream
  inline static void deserialize(T &is, struct ExecutionStats &exstats) {
    deserialize(is, exstats.llvm_inst_str);
    deserialize(is, exstats.file_loc);
    deserialize(is, exstats.instructions_cnt);
    
    deserialize(is, exstats.queryCost_us);
    deserialize(is, exstats.queryCost_increment_us);
  }
  template <typename T> // istream
  inline static void skip(T &is, const struct ExecutionStats &exstats) {
    skip(is, exstats.llvm_inst_str);
    skip(is, exstats.file_loc);
    skip(is, exstats.instructions_cnt);
    
    skip(is, exstats.queryCost_us);
    skip(is, exstats.queryCost_increment_us);
  }

  // One string one instruction counter Statistics related serializer
  template <typename T> // ostream
  inline static void serialize(T &os, const struct StringInstStats &stris) {
    serialize(os, stris.instcnt);
    serialize(os, stris.str);
  }
  template <typename T> // istream
  inline static void deserialize(T &is, struct StringInstStats &stris) {
    deserialize(is, stris.instcnt);
    deserialize(is, stris.str);
  }
  template <typename T> // istream
  inline static void skip(T &is, const struct StringInstStats &stris) {
    skip(is, stris.instcnt);
    skip(is, stris.str);
  }

  // Path recording related serializer
  template <typename T> // ostream
  inline static void serialize(T &os, const struct PathEntry &pe) {
    os.write(reinterpret_cast<const char*>(&pe),
        sizeof(PathEntryBase));
    if (pe.t == PathEntry::FORKREC) {
      for (auto i: pe.Kids) {
        serialize(os, i);
      }
    }
  }

  template <typename T> // istream
  inline static void deserialize(T &is, struct PathEntry &pe) {
    is.read(reinterpret_cast<char*>(&pe),
        sizeof(PathEntryBase));
    if (pe.t == PathEntry::FORKREC) {
      pe.Kids.resize(pe.body.rec.numKids);
      for (PathEntry::numKids_t i=0; i < pe.body.rec.numKids; ++i) {
        deserialize(is, pe.Kids[i]);
      }
    }
  }

  template <typename T> // istream
  inline static void skip(T &is, const struct PathEntry &pe) {
    struct PathEntry _pe;
    is.read(reinterpret_cast<char*>(&_pe),
        sizeof(PathEntryBase));
    for (PathEntry::numKids_t i=0; i < _pe.body.rec.numKids; ++i) {
      skip(is, PathEntry::ConstantType());
    }
  }
}
#endif

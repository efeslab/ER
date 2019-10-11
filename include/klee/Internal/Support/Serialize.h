#ifndef KLEE_SERIALIZE_H
#define KLEE_SERIALIZE_H
#include <iostream>
#include <type_traits>
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
  // simple type serializer
#define BUILTIN_SERIALIZATION_CHECK static_assert(\
    std::is_same<V, uint64_t>::value ||\
    std::is_same<V, int64_t>::value ||\
    std::is_same<V, uint32_t>::value ||\
    std::is_same<V, int32_t>::value, "Type not supported")
  template <typename T, typename V> // ostream
  inline static void serialize(T &os, const V uit) {
    BUILTIN_SERIALIZATION_CHECK;
    os.write(reinterpret_cast<const char*>(&uit), sizeof(uit));
  }
  template <typename T, typename V> // istream
  inline static void deserialize(T &is, V &uit) {
    BUILTIN_SERIALIZATION_CHECK;
    is.read(reinterpret_cast<char*>(&uit), sizeof(uit));
  }
  template <typename T, typename V> // istream
  inline static void skip(T &is, const V &uit) {
    BUILTIN_SERIALIZATION_CHECK;
    is.seekg(sizeof(uit), std::ios::cur);
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
    os.write(reinterpret_cast<const char*>(&pe), sizeof(pe));
  }
  template <typename T> // istream
  inline static void deserialize(T &is, struct PathEntry &pe) {
    is.read(reinterpret_cast<char*>(&pe), sizeof(pe));
  }
  template <typename T> // istream
  inline static void skip(T &is, const struct PathEntry &pe) {
    is.seekg(sizeof(pe), std::ios::cur);
  }
}
#endif

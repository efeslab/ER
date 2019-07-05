#ifndef KLEE_SERIALIZE_H
#define KLEE_SERIALIZE_H
#include <iostream>
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

  // Execution Statistics related serializer
  struct ExecutionStats {
    std::string llvm_inst_str;
    std::string file_loc;
    uint64_t instructions_cnt;
    
    int64_t queryCost_us;
    int64_t queryCost_increment_us;
  };
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

  // Constraint Statistics related serializer
  struct ConstraintStats {
    uint64_t instructions_cnt;
    std::string new_constraint;
  };
  template <typename T> // ostream
  inline static void serialize(T &os, const struct ConstraintStats &cs) {
    serialize(os, cs.instructions_cnt);
    serialize(os, cs.new_constraint);
  }
  template <typename T> // istream
  inline static void deserialize(T &is, struct ConstraintStats &cs) {
    deserialize(is, cs.instructions_cnt);
    deserialize(is, cs.new_constraint);
  }
  template <typename T> // istream
  inline static void skip(T &is, const struct ConstraintStats &cs) {
    skip(is, cs.instructions_cnt);
    skip(is, cs.new_constraint);
  }

  // Path recording related serializer
  struct PathEntry {
    enum PathEntry_t: unsigned char {FORK, SWITCH, INDIRECTBR};
    typedef uint8_t switchIndex_t;
    typedef uint8_t indirectbrIndex_t;
    PathEntry_t t;
    union {
      bool br;
      // Here assume the number of branches won't exceed 256
      switchIndex_t switchIndex;
      indirectbrIndex_t indirectbrIndex;
    } body;
  };
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

#ifndef KLEE_SERIALIZE_H
#define KLEE_SERIALIZE_H
#include <iostream>
/*
 * Serialize and Deserialize
 */
namespace klee {
  // char serializer
  inline static void serialize(std::ostream &os, const char c) {
    os.put(c);
  }
  inline static void deserialize(std::istream &is, char &c) {
    is.get(c);
  }
  inline static void skip(std::istream &is, const char &c) {
    is.seekg(sizeof(c), std::ios::cur);
  }
  // std::string serializer
  inline static void serialize(std::ostream &os, const std::string &str) {
    std::string::size_type size = str.size();
    os.write(reinterpret_cast<const char*>(&size), sizeof(size));
    os.write(reinterpret_cast<const char*>(str.c_str()), str.size());
  }
  inline static void deserialize(std::istream &is, std::string &str) {
    std::string::size_type size;
    is.read(reinterpret_cast<char*>(&size), sizeof(size));
    str.resize(size);
    is.read(&str[0], size);
  }
  inline static void skip(std::istream &is, const std::string&) {
    std::string::size_type size;
    is.read(reinterpret_cast<char*>(&size), sizeof(size));
    is.seekg(size, std::ios::cur);
  }
  // uint64_t serializer
  inline static void serialize(std::ostream &os, const uint64_t uit) {
    os.write(reinterpret_cast<const char*>(&uit), sizeof(uit));
  }
  inline static void deserialize(std::istream &is, uint64_t &uit) {
    is.read(reinterpret_cast<char*>(&uit), sizeof(uit));
  }
  inline static void skip(std::istream &is, const uint64_t &uit) {
    is.seekg(sizeof(uit), std::ios::cur);
  }
  inline static void serialize(std::ostream &os, const int64_t uit) {
    os.write(reinterpret_cast<const char*>(&uit), sizeof(uit));
  }
  inline static void deserialize(std::istream &is, int64_t &uit) {
    is.read(reinterpret_cast<char*>(&uit), sizeof(uit));
  }
  inline static void skip(std::istream &is, const int64_t &uit) {
    is.seekg(sizeof(uit), std::ios::cur);
  }

  struct ExecutionStats {
    std::string llvm_inst_str;
    std::string file_loc;
    uint64_t instructions_cnt;
    
    std::string constraint;
    int64_t queryCost_us;
    std::string constraint_increment;
    int64_t queryCost_increment_us;
    
    uint64_t trueBranches, falseBranches;
  };
  inline static void serialize(std::ostream &os, const struct ExecutionStats &exstats) {
    serialize(os, exstats.llvm_inst_str);
    serialize(os, exstats.file_loc);
    serialize(os, exstats.instructions_cnt);
    
    serialize(os, exstats.constraint);
    serialize(os, exstats.constraint_increment);
    serialize(os, exstats.queryCost_us);
    serialize(os, exstats.queryCost_increment_us);
    
    serialize(os, exstats.trueBranches);
    serialize(os, exstats.falseBranches);
  }
  inline static void deserialize(std::istream &is, struct ExecutionStats &exstats) {
    deserialize(is, exstats.llvm_inst_str);
    deserialize(is, exstats.file_loc);
    deserialize(is, exstats.instructions_cnt);
    
    deserialize(is, exstats.constraint);
    deserialize(is, exstats.constraint_increment);
    deserialize(is, exstats.queryCost_us);
    deserialize(is, exstats.queryCost_increment_us);
    
    deserialize(is, exstats.trueBranches);
    deserialize(is, exstats.falseBranches);
  }
  inline static void skip(std::istream &is, const struct ExecutionStats &exstats) {
    skip(is, exstats.llvm_inst_str);
    skip(is, exstats.file_loc);
    skip(is, exstats.instructions_cnt);
    
    skip(is, exstats.constraint);
    skip(is, exstats.constraint_increment);
    skip(is, exstats.queryCost_us);
    skip(is, exstats.queryCost_increment_us);
    
    skip(is, exstats.trueBranches);
    skip(is, exstats.falseBranches);
  }
}
#endif

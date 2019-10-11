#ifndef KLEE_SERIALIZABLETYPES_H
#define KLEE_SERIALIZABLETYPES_H
#include <string>
#include <vector>
namespace klee {
  struct ExecutionStats {
    std::string llvm_inst_str;
    std::string file_loc;
    uint64_t instructions_cnt;
    
    int64_t queryCost_us;
    int64_t queryCost_increment_us;
  };
  // One string one instruction counter Statistics
  // Used by Constraint Stats and Stack Stats
  struct StringInstStats {
    uint64_t instcnt;
    std::string str;
  };
  // The unit of path recording
  struct PathEntry {
    enum PathEntry_t: unsigned char {FORK, SWITCH, INDIRECTBR};
    typedef uint16_t switchIndex_t;
    typedef uint8_t indirectbrIndex_t;
    typedef uint8_t numKids_t;
    PathEntry_t t;
    union {
      bool br;
      // Here assume the number of branches won't exceed 256
      switchIndex_t switchIndex;
      indirectbrIndex_t indirectbrIndex;
    } body;
  };
}
#endif

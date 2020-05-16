#ifndef KLEE_REFHASHMAP_H
#define KLEE_REFHASHMAP_H

#include "klee/util/Ref.h"

#include <unordered_map>
#include <unordered_set>

namespace klee {

namespace util {
template <typename T> struct RefHash {
  unsigned operator()(const ref<T> &e) const { return e->hash(); }
};

template <typename T> struct RefCmp {
  bool operator()(const ref<T> &a, const ref<T> &b) const { return a == b; }
};
} // namespace util

template <typename Tkey, typename Tval>
using RefHashMap =
    std::unordered_map<ref<Tkey>, Tval, klee::util::RefHash<Tkey>,
                       klee::util::RefCmp<Tkey>>;

template <typename T>
using RefHashSet =
    std::unordered_set<ref<T>, klee::util::RefHash<T>, klee::util::RefCmp<T>>;
} // namespace klee
#endif // KLEE_REFHASHMAP_H

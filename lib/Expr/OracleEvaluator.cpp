#include "klee/util/OracleEvaluator.h"
#include "klee/Internal/Support/ErrorHandling.h"
#include "klee/SolverImpl.h"
#include "klee/OptionCategories.h"

using namespace llvm;

namespace klee {
  cl::opt<std::string> OracleKTest( "oracle-KTest", cl::init(""),
      cl::desc(""), cl::cat(HASECat));
} // namespace klee

ref<Expr> OracleEvaluator::getInitialValue(const Array &array, unsigned index) {
    // it.first == array name, it.second == index at ktest->objects
    arrayname2idx_ty::const_iterator it = arrayname2idx.find(array.name);
    if (it == arrayname2idx.end()) {
        klee_message("Cannot find symbolic array %s in KTest", array.name.c_str());
        abort();
    }
    KTestObject &kobj = ktest->objects[it->second];
    if (index >= kobj.numBytes) {
        klee_message("Array %s requested index (%u) >= numBytes (%u)",
                array.name.c_str(), index, kobj.numBytes);
        abort();
    }
    assert(array.getRange() == Expr::Int8 && "Current implementation only supports byte array");
    return ConstantExpr::alloc(kobj.bytes[index], array.getRange());
}
OracleEvaluator::OracleEvaluator(std::string KTestPath, bool silent) {
    ktest = kTest_fromFile(KTestPath.c_str());
    assert(ktest && "Open KTestFile error");
    for (unsigned int i=0; i < ktest->numObjects; ++i) {
        if (!silent) {
            klee_message("Loading %s from OracleKTest", ktest->objects[i].name);
        }
        arrayname2idx[ktest->objects[i].name] = i;
    }
}

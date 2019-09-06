#include "OracleEvaluator.h"
#include "klee/Internal/Support/ErrorHandling.h"
#include "klee/SolverImpl.h"

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
OracleEvaluator::OracleEvaluator(std::string KTestPath) {
    ktest = kTest_fromFile(KTestPath.c_str());
    assert(ktest && "Open KTestFile error");
    for (unsigned int i=0; i < ktest->numObjects; ++i) {
        klee_message("Loading %s from OracleKTest", ktest->objects[i].name);
        arrayname2idx[ktest->objects[i].name] = i;
    }
}
/*
class OracleSolver : public SolverImpl {
    Solver *solver;
    OracleEvaluator oracle_eval;
    public:
    OracleSolver(Solver *_solver, std::string KTestPath): solver(_solver), ktest_ev(KTestPath) {}
    ~OracleSolver() {
        kTest_free(ktest);
    }
    bool computeTruth(const Query&, bool &isValid);
    bool computeValidity(const Query&, Solver::Validity &result);
    bool computeValue(const Query&, ref<Expr> &result);
    bool computeInitialValues(const Query&,
            const std::vector<const Array*> &objects,
            std::vector<std::vector<unsigned char> > &values,
            bool &hasSolution);
    SolverRunStatus getOperationStatusCode();
    char *getConstraintLog(const Query&);
    void setCoreSolverTimeout(time::Span timeout) {};
}

bool OracleSolver::computeTruth(const Query &query, bool &isValid) {
    ref<Expr> result = ktest_ev.visit(query.expr);
}
*/

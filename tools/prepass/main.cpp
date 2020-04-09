#include "klee/Config/Version.h"
#include "klee/Internal/Module/Cell.h"
#include "klee/Internal/Module/InstructionInfoTable.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/Internal/Module/KModule.h"
#include "klee/Internal/Support/Debug.h"
#include "klee/Internal/Support/ErrorHandling.h"
#include "klee/Internal/Support/ModuleUtil.h"
#include "klee/Interpreter.h"
#include "klee/OptionCategories.h"
#include "klee/Internal/Support/PrintVersion.h"

#if LLVM_VERSION_CODE >= LLVM_VERSION(4, 0)
#include "llvm/Bitcode/BitcodeWriter.h"
#else
#include "llvm/Bitcode/ReaderWriter.h"
#endif
#include "llvm/IR/CallSite.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/Signals.h"
#include "llvm/Transforms/Scalar.h"
#if LLVM_VERSION_CODE >= LLVM_VERSION(8, 0)
#include "llvm/Transforms/Scalar/Scalarizer.h"
#endif
#include "llvm/Transforms/Utils/Cloning.h"
#if LLVM_VERSION_CODE >= LLVM_VERSION(7, 0)
#include "llvm/Transforms/Utils.h"
#endif
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/APInt.h"

#include <iostream>
#include <fstream>

using namespace llvm;
using namespace klee;

namespace klee {
cl::OptionCategory HASEPrePassCat("prepass");
}

static cl::opt<std::string> InputFile(
    cl::desc("input_bitcode"),
    cl::Positional,
    cl::Required);
static cl::opt<std::string> OutputFile(
    cl::desc("output_bitcode"),
    cl::Positional,
    cl::Required);

llvm::cl::opt<bool> RemoveFP(
    "remove-fp",
    llvm::cl::desc("Remove floating point OPs."),
    llvm::cl::init(true),
    llvm::cl::cat(klee::HASEPrePassCat));

llvm::cl::opt<bool> AssignID(
    "assign-id",
    llvm::cl::desc("Assign a human-readable ID to each LLVM IR instructions."
                   " (default=true)"),
    llvm::cl::init(true),
    llvm::cl::cat(klee::HASEPrePassCat));

llvm::cl::opt<bool> InsertPTWrite(
    "insert-ptwrite",
    llvm::cl::desc("Insert PTWrite instructions to specific places."
                   " (default=false)"),
    llvm::cl::init(false),
    llvm::cl::cat(klee::HASEPrePassCat));
llvm::cl::opt<std::string> PTWriteCFG(
    "ptwrite-cfg",
    llvm::cl::desc("Which LLVM IR instruction should be recorded. "
                "One instruction unique ID per line."),
    llvm::cl::init(""),
    llvm::cl::cat(klee::HASEPrePassCat));

llvm::cl::opt<bool> InsertTag(
    "insert-tag",
    llvm::cl::desc("Insert tags to specific places."
                   " (default=false)"),
    llvm::cl::init(false),
    llvm::cl::cat(klee::HASEPrePassCat));
llvm::cl::opt<std::string> TagCFG(
    "tag-cfg",
    llvm::cl::desc("Which LLVM IR instruction should be recorded. "
                "One instruction unique ID per line."),
    llvm::cl::init(""),
    llvm::cl::cat(klee::HASEPrePassCat));

static void HideOptions() {
    StringMap<cl::Option *> &map = cl::getRegisteredOptions();
    for (auto &elem : map) {
        if (elem.second->Category != &klee::HASEPrePassCat) {
            elem.second->setHiddenFlag(cl::Hidden);
        }
    }
}

int main(int argc, char **argv) {
    sys::PrintStackTraceOnErrorSignal(argv[0]);
    HideOptions();
    cl::SetVersionPrinter(klee::printVersion);
    cl::ParseCommandLineOptions(argc, argv);

    std::vector<std::unique_ptr<llvm::Module>> loadedModules;
    LLVMContext ctx;

    std::string errorMsg;
    if (!klee::loadFile(InputFile, ctx, loadedModules, errorMsg)) {
      klee_error("error loading program '%s': %s", InputFile.c_str(),
          errorMsg.c_str());
    }
    assert(loadedModules.size() == 1);
    Module *M = loadedModules[0].get();

    std::error_code EC;
    raw_fd_ostream fs(OutputFile, EC, llvm::sys::fs::F_None);
    if (EC) {
      klee_error("error opening output file");
    }

    if (RemoveFP) {
      KModule::removeFabs(M);
    }

    if (AssignID) {
      std::string prefix = "";
      KModule::assignID(M, prefix);
    }

    if (InsertPTWrite) {
      if (PTWriteCFG!="")
        KModule::addPTWrite(M, PTWriteCFG);
    }

    if (InsertTag) {
      if (TagCFG!="")
        KModule::addTag(M, TagCFG);
    }

#if LLVM_VERSION_CODE >= LLVM_VERSION(7, 0)
    WriteBitcodeToFile(*M, fs);
#else
    WriteBitcodeToFile(M, fs);
#endif
    fs.close();
    llvm::outs() << "Module saved to " << OutputFile << "\n";

    M = nullptr;
    loadedModules.clear();

    return 0;
}

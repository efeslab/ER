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
    cl::Optional);

llvm::cl::opt<bool> RemoveFP(
    "remove-fp",
    llvm::cl::desc("Remove floating point OPs."),
    llvm::cl::init(true),
    llvm::cl::cat(klee::HASEPrePassCat));

llvm::cl::opt<bool> AssignID(
    "assign-id",
    llvm::cl::desc("Assign a human-readable ID to each LLVM IR instruction."
                   " (default=true)"),
    llvm::cl::init(true),
    llvm::cl::cat(klee::HASEPrePassCat));

llvm::cl::opt<int> SelectRandom("select-random",
                                llvm::cl::desc("target for select-random"),
                                llvm::cl::init(0),
                                llvm::cl::cat(klee::HASEPrePassCat));
llvm::cl::opt<bool> RemoveID(
    "remove-id",
    llvm::cl::desc("Remove the human-readable ID from LLVM IR instruction."
                   " (default=false)"),
    llvm::cl::init(false), llvm::cl::cat(klee::HASEPrePassCat));

llvm::cl::opt<bool> InsertPTWrite(
    "insert-ptwrite",
    llvm::cl::desc("Insert PTWrite instructions to specific places."
                   " (default=false)"),
    llvm::cl::init(false), llvm::cl::cat(klee::HASEPrePassCat));
llvm::cl::opt<std::string>
    PTWriteInstCFG("ptwrite-cfg",
               llvm::cl::desc("Which LLVM IR instruction should be recorded. "
                              "One instruction unique ID per line."),
               llvm::cl::init(""), llvm::cl::cat(klee::HASEPrePassCat));
llvm::cl::opt<std::string> PTWriteWholeFunCFG(
    "ptwrite-func-cfg",
    llvm::cl::desc(
        "A list of function names, whose entire body should be instrumented"),
    llvm::cl::init(""), llvm::cl::cat(klee::HASEPrePassCat));
llvm::cl::opt<bool>
    InsertTag("insert-tag",
              llvm::cl::desc("Insert tags to specific places. (default=false)"),
              llvm::cl::init(false), llvm::cl::cat(klee::HASEPrePassCat));
llvm::cl::opt<bool> InsertTagLoc(
    "insert-tag-loc",
    llvm::cl::desc("Insert tags to specific places based on debug info."
                   " (default=false)"),
    llvm::cl::init(false), llvm::cl::cat(klee::HASEPrePassCat));
llvm::cl::opt<std::string>
    TagCFG("tag-cfg",
           llvm::cl::desc("Which LLVM IR instruction should be recorded. "
                          "One instruction unique ID per line."),
           llvm::cl::init(""), llvm::cl::cat(klee::HASEPrePassCat));

static llvm::cl::extrahelp extrahelp(
    "\n"
    "NOTE: You need an input bitcode containing frequency info to see "
    "the recording statistics (how many records, how many bytes, etc.)\n");

llvm::cl::opt<bool> AssignDebugIR(
    "debugir",
    llvm::cl::desc("Assign DebugIR"),
    llvm::cl::init(false),
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

    if (SelectRandom > 0) {
      KModule::selectRandInst(M, SelectRandom);
      return 0;
    }

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
      if (!PTWriteInstCFG.empty() || !PTWriteWholeFunCFG.empty())
        KModule::addPTWrite(M, PTWriteInstCFG, PTWriteWholeFunCFG);
    }

    if (InsertTagLoc) {
      if (TagCFG!="")
        KModule::addTag(M, TagCFG, true);
    }
    else if (InsertTag) {
      if (TagCFG!="")
        KModule::addTag(M, TagCFG, false);
    }

    /* We need to have this pass because the ID will hurt the performance of the compiled binary */
    if (RemoveID) {
      KModule::removeID(M);
    }


    if (AssignDebugIR) {
      SmallString<128> tempdir;
      llvm::sys::path::system_temp_directory(true, tempdir);
      std::string directory = tempdir.str().str();
      std::string filename = M->getName().str();
      KModule::assignDebugIR(M, directory, filename);
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

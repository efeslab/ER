#include "klee/Internal/ADT/KTest.h"
#include "klee/Internal/Support/PrintVersion.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Signals.h"

#include <iostream>
#include <fstream>
#include <cstring>
#include <sstream>
#include <unordered_map>

using namespace llvm;
using namespace klee;

static cl::opt<std::string> InputKTestFile(
    cl::desc("input_ktest_file"),
    cl::Positional,
    cl::Required);

static cl::opt<std::string> InputTemplateFile(
    cl::desc("input_template_file"),
    cl::Positional,
    cl::Required);

static cl::opt<std::string> OutputFile(
    cl::desc("output_concretize_cfg"),
    cl::Positional,
    cl::Required);

int main(int argc, char **argv) {
  sys::PrintStackTraceOnErrorSignal(argv[0]);
  cl::SetVersionPrinter(klee::printVersion);
  StringMap<cl::Option *> &map = cl::getRegisteredOptions();
  for (auto &elem : map) {
    auto &categories = elem.second->Categories;
    if (std::find(categories.begin(), categories.end(), &llvm::cl::GeneralCategory) != categories.end()) {
            elem.second->setHiddenFlag(cl::Hidden);
    }
  }
  cl::ParseCommandLineOptions(argc, argv);

  std::ifstream inTemp(InputTemplateFile);
  assert(inTemp.good() && "Cannot open input template file");
  std::ofstream outFile(OutputFile);
  assert(outFile.good() && "Cannot open output file");

  KTest *ktest = kTest_fromFile(InputKTestFile.c_str());
  if (!ktest) {
    llvm::errs() << "Invalid KTest file at " << InputKTestFile << "\n";
    abort();
  }

  while (1) {
    std::istringstream line;
    std::string linestr, object;
    unsigned offset;

    std::getline(inTemp, linestr);
    if (!inTemp.good()) {
      break;
    }
    else {
      line.str(linestr);
      line >> object >> offset;
    }

    for (unsigned i = 0; i < ktest->numObjects; i++) {
      std::string name(ktest->objects[i].name);
      if (name == object) {
        outFile << name << " " << offset << " " << (int)ktest->objects[i].bytes[offset] << "\n";
      }
    }
  }

  inTemp.close();
  outFile.close();

  return 0;
}




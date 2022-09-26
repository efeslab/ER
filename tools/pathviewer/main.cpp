#include "klee/Internal/Support/PrintVersion.h"
#include "klee/Internal/Support/Serialize.h"

#include "llvm/ADT/APInt.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Signals.h"

#include <iostream>
#include <fstream>

using namespace llvm;
using namespace klee;

cl::OptionCategory PathViewerCmdOpt("pathviewer", "pathviewer commandline options");
enum ToolActions { GetInfo, Dump };
static cl::opt<ToolActions> ToolAction(
    cl::desc("Tool actions:"), cl::init(GetInfo),
    cl::values(
      clEnumValN(GetInfo, "info", "Get summarized info of a recorded execution path (default)"),
      clEnumValN(Dump, "dump", "Dump a recorded exectuion path (binary) to text format")
      ),
    cl::cat(PathViewerCmdOpt)
    );
static cl::opt<bool> DumpDataRec(
    "dumpdata",
    cl::desc("Also dump DATAREC entry. By default it is false, since you may "
      "compare path from recording phase (with data recording) with path from "
      "replay phase (without data recording). "
      "If this is true, \"*.path_datarec\" is needed."),
    cl::init(false), cl::cat(PathViewerCmdOpt)
    );
static cl::opt<std::string> PathFile(cl::desc("*.path"),
    cl::Positional, cl::Required, cl::cat(PathViewerCmdOpt));

static void HideOptions(cl::OptionCategory &Category) {
    StringMap<cl::Option *> &map = cl::getRegisteredOptions();
    for (auto &elem : map) {
	auto &categories = elem.second->Categories;
        if (std::find(categories.begin(), categories.end(), &Category) != categories.end()) {
            elem.second->setHiddenFlag(cl::Hidden);
        }
    }
}

static const char *PathEntry_t_str[] = {
  "FORK", "SWITCH_EXPIDX", "SWITCH_BBIDX", "INDIRECTBR", "DATAREC", "SCHEDULE"
};

static std::ostream &operator<<(std::ostream &os, PathEntry pe) {
  switch (pe.t) {
    case PathEntry::FORK:
      os << "FORK " << pe.body.br;
      break;
    case PathEntry::SWITCH_EXPIDX:
      os << "SWITCH_EXPIDX " << pe.body.switchIndex;
      break;
    case PathEntry::SWITCH_BBIDX:
      os << "SWITCH_BBIDX " << pe.body.switchIndex;
      break;
    case PathEntry::INDIRECTBR:
      os << "INDIRECTBR_IDX " << pe.body.indirectbrIndex;
      break;
    case PathEntry::DATAREC:
      // do not print anything about DATAREC without DumpDataRec
      break;
    case PathEntry::SCHEDULE:
      os << "SCHEDULE TGTID " << pe.body.tgtid;
      break;
    default:
      os << "Unknown PathEntry";
  }
  return os;
}

static std::ostream &operator<<(std::ostream &os, std::pair<PathEntry, DataRecEntry> &&entry_pair) {
  PathEntry &pe = entry_pair.first;
  DataRecEntry &drec = entry_pair.second;
  if (pe.t == PathEntry::DATAREC) {
    APInt var((unsigned int)(pe.body.drec.width), drec.data);
    os << "DATAREC w" << std::dec << (unsigned int)(pe.body.drec.width)
      << " (" << drec.instUniqueID << "): 0x" << std::hex << var.getLimitedValue();
  }
  else {
    os << "only DATAREC can be printed with recorded data";
  }
  return os;
}

int main(int argc, char **argv) {
    sys::PrintStackTraceOnErrorSignal(argv[0]);
    HideOptions(llvm::cl::GeneralCategory);
    cl::SetVersionPrinter(klee::printVersion);
    cl::ParseCommandLineOptions(argc, argv);

    // load given *.path
    std::vector<PathEntry> pathentries;
    std::ifstream fpath(PathFile);
    assert(fpath.good() && "Cannot open input .path file");
    while (fpath.good()) {
      PathEntry pe;
      deserialize(fpath, pe);
      fpath.peek();
      pathentries.push_back(pe);
    }
    fpath.close();

    // load associated *.path_datarec if necessary
    std::vector<DataRecEntry> dataentries;
    if (DumpDataRec) {
      std::ifstream fdpath(PathFile + "_datarec");
      assert(fdpath.good() && "Cannot open associated .path_datarec file");
      while (fdpath.good()) {
        DataRecEntry drec;
        deserialize(fdpath, drec);
        fdpath.peek();
        dataentries.push_back(drec);
      }
      fdpath.close();
    }

    uint32_t type_cnt[PathEntry::NUM_PATHENTRY_T] = {0};
    uint32_t datarec_bytes = 0;
    switch (ToolAction) {
      case Dump:
        {
          auto drec_it = dataentries.begin();
          for (auto &pe: pathentries) {
            if (DumpDataRec && (pe.t == PathEntry::DATAREC)) {
              assert(drec_it != dataentries.end() && ".path_datarec exhausts too early");
              std::cout << std::make_pair(pe, *drec_it) << '\n';
              ++drec_it;
            }
            else if (DumpDataRec || (pe.t != PathEntry::DATAREC)){
              // do not print an DATAREC entry here without DumpDataRec
              std::cout << pe << '\n';
            }
          }
        }
        break;
      case GetInfo:
        for (auto &pe: pathentries) {
          if (pe.t < PathEntry::NUM_PATHENTRY_T) {
            if (pe.t == PathEntry::DATAREC) {
              datarec_bytes += pe.body.drec.width / 8;
            }
            ++type_cnt[pe.t];
          }
        }
        for (unsigned int i=0; i < PathEntry::NUM_PATHENTRY_T; ++i) {
          std::cout << PathEntry_t_str[i] << ": " << type_cnt[i] << '\n';
        }
        std::cout << "DataRecBytes: " << datarec_bytes << std::endl;
        break;
      default:
        ;
    }
}

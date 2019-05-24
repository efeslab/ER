//===-- TreeStream.cpp ----------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "TreeStreamWriter"
#include "klee/Internal/ADT/TreeStream.h"

#include "klee/Internal/Support/Debug.h"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <iterator>
#include <map>

#include "llvm/Support/raw_ostream.h"
#include <string.h>

using namespace klee;

///

TreeStreamWriter::TreeStreamWriter(const std::string &_path) 
  : lastID(0),
    lastLen(0),
    lastLen_off(std::iostream::pos_type(0)),
    isWritten(false),
    lastLen_dirty(false),
    path(_path),
    output(new std::ofstream(path.c_str(), 
                             std::ios::out | std::ios::binary)),
    ids(1) {
  if (!output->good()) {
    delete output;
    output = 0;
  }
}

TreeStreamWriter::~TreeStreamWriter() {
  flush();
  delete output;
}

bool TreeStreamWriter::good() {
  return !!output;
}

TreeOStream TreeStreamWriter::open() {
  return open(TreeOStream(*this, 0));
}

TreeOStream TreeStreamWriter::open(const TreeOStream &os) {
  assert(output && os.writer==this);
  flush_lastLen();
  isWritten = false;
  unsigned id = ids++;
  output->write(reinterpret_cast<const char*>(&os.id), 4);
  unsigned tag = id | (1<<31);
  output->write(reinterpret_cast<const char*>(&tag), 4);
  return TreeOStream(*this, id);
}

void TreeStreamWriter::flush_lastLen() {
  if (lastLen_dirty) {
    std::iostream::pos_type save_pos = output->tellp();
    output->seekp(lastLen_off);
    output->write(reinterpret_cast<const char*>(&lastLen), sizeof(lastLen));
    output->seekp(save_pos);
    lastLen_dirty = false;
  }
}
void TreeStreamWriter::write_metadata(TreeOStream &os) {
  if (isWritten) {
    flush_lastLen();
  }
  output->write(reinterpret_cast<const char*>(&os.id), sizeof(os.id));
  lastLen_off = output->tellp();
  lastLen = 0;
  lastID = os.id;
  output->write(reinterpret_cast<const char*>(&lastLen), sizeof(lastLen));
  isWritten = true;
}

template<typename T>
void TreeStreamWriter::write(TreeOStream &os, const T &entry) {
  if (!isWritten || 
     (isWritten && os.id != lastID)) {
    write_metadata(os);
  }
  serialize(*output, entry);
  ++lastLen;
  lastLen_dirty = true;
}

void TreeStreamWriter::flush() {
  flush_lastLen();
  output->flush();
}

template <typename T>
void TreeStreamWriter::readStream(TreeStreamID streamID,
                                  std::vector<T> &out) {
  assert(streamID>0 && streamID<ids);
  flush();
  
  std::ifstream is(path.c_str(),
                   std::ios::in | std::ios::binary);
  assert(is.good());
  KLEE_DEBUG(llvm::errs() << "finding chain for: " << streamID << "\n");

  std::map<unsigned,unsigned> parents;
  std::vector<unsigned> roots;
  for (;;) {
    assert(is.good());
    unsigned id;
    unsigned tag;
    is.read(reinterpret_cast<char*>(&id), sizeof(id));
    is.read(reinterpret_cast<char*>(&tag), sizeof(tag));
    if (tag&(1<<31)) { // fork
      unsigned child = tag ^ (1<<31);

      if (child==streamID) {
        roots.push_back(child);
        while (id) {
          roots.push_back(id);
          std::map<unsigned, unsigned>::iterator it = parents.find(id);
          assert(it!=parents.end());
          id = it->second;
        } 
        break;
      } else {
        parents.insert(std::make_pair(child,id));
      }
    } else {
      unsigned len = tag;
      for (unsigned i=0; i < len; ++i) {
        //FIXME char doesn't store size
        T dummy_overload_hint;
        skip(is, dummy_overload_hint);
      }
    }
  }
  KLEE_DEBUG({
      llvm::errs() << "roots: ";
      for (size_t i = 0, e = roots.size(); i < e; ++i) {
        llvm::errs() << roots[i] << " ";
      }
      llvm::errs() << "\n";
    });
  is.seekg(0, std::ios::beg);
  for (;;) {
    unsigned id;
    unsigned tag;
    is.read(reinterpret_cast<char*>(&id), 4);
    is.read(reinterpret_cast<char*>(&tag), 4);
    if (!is.good()) break;
    if (tag&(1<<31)) { // fork
      unsigned child = tag ^ (1<<31);
      if (id==roots.back() && roots.size()>1 && child==roots[roots.size()-2])
        roots.pop_back();
    } else {
      unsigned len = tag;
      T entry;
      for (unsigned i=0; i < len; ++i) {
        if (id==roots.back()) {
          deserialize(is, entry);
          out.push_back(std::move(entry));
        } else {
          skip(is, entry);
        }
      }
    }
  }
}

// template function installation
template void TreeStreamWriter::write(TreeOStream &os, const char &entry);
template void TreeStreamWriter::write(TreeOStream &os, const std::string &entry);
template void TreeStreamWriter::readStream(TreeStreamID streamID, std::vector<char> &out);
template void TreeStreamWriter::readStream(TreeStreamID streamID, std::vector<std::string> &out);
///

TreeOStream::TreeOStream()
  : writer(0),
    id(0) {
}

TreeOStream::TreeOStream(TreeStreamWriter &_writer, unsigned _id)
  : writer(&_writer),
    id(_id) {
}

TreeOStream::~TreeOStream() {
}

unsigned TreeOStream::getID() const {
  assert(writer);
  return id;
}

void TreeOStream::flush() {
  assert(writer);
  writer->flush();
}

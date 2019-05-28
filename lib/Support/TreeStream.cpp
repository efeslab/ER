//===-- TreeStream.cpp ----------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Internal/ADT/TreeStream.h"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <iterator>

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

void TreeStreamWriter::flush() {
  flush_lastLen();
  output->flush();
}

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

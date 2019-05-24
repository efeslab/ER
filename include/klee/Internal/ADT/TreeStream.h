//===-- TreeStream.h --------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef __UTIL_TREESTREAM_H__
#define __UTIL_TREESTREAM_H__

#include <string>
#include <vector>
#include <iostream>
#include <cassert>

namespace klee {

  typedef unsigned TreeStreamID;
  class TreeOStream;

  class TreeStreamWriter {

    friend class TreeOStream;

  private:
    unsigned lastID, lastLen;
    std::iostream::pos_type lastLen_off;
    bool isWritten, lastLen_dirty;

    std::string path;
    std::ofstream *output;
    unsigned ids;

    static void serialize(std::ostream &os, const char &c) {
      os.put(c);
    }
    static void deserialize(std::istream &is, char &c) {
      is.get(c);
    }
    static void serialize(std::ostream &os, const std::string &str) {
      std::string::size_type size = str.size();
      os.write(reinterpret_cast<const char*>(&size), sizeof(size));
      os.write(reinterpret_cast<const char*>(str.c_str()), str.size());
    }
    static void deserialize(std::istream &is, std::string &str) {
      std::string::size_type size;
      is.read(reinterpret_cast<char*>(&size), sizeof(size));
      str.resize(size);
      is.read(&str[0], size);
    }
    static void skip(std::istream &is, char&) {
      is.seekg(1, std::ios::cur);
    }
    static void skip(std::istream &is, std::string&) {
      std::string::size_type size;
      is.read(reinterpret_cast<char*>(&size), sizeof(size));
      is.seekg(size, std::ios::cur);
    }
    template<typename T> void write(TreeOStream &os, const T &entry);
    void write_metadata(TreeOStream &os);
    void flush_lastLen();

  public:
    TreeStreamWriter(const std::string &_path);
    ~TreeStreamWriter();

    bool good();

    TreeOStream open();
    TreeOStream open(const TreeOStream &node);

    void flush();

    // hack, to be replace by proper stream capabilities
    template <typename T>
    void readStream(TreeStreamID id,
                    std::vector<T> &out);
  };

  class TreeOStream {
    friend class TreeStreamWriter;

  private:
    TreeStreamWriter *writer;
    unsigned id;
    
    TreeOStream(TreeStreamWriter &_writer, unsigned _id);

  public:
    TreeOStream();
    ~TreeOStream();

    unsigned getID() const;

    template<typename T> TreeOStream &operator<<(const T &entry) {
      assert(writer);
      writer->write(*this, entry);
      return *this;
    }

    void flush();
  };
}

#endif

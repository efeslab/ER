#include "klee/Internal/ADT/TreeStream.h"
#include <vector>
#include <cstring>

#include "gtest/gtest.h"

using namespace klee;

/* Basic test, checking that after writing "abc" and then "defg", we
   get a {'a', 'b', 'c', 'c', 'd', 'e', 'f', 'g' } back.  */
TEST(TreeStreamTest, Basic) {
  TreeStreamWriter tsw("tsw1.out");
  ASSERT_TRUE(tsw.good());
  
  TreeOStream tos = tsw.open();
  tos << 'a' << 'b' << 'c';
  tos << 'd' << 'e' << 'f' << 'g';
  tos.flush();

  std::vector<char> out;
  tsw.readStream(tos.getID(), out);
  ASSERT_EQ(7u, out.size());
  
  for (char c = 'a'; c <= 'g'; c++)
    ASSERT_EQ(c, out[c - 'a']);
}


/* This tests the case when we perform a write with a size larger than
   the buffer size, which is a constant set to 4*4096.  This test fails 
   without #704 */
TEST(TreeStreamTest, WriteLargerThanBufferSize) {
  TreeStreamWriter tsw("tsw2.out");
  ASSERT_TRUE(tsw.good());
  
  TreeOStream tos = tsw.open();
#define NBYTES 5*4096UL
  char buf[NBYTES];
  memset(buf, 'A', sizeof(buf));
  tos << std::string(buf, NBYTES);
  tos.flush();

  std::vector<std::string> out;
  tsw.readStream(tos.getID(), out);
  ASSERT_EQ(1u, out.size());
  for (unsigned i=0; i<NBYTES; i++)
    ASSERT_EQ('A', out[0][i]);
}

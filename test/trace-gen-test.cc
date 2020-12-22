#include <vector>

#include "gtest/gtest.h"
#include "util/trace-gen.h"

using namespace cache;
using namespace std;

TEST(SameKeyTrace, Basic) {
  vector<Request> trace = TraceGen::SameKeyTrace(1, "key", "value");
  ASSERT_EQ(trace.size(), 1);
  for (const Request& r: trace) {
    ASSERT_EQ(r.key, "key");
    ASSERT_EQ(r.value, "value");
  }

  trace = TraceGen::SameKeyTrace(10, "key", "value");
  ASSERT_EQ(trace.size(), 10);
  for (const Request& r: trace) {
    ASSERT_EQ(r.key, "key");
    ASSERT_EQ(r.value, "value");
  }
}


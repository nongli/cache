#include <unordered_set>
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

int ComputeUniqueKeys(Trace* trace) {
  unordered_set<string> keys;
  while (true) {
    const Request* r = trace->next();
    if (r == nullptr) {
      break;
    }
    keys.insert(r->key);
  }
  return keys.size();
}

TEST(CycleTrace, Basic) {
  FixedTrace trace1(TraceGen::CycleTrace(100, 100, "v"));
  ASSERT_EQ(100, ComputeUniqueKeys(&trace1));

  FixedTrace trace2(TraceGen::CycleTrace(100, 10, "v"));
  ASSERT_EQ(10, ComputeUniqueKeys(&trace2));
}

TEST(ZipfianTrace, Basic) {
  FixedTrace trace1(TraceGen::ZipfianDistribution(0, 100, 100, 0.7, "v"));
  ASSERT_EQ(57, ComputeUniqueKeys(&trace1));

  FixedTrace trace2(TraceGen::ZipfianDistribution(0, 100, 100, 1, "v"));
  ASSERT_EQ(49, ComputeUniqueKeys(&trace2));

  FixedTrace trace3(TraceGen::ZipfianDistribution(0, 100, 20, 0.7, "v"));
  ASSERT_EQ(20, ComputeUniqueKeys(&trace3));

  FixedTrace trace4(TraceGen::ZipfianDistribution(0, 100, 20, 1, "v"));
  ASSERT_EQ(20, ComputeUniqueKeys(&trace4));
}

TEST(Zipf, Basic) {
  int k = 20;
  Zipfian zipf(k, 1);

  vector<int> histo;
  histo.resize(k + 1);
  for (int n = 0; n < 10000; ++n) {
    histo[zipf.Gen()]++;
  }
  for (int i = 1; i < histo.size(); ++i) {
    printf("%d: %d\n", i, histo[i]);
  }
}


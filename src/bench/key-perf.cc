#include "bench/bench-util.h"
#include "cache/arc.h"
#include "cache/flex-arc.h"
#include "cache/tiered-cache.h"
#include "util/belady.h"
#include "util/table-printer.h"
#include "util/trace-gen.h"

#include "gflags/gflags.h"

#include <map>

/**

trace            cache          hits  misses  evicts     p  max_p  hit %  LRU %  LFU %  miss %  LRU Ghost %  LFU Ghost %  filters   micros/val
----------------------------------------------------------------------------------------------------------------------------------------------
med-seq-cycle    std::string  200000  200000  150000     0      0     50     25     75      50            0            0        -     0.346540
med-seq-cycle    external     200000  200000  150000     0      0     50     25     75      50            0            0        -     0.212600
med-seq-cycle    ref-count    200000  200000  150000     0      0     50     25     75      50            0            0        -     0.271085

seq-cycle-10%    std::string  180000   20000       0     0      0     90     11     88      10            0            0        -     0.126800
seq-cycle-10%    external     180000   20000       0     0      0     90     11     88      10            0            0        -     0.077420
seq-cycle-10%    ref-count    180000   20000       0     0      0     90     11     88      10            0            0        -     0.086190

seq-cycle-50%    std::string      10  199990  149990  5000   5000      0    100      0      99            0           37        -     0.604600
seq-cycle-50%    external         10  199990  149990  5000   5000      0    100      0      99            0           37        -     0.367880
seq-cycle-50%    ref-count        10  199990  149990  5000   5000      0    100      0      99            0           37        -     0.507655

seq-unique       std::string       0  200000  150000     0      0      0      -      -     100            0            0        -     0.613145
seq-unique       external          0  200000  150000     0      0      0      -      -     100            0            0        -     0.360590
seq-unique       ref-count         0  200000  150000     0      0      0      -      -     100            0            0        -     0.504265

tiny-seq-cycle   std::string  200000  200000  150000     0      0     50      1     99      50            0            0        -     0.347293
tiny-seq-cycle   external     200000  200000  150000     0      0     50      1     99      50            0            0        -     0.214822
tiny-seq-cycle   ref-count    200000  200000  150000     0      0     50      1     99      50            0            0        -     0.277520

zipf-.7          std::string   94660  105340   55340  1047   1047     47     26     73      52            0            9        -     0.350630
zipf-.7          external      94660  105340   55340  1047   1047     47     26     73      52            0            9        -     0.257915
zipf-.7          ref-count     94660  105340   55340  1047   1047     47     26     73      52            0            9        -     0.290900

zipf-1           std::string  146650   53350    3350     7      7     73     12     87      26            0            0        -     0.186535
zipf-1           external     146650   53350    3350     7      7     73     12     87      26            0            0        -     0.144245
zipf-1           ref-count    146650   53350    3350     7      7     73     12     87      26            0            0        -     0.147315

zipf-seq         std::string  241120  358880  308880    18   1269     40     10     89      59            0           16        -     0.450433
zipf-seq         external     241120  358880  308880    18   1269     40     10     89      59            0           16        -     0.324565
zipf-seq         ref-count    241120  358880  308880    18   1269     40     10     89      59            0           16        -     0.377113

**/

DEFINE_bool(minimal, true, "Include minimal (aka) smoke caches in tests.");
DEFINE_int64(unique_keys, 20000, "Number of unique keys to test.");
DEFINE_string(base_size, "", "Base size for cache. Defaults to unique_keys");
DEFINE_int64(iters, 5, "Number of times to repeated the trace.");
DEFINE_string(trace, "", "Name of trace to run.");
DEFINE_int64(trace_limits, 0, "How much of the trace to use. 0 means run all");

using namespace std;
using namespace cache;

map<string, Trace*> traces;

void Test(TablePrinter* results, int64_t n, int iters) {
  for (auto trace : traces) {
    AdaptiveCache<string, int64_t, NopLock, TraceSizer> scache(n * .25);
    Run<string>(results, n, trace.first, trace.second, &scache, CacheType::Arc,
                iters, "std::string");

    AdaptiveCache<TestKey, int64_t, NopLock, TraceSizer> kcache(n * .25);
    Run<TestKey>(results, n, trace.first, trace.second, &kcache, CacheType::Arc, iters,
                 "external");

    AdaptiveCache<RefCountKey, int64_t, NopLock, TraceSizer> ref_cache(n * .25);
    Run<RefCountKey>(results, n, trace.first, trace.second, &ref_cache, CacheType::Arc,
                     iters, "ref-count");

    results->AddEmptyRow();
  }
}

void AddTrace(string_view name, Trace* trace) {
  if (!FLAGS_trace.empty() && FLAGS_trace != name) {
    delete trace;
    return;
  }
  traces[string(name)] = trace;
}

template <typename C> void del(vector<C*> v) {
  for (C* c : v) {
    delete c;
  }
}

int main(int argc, char** argv) {
  gflags::SetUsageMessage("Cache Comparison");
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  TablePrinter results;
  results.AddColumn("trace", true);
  results.AddColumn("cache", true);
  results.AddColumn("hits", false);
  results.AddColumn("misses", false);
  results.AddColumn("evicts", false);
  results.AddColumn("p", false);
  results.AddColumn("max_p", false);
  results.AddColumn("hit %", false);
  results.AddColumn("LRU %", false);
  results.AddColumn("LFU %", false);
  results.AddColumn("miss %", false);
  results.AddColumn("LRU Ghost %", false);
  results.AddColumn("LFU Ghost %", false);
  results.AddColumn("filters", false);
  results.AddColumn("micros/val", false);

  const int keys = FLAGS_unique_keys;
  int64_t base_size = keys;
  if (!FLAGS_base_size.empty()) {
    base_size = ParseMemSpec(FLAGS_base_size);
  }
  cerr << "Using base size: " << base_size << endl;
  MemoryPool pool;

  if (FLAGS_trace.empty()) {
    //
    // Configure traces
    //
    AddTrace("seq-unique",
             new FixedTrace(TraceGen::CycleTrace(keys, keys, 1, &pool)));
    AddTrace("seq-cycle-10%",
             new FixedTrace(TraceGen::CycleTrace(keys, keys * .1, 1, &pool)));
    AddTrace("seq-cycle-50%",
             new FixedTrace(TraceGen::CycleTrace(keys, keys * .5, 1, &pool)));
    AddTrace("zipf-1",
             new FixedTrace(TraceGen::ZipfianDistribution(0, keys, keys, 1, 1, &pool)));
    AddTrace("zipf-.7", new FixedTrace(TraceGen::ZipfianDistribution(
                            0, keys, keys, 0.7, 1, &pool)));

    // zipf, all keys, zipf
    FixedTrace* zip_seq =
        new FixedTrace(TraceGen::ZipfianDistribution(0, keys, keys, 0.7, 1, &pool));
    zip_seq->Add(TraceGen::CycleTrace(keys, keys, 1, &pool));
    zip_seq->Add(TraceGen::ZipfianDistribution(0, keys, keys, 0.7, 1, &pool));
    AddTrace("zipf-seq", zip_seq);

    // tiny + all keys
    FixedTrace* tiny_seq_cycle =
        new FixedTrace(TraceGen::CycleTrace(keys, keys * .01, 1, &pool));
    tiny_seq_cycle->Add(TraceGen::CycleTrace(keys, keys, 1, &pool));
    AddTrace("tiny-seq-cycle", tiny_seq_cycle);

    // medium + all keys
    FixedTrace* med_seq_cycle =
        new FixedTrace(TraceGen::CycleTrace(keys, keys * .25, 1, &pool));
    med_seq_cycle->Add(TraceGen::CycleTrace(keys, keys, 1, &pool));
    AddTrace("med-seq-cycle", med_seq_cycle);
  } else {
    TraceReader* reader = new TraceReader(FLAGS_trace, FLAGS_trace_limits, &pool);
    AddTrace(FLAGS_trace, reader);
  }

  Test(&results, base_size, FLAGS_iters);
  printf("%s\n", results.ToString().c_str());

  pool.free();
  for (auto t : traces) {
    delete t.second;
  }
  return 0;
}

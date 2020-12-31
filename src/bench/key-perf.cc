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

$ exe/key-perf --iters 10
trace            cache          hits  misses  evicts     p  max_p  hit %  LRU %  LFU %  miss %  LRU Ghost %  LFU Ghost %  filters   micros/val
----------------------------------------------------------------------------------------------------------------------------------------------
med-seq-cycle    std::string  200000  200000  150000     0      0     50     25     75      50            0            0        -     0.480393
med-seq-cycle    external     200000  200000  150000     0      0     50     25     75      50            0            0        -     0.321110

seq-cycle-10%    std::string  180000   20000       0     0      0     90     11     88      10            0            0        -     0.143300
seq-cycle-10%    external     180000   20000       0     0      0     90     11     88      10            0            0        -     0.090535

seq-cycle-50%    std::string      10  199990  149990  5000   5000      0    100      0      99            0           37        -     1.141740
seq-cycle-50%    external         10  199990  149990  5000   5000      0    100      0      99            0           37        -     0.619650

seq-unique       std::string       0  200000  150000     0      0      0      -      -     100            0            0        -     0.870960
seq-unique       external          0  200000  150000     0      0      0      -      -     100            0            0        -     0.676240

tiny-seq-cycle   std::string  200000  200000  150000     0      0     50      1     99      50            0            0        -     0.448452
tiny-seq-cycle   external     200000  200000  150000     0      0     50      1     99      50            0            0        -     0.325518

zipf-.7          std::string   95170  104830   54830  1021   1021     47     26     73      52            0            9        -     0.542935
zipf-.7          external      95170  104830   54830  1021   1021     47     26     73      52            0            9        -     0.439885

zipf-1           std::string  146180   53820    3820    10     10     73     12     87      26            0            0        -     0.243740
zipf-1           external     146180   53820    3820    10     10     73     12     87      26            0            0        -     0.210915

zipf-seq         std::string  240590  359410  309410     7   1214     40     11     88      59            0           16        -     0.609812
zipf-seq         external     240590  359410  309410     7   1214     40     11     88      59            0           16        -     0.523703

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

#include "bench/bench-util.h"
#include "cache/arc.h"
#include "cache/flex-arc.h"
#include "cache/tiered-cache.h"
#include "util/belady.h"
#include "util/table-printer.h"
#include "util/trace-gen.h"

#include "gflags/gflags.h"

#include <chrono>
#include <map>

/**
trace            cache            hits  misses  evicts     p  max_p  hit %  LRU %  LFU %  miss %  LRU Ghost %  LFU Ghost %  filters   micros/val
------------------------------------------------------------------------------------------------------------------------------------------------
med-seq-cycle    arc-25         400000  400000  300000     0      0     50     25     75      50            0            0        -     0.347035
med-seq-cycle    arc-25-filter  300000  500000       0     0      0     37     33     66      62            0            0   400000     0.286021
med-seq-cycle    lru-25         400000  400000  300000     -      -     50      -      -      50            -            -        -     0.203671
med-seq-cycle    farc-25-400    400000  400000  300000     0      0     50     25     75      50            0            0        -     0.268634

seq-cycle-10%    arc-25         360000   40000       0     0      0     90     11     88      10            0            0        -     0.079740
seq-cycle-10%    arc-25-filter  320000   80000       0     0      0     80     12     87      20            0            0    40000     0.126828
seq-cycle-10%    lru-25         360000   40000       0     -      -     90      -      -      10            -            -        -     0.058572
seq-cycle-10%    farc-25-400    360000   40000       0     0      0     90     11     88      10            0            0        -     0.103008

seq-cycle-50%    arc-25             20  399980  299980  5000   5000      0    100      0      99            0           37        -     0.660722
seq-cycle-50%    arc-25-filter       0  400000  100000     0      0      0      -      -     100            0            0   200000     0.431517
seq-cycle-50%    lru-25              0  400000  300000     -      -      0      -      -     100            -            -        -     0.369670
seq-cycle-50%    farc-25-400     50000  350000  250000  5000   5000     12    100      0      87            0           42        -     0.557948

seq-unique       arc-25              0  400000  300000     0   5000      0      -      -     100            0            0        -     0.636285
seq-unique       arc-25-filter       0  400000       0     0      0      0      -      -     100            0            0   400000     0.405140
seq-unique       lru-25              0  400000  300000     -      -      0      -      -     100            -            -        -     0.368667
seq-unique       farc-25-400         0  400000  300000     0   5000      0      -      -     100            0            0        -     0.675765

tiny-seq-cycle   arc-25         400000  400000  300000     0   5000     50      1     99      50            0            0        -     0.337612
tiny-seq-cycle   arc-25-filter  396000  404000       0     0      0     49      1     98      50            0            0   400000     0.212765
tiny-seq-cycle   lru-25         400000  400000  300000     -      -     50      -      -      50            -            -        -     0.196477
tiny-seq-cycle   farc-25-400    400000  400000  300000     0   5000     50      1     99      50            0            0        -     0.316476

zipf-.7          arc-25         190340  209660  109660  1021   5000     47     26     73      52            0            9        -     0.383600
zipf-.7          arc-25-filter  139440  260560       0     0      0     34     22     77      65            0            0   187560     0.286070
zipf-.7          lru-25         187680  212320  112320     -      -     46      -      -      53            -            -        -     0.231547
zipf-.7          farc-25-400    190340  209660  109660  1105   5000     47     26     73      52            0           10        -     0.351245

zipf-1           arc-25         292360  107640    7640    10   5000     73     12     87      26            0            0        -     0.172180
zipf-1           arc-25-filter  254660  145340       0     0      0     63      7     92      36            0            0   107440     0.194488
zipf-1           lru-25         292340  107660    7660     -      -     73      -      -      26            -            -        -     0.112543
zipf-1           farc-25-400    292360  107640    7640    10   5000     73     12     87      26            0            0        -     0.192735

zipf-seq         arc-25         481180  718820  618820     7   5000     40     11     88      59            0           16        -     0.473558
zipf-seq         arc-25-filter  464120  735880  168600    86   1128     38     11     88      61            0           12   467280     0.425268
zipf-seq         lru-25         417920  782080  682080     -      -     34      -      -      65            -            -        -     0.279900
zipf-seq         farc-25-400    451340  748660  648660     0   5000     37     11     88      62            0           46        -     0.569691

**/

DEFINE_bool(include_lru, true, "Include lru cache in tests.");
DEFINE_bool(include_belady, false, "Include belady cache in tests.");
DEFINE_bool(include_tiered, false, "Include tiered cache in tests.");

DEFINE_bool(minimal, true, "Include minimal (aka) smoke caches in tests.");
DEFINE_int64(unique_keys, 20000, "Number of unique keys to test.");
DEFINE_string(base_size, "", "Base size for cache. Defaults to unique_keys");
DEFINE_int64(iters, 5, "Number of times to repeated the trace.");
DEFINE_string(trace, "", "Name of trace to run.");
DEFINE_int64(trace_limits, 0, "How much of the trace to use. 0 means run all");

using namespace std;
using namespace cache;

typedef TieredCache<RefCountKey, int64_t,
                    AdaptiveCache<RefCountKey, int64_t, NopLock, TraceSizer>>
    TieredArc;

map<string, Trace*> traces;
vector<AdaptiveCache<RefCountKey, int64_t, NopLock, TraceSizer>*> arcs;
vector<LRUCache<RefCountKey, int64_t, NopLock, TraceSizer>*> lrus;
vector<FlexARC<RefCountKey, int64_t, NopLock, TraceSizer>*> farcs;
vector<TieredArc*> tiered_caches;

void Test(TablePrinter* results, int64_t base_size, int iters) {
  for (auto trace : traces) {
    for (AdaptiveCache<RefCountKey, int64_t, NopLock, TraceSizer>* cache : arcs) {
      Run<RefCountKey>(results, base_size, trace.first, trace.second, cache,
                       CacheType::Arc, iters);
    }
    for (LRUCache<RefCountKey, int64_t, NopLock, TraceSizer>* cache : lrus) {
      Run<RefCountKey>(results, base_size, trace.first, trace.second, cache,
                       CacheType::Lru, iters);
    }
    for (FlexARC<RefCountKey, int64_t, NopLock, TraceSizer>* cache : farcs) {
      Run<RefCountKey>(results, base_size, trace.first, trace.second, cache,
                       CacheType::Farc, iters);
    }
    for (TieredArc* cache : tiered_caches) {
      Run<RefCountKey>(results, base_size, trace.first, trace.second, cache,
                       CacheType::Tiered, iters);
    }
    if (FLAGS_include_belady) {
      BeladyCache<string, int64_t> cache(base_size * .25, trace.second);
      Run<string>(results, base_size, trace.first, trace.second, &cache,
                  CacheType::Belady, iters);
    }

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

  if (FLAGS_trace.empty()) {
    //
    // Configure traces
    //
    AddTrace("seq-unique", new FixedTrace(TraceGen::CycleTrace(keys, keys, 1)));
    AddTrace("seq-cycle-10%",
             new FixedTrace(TraceGen::CycleTrace(keys, keys * .1, 1)));
    AddTrace("seq-cycle-50%",
             new FixedTrace(TraceGen::CycleTrace(keys, keys * .5, 1)));
    AddTrace("zipf-1", new FixedTrace(
                           TraceGen::ZipfianDistribution(0, keys, keys, 1, 1)));
    AddTrace("zipf-.7", new FixedTrace(TraceGen::ZipfianDistribution(
                            0, keys, keys, 0.7, 1)));

    // zipf, all keys, zipf
    FixedTrace* zip_seq =
        new FixedTrace(TraceGen::ZipfianDistribution(0, keys, keys, 0.7, 1));
    zip_seq->Add(TraceGen::CycleTrace(keys, keys, 1));
    zip_seq->Add(TraceGen::ZipfianDistribution(0, keys, keys, 0.7, 1));
    AddTrace("zipf-seq", zip_seq);

    // tiny + all keys
    FixedTrace* tiny_seq_cycle =
        new FixedTrace(TraceGen::CycleTrace(keys, keys * .01, 1));
    tiny_seq_cycle->Add(TraceGen::CycleTrace(keys, keys, 1));
    AddTrace("tiny-seq-cycle", tiny_seq_cycle);

    // medium + all keys
    FixedTrace* med_seq_cycle =
        new FixedTrace(TraceGen::CycleTrace(keys, keys * .25, 1));
    med_seq_cycle->Add(TraceGen::CycleTrace(keys, keys, 1));
    AddTrace("med-seq-cycle", med_seq_cycle);
  } else {
    TraceReader* reader = new TraceReader(FLAGS_trace, FLAGS_trace_limits);
    AddTrace(FLAGS_trace, reader);
  }

  //
  // Configure caches
  //
  if (FLAGS_minimal) {
    arcs.push_back(new AdaptiveCache<RefCountKey, int64_t, NopLock, TraceSizer>(
        base_size * .25));
    arcs.push_back(new AdaptiveCache<RefCountKey, int64_t, NopLock, TraceSizer>(
        base_size * .25, base_size * .5));
    farcs.push_back(new FlexARC<RefCountKey, int64_t, NopLock, TraceSizer>(
        base_size * .25, base_size));
    lrus.push_back(
        new LRUCache<RefCountKey, int64_t, NopLock, TraceSizer>(base_size * .25));
  } else {
    const vector<double> cache_sizes{.05, .1, .5, 1.0};
    const vector<double> ghost_sizes{.5, 1.0, 2.0, 3.0};
    for (double sz : cache_sizes) {
      arcs.push_back(new AdaptiveCache<RefCountKey, int64_t, NopLock, TraceSizer>(
          base_size * sz));
      if (FLAGS_include_lru) {
        lrus.push_back(
            new LRUCache<RefCountKey, int64_t, NopLock, TraceSizer>(base_size * sz));
      }
      for (double gs : ghost_sizes) {
        farcs.push_back(new FlexARC<RefCountKey, int64_t, NopLock, TraceSizer>(
            base_size * sz, base_size * sz * gs));
      }
    }
  }

  if (FLAGS_include_tiered) {
    TieredArc* tiered = new TieredArc();
    tiered->add_cache(
        10, make_shared<AdaptiveCache<RefCountKey, int64_t, NopLock, TraceSizer>>(
                base_size * .25));
    tiered_caches.push_back(tiered);
  }

  Test(&results, base_size, FLAGS_iters);
  printf("%s\n", results.ToString().c_str());

  for (auto t : traces) {
    delete t.second;
  }
  del(arcs);
  del(lrus);
  del(farcs);
  del(tiered_caches);
  return 0;
}

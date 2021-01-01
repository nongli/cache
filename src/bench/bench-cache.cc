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

root@7e9695457159:/cache/build/release# exe/bench-cache --include_belady
trace            cache            hits  misses  evicts     p  max_p  hit %  LRU %  LFU %  miss %  LRU Ghost %  LFU Ghost %  filters   micros/val
------------------------------------------------------------------------------------------------------------------------------------------------
med-seq-cycle    arc-25         100000  100000   75000     0      0     50     25     75      50            0            0        -     0.316085
med-seq-cycle    arc-25-filter   75000  125000       0     0      0     37     33     66      62            0            0   100000     0.304790
med-seq-cycle    lru-25         100000  100000   75000     -      -     50      -      -      50            -            -        -     0.149010
med-seq-cycle    farc-25-400    100000  100000   75000     0      0     50     25     75      50            0            0        -     0.300020
med-seq-cycle    belady-25      100000  100000   75000     0      0     50      0      0      50            0            0        -     0.360075

seq-cycle-10%    arc-25          90000   10000       0     0      0     90     11     88      10            0            0        -     0.097650
seq-cycle-10%    arc-25-filter   80000   20000       0     0      0     80     12     87      20            0            0    10000     0.126270
seq-cycle-10%    lru-25          90000   10000       0     -      -     90      -      -      10            -            -        -     0.049190
seq-cycle-10%    farc-25-400     90000   10000       0     0      0     90     11     88      10            0            0        -     0.098940
seq-cycle-10%    belady-25       90000   10000       0     0      0     90      0      0      10            0            0        -     0.253060

seq-cycle-50%    arc-25              5   99995   74995  5000   5000      0    100      0      99            0           37        -     0.589000
seq-cycle-50%    arc-25-filter       0  100000   25000     0      0      0      -      -     100            0            0    50000     0.431300
seq-cycle-50%    lru-25              0  100000   75000     -      -      0      -      -     100            -            -        -     0.250990
seq-cycle-50%    farc-25-400     12500   87500   62500  5000   5000     12    100      0      87            0           42        -     0.487220
seq-cycle-50%    belady-25       25000   75000   50000     0      0     25      0      0      75            0            0        -     0.366350

seq-unique       arc-25              0  100000   75000     0   5000      0      -      -     100            0            0        -     0.605440
seq-unique       arc-25-filter       0  100000       0     0      0      0      -      -     100            0            0   100000     0.359190
seq-unique       lru-25              0  100000   75000     -      -      0      -      -     100            -            -        -     0.251490
seq-unique       farc-25-400         0  100000   75000     0   5000      0      -      -     100            0            0        -     0.550580
seq-unique       belady-25           0  100000   75000     0      0      0      -      -     100            0            0        -     0.402990

tiny-seq-cycle   arc-25         100000  100000   75000     0   5000     50      1     99      50            0            0        -     0.287485
tiny-seq-cycle   arc-25-filter   99000  101000       0     0      0     49      1     98      50            0            0   100000     0.211110
tiny-seq-cycle   lru-25         100000  100000   75000     -      -     50      -      -      50            -            -        -     0.145220
tiny-seq-cycle   farc-25-400    100000  100000   75000     0   5000     50      1     99      50            0            0        -     0.307500
tiny-seq-cycle   belady-25      100000  100000   75000     0      0     50      0      0      50            0            0        -     0.295940

zipf-.7          arc-25          47330   52670   27670  1047   5000     47     26     73      52            0            9        -     0.338940
zipf-.7          arc-25-filter   34995   65005       0     0      0     34     22     77      65            0            0    46940     0.278350
zipf-.7          lru-25          46755   53245   28245     -      -     46      -      -      53            -            -        -     0.160760
zipf-.7          farc-25-400     47330   52670   27670  1146   5000     47     26     73      52            0           10        -     0.326430
zipf-.7          belady-25       53060   46940   21940     0      0     53      0      0      46            0            0        -     0.439430

zipf-1           arc-25          73325   26675    1675     7   5000     73     12     87      26            0            0        -     0.161370
zipf-1           arc-25-filter   64425   35575       0     0      0     64      7     92      35            0            0    26640     0.167040
zipf-1           lru-25          73325   26675    1675     -      -     73      -      -      26            -            -        -     0.095180
zipf-1           farc-25-400     73325   26675    1675     7   5000     73     12     87      26            0            0        -     0.162440
zipf-1           belady-25       73360   26640    1640     0      0     73      0      0      26            0            0        -     0.344810

zipf-seq         arc-25         120560  179440  154440    18   5000     40     10     89      59            0           16        -     0.439803
zipf-seq         arc-25-filter  115620  184380   42345    66   1156     38     11     88      61            0           12   117035     0.379547
zipf-seq         lru-25         104200  195800  170800     -      -     34      -      -      65            -            -        -     0.203370
zipf-seq         farc-25-400    112460  187540  162540     0   5000     37     11     88      62            0           46        -     0.477113
zipf-seq         belady-25      156110  143890  118890     0      0     52      0      0      47            0            0        -     0.480910

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

typedef TieredCache<string, int64_t,
                    AdaptiveCache<string, int64_t, NopLock, TraceSizer>>
    TieredArc;

map<string, Trace*> traces;
vector<AdaptiveCache<string, int64_t, NopLock, TraceSizer>*> arcs;
vector<LRUCache<string, int64_t, NopLock, TraceSizer>*> lrus;
vector<FlexARC<string, int64_t, NopLock, TraceSizer>*> farcs;
vector<TieredArc*> tiered_caches;

void Test(TablePrinter* results, int64_t base_size, int iters) {
  for (auto trace : traces) {
    for (AdaptiveCache<string, int64_t, NopLock, TraceSizer>* cache : arcs) {
      Run<string>(results, base_size, trace.first, trace.second, cache,
                  CacheType::Arc, iters);
    }
    for (LRUCache<string, int64_t, NopLock, TraceSizer>* cache : lrus) {
      Run<string>(results, base_size, trace.first, trace.second, cache,
                  CacheType::Lru, iters);
    }
    for (FlexARC<string, int64_t, NopLock, TraceSizer>* cache : farcs) {
      Run<string>(results, base_size, trace.first, trace.second, cache,
                  CacheType::Farc, iters);
    }
    for (TieredArc* cache : tiered_caches) {
      Run<string>(results, base_size, trace.first, trace.second, cache,
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
    arcs.push_back(new AdaptiveCache<string, int64_t, NopLock, TraceSizer>(
        base_size * .25));
    arcs.push_back(new AdaptiveCache<string, int64_t, NopLock, TraceSizer>(
        base_size * .25, base_size * .5));
    farcs.push_back(new FlexARC<string, int64_t, NopLock, TraceSizer>(
        base_size * .25, base_size));
    lrus.push_back(
        new LRUCache<string, int64_t, NopLock, TraceSizer>(base_size * .25));
  } else {
    const vector<double> cache_sizes{.05, .1, .5, 1.0};
    const vector<double> ghost_sizes{.5, 1.0, 2.0, 3.0};
    for (double sz : cache_sizes) {
      arcs.push_back(new AdaptiveCache<string, int64_t, NopLock, TraceSizer>(
          base_size * sz));
      if (FLAGS_include_lru) {
        lrus.push_back(
            new LRUCache<string, int64_t, NopLock, TraceSizer>(base_size * sz));
      }
      for (double gs : ghost_sizes) {
        farcs.push_back(new FlexARC<string, int64_t, NopLock, TraceSizer>(
            base_size * sz, base_size * sz * gs));
      }
    }
  }

  if (FLAGS_include_tiered) {
    TieredArc* tiered = new TieredArc();
    tiered->add_cache(
        10, make_shared<AdaptiveCache<string, int64_t, NopLock, TraceSizer>>(
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

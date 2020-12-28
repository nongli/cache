#include "cache/arc.h"
#include "cache/flex-arc.h"
#include "util/table-printer.h"
#include "util/trace-gen.h"

#include "gflags/gflags.h"

#include <chrono>
#include <map>

/**
$ exe/bench-cache

trace            cache            hits  misses  evicts     p  max_p  hit %  LRU %  LFU %  miss %  LRU Ghost %  LFU Ghost %  filters   micros/val
------------------------------------------------------------------------------------------------------------------------------------------------
med-seq-cycle    arc-25         119996   80004   75004     0      0     59      4     95      40            0            0        -     3.773340
med-seq-cycle    arc-25-filter  115000   85000       0     0      0     57      4     95      42            0            0    80000     2.777510
med-seq-cycle    lru-25         100000  100000   95000     -      -     50      -      -      50            -            -        -     1.992620
med-seq-cycle    farc-25-400    105000   95000   90000     0   5000     52      4     95      47            0           78        -     4.552605

seq-cycle-10%    arc-25          98000    2000       0     0      0     98      2     97       2            0            0        -     1.100750
seq-cycle-10%    arc-25-filter   96000    4000       0     0      0     96      2     97       4            0            0     2000     1.161190
seq-cycle-10%    lru-25          98000    2000       0     -      -     98      -      -       2            -            -        -     0.523590
seq-cycle-10%    farc-25-400     98000    2000       0     0   5000     98      2     97       2            0            0        -     1.050600

seq-cycle-50%    arc-25           2501   97499   92499  3750   5000      2    100      0      97            0           73        -     8.100470
seq-cycle-50%    arc-25-filter    2501   97499   82499     0   5000      2    100      0      97            0           64    10000     7.730420
seq-cycle-50%    lru-25              0  100000   95000     -      -      0      -      -     100            -            -        -     3.455090
seq-cycle-50%    farc-25-400      2500   97500   92500     0   5000      2    100      0      97            0           89        -     8.080820

seq-unique       arc-25              0  100000   95000     0   5000      0      -      -     100            0            0        -     7.825240
seq-unique       arc-25-filter       0  100000       0     0   5000      0      -      -     100            0            0   100000     4.866460
seq-unique       lru-25              0  100000   95000     -      -      0      -      -     100            -            -        -     3.495960
seq-unique       farc-25-400      2500   97500   92500     0   5000      2    100      0      97            0           79        -     8.449350

tiny-seq-cycle   arc-25         100800   99200   94200     0   5000     50      0     99      49            0            0        -     4.371300
tiny-seq-cycle   arc-25-filter  100600   99400       0     0   5000     50      0     99      49            0            0    99200     3.045655
tiny-seq-cycle   lru-25         100000  100000   95000     -      -     50      -      -      50            -            -        -     1.991965
tiny-seq-cycle   farc-25-400    102600   97400   92400     0   5000     51      2     97      48            0           79        -     4.732015

zipf-.7          arc-25          66352   33648   28648  1061   5000     66      3     96      33            0            3        -     3.574240
zipf-.7          arc-25-filter   62910   37090   22702   932   5000     62      4     95      37            0            2     9388     3.317760
zipf-.7          lru-25          51667   48333   43333     -      -     51      -      -      48            -            -        -     1.960610
zipf-.7          farc-25-400     52870   47130   42130     0   5000     52      4     95      47            0           80        -     4.703560

zipf-1           arc-25          80558   19442   14442     0   5000     80      3     96      19            0           72        -     2.374250
zipf-1           arc-25-filter   79472   20528   10200     0   5000     79      3     96      20            0           48     5328     2.250710
zipf-1           lru-25          79849   20151   15151     -      -     79      -      -      20            -            -        -     1.124310
zipf-1           farc-25-400     80558   19442   14442     0   5000     80      3     96      19            0           72        -     2.656020

zipf-seq         arc-25         128581  171419  166419    31   5000     42      2     97      57            0           28        -     5.309143
zipf-seq         arc-25-filter  117022  182978  112119     0   5000     39      2     97      60            0           58    65859     5.003490
zipf-seq         lru-25         109112  190888  185888     -      -     36      -      -      63            -            -        -     2.552693
zipf-seq         farc-25-400    110764  189236  184236     0   5000     36      2     97      63            0           89        -     6.168880

**/

DEFINE_bool(include_lru, true, "Include lru cache in tests.");
DEFINE_bool(minimal, true, "Include minimal (aka) smoke caches in tests.");
DEFINE_int64(unique_keys, 20000, "Number of unique keys to test.");
DEFINE_int64(iters, 5, "Number of times to repeated the trace.");
DEFINE_string(trace, "", "Name of trace to run.");

using namespace std;
using namespace cache;

map<string, Trace*> traces;
vector<AdaptiveCache<string, string>*> arcs;
vector<LRUCache<string, string>*> lrus;
vector<FlexARC<string, string>*> farcs;

enum class CacheType { Lru, Arc, Farc };

template <class Cache>
void Test(TablePrinter* results, int n, const string& name, Trace* trace,
          Cache* cache, CacheType type, int iters) {
  cache->clear();

  int64_t total_vals = 0;
  double total_micros = 0;
  for (int i = 0; i < iters; ++i) {
    trace->Reset();
    chrono::steady_clock::time_point start = chrono::steady_clock::now();
    while (true) {
      const Request* r = trace->next();
      if (r == nullptr) {
        break;
      }
      shared_ptr<string> val = cache->get(r->key);
      ++total_vals;
      if (!val) {
        cache->add_to_cache(r->key, make_shared<string>(r->value));
      }
    }
    chrono::steady_clock::time_point end = chrono::steady_clock::now();
    total_micros += chrono::duration_cast<chrono::microseconds>(end - start).count();
  }

  Stats stats = cache->stats();
  vector<string> row;
  row.push_back(name);
  switch (type) {
  case CacheType::Arc:
    if (cache->filter_size() > 0) {
      row.push_back("arc-" + to_string(cache->max_size() * 100 / n) + "-filter");
    } else {
      row.push_back("arc-" + to_string(cache->max_size() * 100 / n));
    }
    break;
  case CacheType::Lru:
    row.push_back("lru-" + to_string(cache->max_size() * 100 / n));
    break;
  case CacheType::Farc:
    row.push_back(
        "farc-" + to_string(cache->max_size() * 100 / n) + "-" +
        to_string(dynamic_cast<FlexARC<string, string>*>(cache)->ghost_size() *
                  100 / cache->max_size()));
    break;
  default:
    assert(false);
  }
  row.push_back(to_string(stats.num_hits));
  row.push_back(to_string(stats.num_misses));
  row.push_back(to_string(stats.num_evicted));
  if (type == CacheType::Lru) {
    row.push_back("-");
    row.push_back("-");
  } else {
    row.push_back(to_string(cache->p()));
    row.push_back(to_string(cache->max_p()));
  }
  row.push_back(
      to_string(stats.num_hits * 100 / (stats.num_hits + stats.num_misses)));
  if (type == CacheType::Lru) {
    row.push_back("-");
    row.push_back("-");
  } else {
    if (stats.num_hits > 0) {
      row.push_back(to_string(stats.lru_hits * 100 / stats.num_hits));
      row.push_back(to_string(stats.lfu_hits * 100 / stats.num_hits));
    } else {
      row.push_back("-");
      row.push_back("-");
    }
  }
  row.push_back(
      to_string(stats.num_misses * 100 / (stats.num_hits + stats.num_misses)));
  if (type == CacheType::Lru) {
    row.push_back("-");
    row.push_back("-");
  } else {
    if (stats.num_misses > 0) {
      row.push_back(to_string(stats.lru_ghost_hits * 100 / stats.num_misses));
      row.push_back(to_string(stats.lfu_ghost_hits * 100 / stats.num_misses));
    } else {
      row.push_back("-");
      row.push_back("-");
    }
  }
  if (stats.arc_filter > 0) {
    row.push_back(to_string(stats.arc_filter));
  } else {
    row.push_back("-");
  }
  row.push_back(to_string(total_micros / total_vals));

  results->AddRow(row);
}

void Test(TablePrinter* results, int n, int iters) {
  for (auto trace : traces) {
    for (AdaptiveCache<string, string>* cache : arcs) {
      Test(results, n, trace.first, trace.second, cache, CacheType::Arc, iters);
    }
    for (LRUCache<string, string>* cache : lrus) {
      Test(results, n, trace.first, trace.second, cache, CacheType::Lru, iters);
    }
    for (FlexARC<string, string>* cache : farcs) {
      Test(results, n, trace.first, trace.second, cache, CacheType::Farc,
           iters);
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

  //
  // Configure traces
  //
  AddTrace("seq-unique", new FixedTrace(TraceGen::CycleTrace(keys, keys, "v")));
  AddTrace("seq-cycle-10%",
      new FixedTrace(TraceGen::CycleTrace(keys, keys * .1, "v")));
  AddTrace("seq-cycle-50%",
      new FixedTrace(TraceGen::CycleTrace(keys, keys * .5, "v")));
  AddTrace("zipf-1",
      new FixedTrace(TraceGen::ZipfianDistribution(0, keys, keys, 1, "v")));
  AddTrace("zipf-.7",
      new FixedTrace(TraceGen::ZipfianDistribution(0, keys, keys, 0.7, "v")));

  // zipf, all keys, zipf
  FixedTrace* zip_seq =
      new FixedTrace(TraceGen::ZipfianDistribution(0, keys, keys, 0.7, "v"));
  zip_seq->Add(TraceGen::CycleTrace(keys, keys, "v"));
  zip_seq->Add(TraceGen::ZipfianDistribution(0, keys, keys, 0.7, "v"));
  AddTrace("zipf-seq", zip_seq);

  // tiny + all keys
  FixedTrace* tiny_seq_cycle = new FixedTrace(
      TraceGen::CycleTrace(keys, keys * .01, "v"));
  tiny_seq_cycle->Add(TraceGen::CycleTrace(keys, keys, "v"));
  AddTrace("tiny-seq-cycle", tiny_seq_cycle);

  // medium + all keys
  FixedTrace* med_seq_cycle = new FixedTrace(
      TraceGen::CycleTrace(keys, keys * .25, "v"));
  med_seq_cycle->Add(TraceGen::CycleTrace(keys, keys, "v"));
  AddTrace("med-seq-cycle", med_seq_cycle);

  //
  // Configure caches
  //
  if (FLAGS_minimal) {
    arcs.push_back(new AdaptiveCache<string, string>(keys * .25));
    arcs.push_back(new AdaptiveCache<string, string>(keys * .25, keys * .5));
    farcs.push_back(new FlexARC<string, string>(keys * .25, keys));
    lrus.push_back(new LRUCache<string, string>(keys * .25));
  } else {
    const vector<double> cache_sizes{.05, .1, .5, 1.0};
    const vector<double> ghost_sizes{.5, 1.0, 2.0, 3.0};
    for (double sz : cache_sizes) {
      arcs.push_back(new AdaptiveCache<string, string>(keys * sz));
      if (FLAGS_include_lru) {
        lrus.push_back(new LRUCache<string, string>(keys * sz));
      }
      for (double gs : ghost_sizes) {
        farcs.push_back(new FlexARC<string, string>(keys * sz, keys * sz * gs));
      }
    }
  }

  Test(&results, keys, FLAGS_iters);
  printf("%s\n", results.ToString().c_str());

  for (auto t : traces) {
    delete t.second;
  }
  for (AdaptiveCache<string, string>* c : arcs) {
    delete c;
  }
  for (LRUCache<string, string>* c : lrus) {
    delete c;
  }

  for (FlexARC<string, string>* c : farcs) {
    delete c;
  }
  return 0;
}

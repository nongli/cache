#include "include/arc.h"
#include "include/flex-arc.h"
#include "include/stats.h"
#include "util/table-printer.h"
#include "util/trace-gen.h"

#include "gflags/gflags.h"

#include <map>

/**

build$ exe/bench-cache -minimal -iters 5
trace              cache            hits   misses   evicts      p    hit %
--------------------------------------------------------------------------
seq-cycle-10%      arc-25          98000     2000        0      0       98
seq-cycle-10%      lru-25          98000     2000        0      0       98
seq-cycle-10%      farc-25-400     98000     2000        0      0       98

seq-cycle-50%      arc-25           2501    97499    92499   3750        2
seq-cycle-50%      lru-25              0   100000    95000      0        0
seq-cycle-50%      farc-25-400      2500    97500    92500      0        2

seq-unique         arc-25              0   100000    95000      0        0
seq-unique         lru-25              0   100000    95000      0        0
seq-unique         farc-25-400      2500    97500    92500      0        2

small-big-cycle    arc-25         108000    92000    87000      0       54
small-big-cycle    lru-25         100000   100000    95000      0       50
small-big-cycle    farc-25-400    103500    96500    91500      0       51

zipf-.7            arc-25          66602    33398    28398   1024       66
zipf-.7            lru-25          51784    48216    43216      0       51
zipf-.7            farc-25-400     53025    46975    41975      0       53

zipf-1             arc-25          80782    19218    14218      0       80
zipf-1             lru-25          79825    20175    15175      0       79
zipf-1             farc-25-400     80782    19218    14218      0       80

zipf-seq           arc-25         128184   171816   166816     37       42
zipf-seq           lru-25         109344   190656   185656      0       36
zipf-seq           farc-25-400    111015   188985   183985      0       37

**/

DEFINE_bool(include_lru, true, "Include lru cache in tests.");
DEFINE_bool(minimal, true, "Include minimal (aka) smoke caches in tests.");
DEFINE_int64(unique_keys, 20000, "Number of unique keys to test.");
DEFINE_int64(iters, 1, "Number of times to repeated the trace.");

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
  cache->Clear();

  for (int i = 0; i < iters; ++i) {
    trace->Reset();
    while (true) {
      const Request* r = trace->next();
      if (r == nullptr) {
        break;
      }
      shared_ptr<string> val = cache->get(r->key);
      if (!val) {
        cache->add_to_cache(r->key, make_shared<string>(r->value));
      }
    }
  }

  Stats stats = cache->stats();
  vector<string> row;
  row.push_back(name);
  switch (type) {
  case CacheType::Arc:
    row.push_back("arc-" + to_string(cache->max_size() * 100 / n));
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
  row.push_back(to_string(cache->p()));
  row.push_back(
      to_string(stats.num_hits * 100 / (stats.num_hits + stats.num_misses)));
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
      Test(results, n, trace.first, trace.second, cache, CacheType::Farc, iters);
    }

    results->AddEmptyRow();
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
  results.AddColumn("hit %", false);

  const int keys = FLAGS_unique_keys;

  //
  // Configure traces
  //
  traces["seq-unique"] =
      new FixedTrace(TraceGen::CycleTrace(keys, keys, "v"));
  traces["seq-cycle-10%"] =
      new FixedTrace(TraceGen::CycleTrace(keys, keys * .1, "v"));
  traces["seq-cycle-50%"] =
      new FixedTrace(TraceGen::CycleTrace(keys, keys * .5, "v"));
  traces["zipf-1"] = new FixedTrace(
      TraceGen::ZipfianDistribution(0, keys, keys, 1, "v"));
  traces["zipf-.7"] = new FixedTrace(
      TraceGen::ZipfianDistribution(0, keys, keys, 0.7, "v"));

  // zipf, all keys, zipf
  FixedTrace* zip_seq = new FixedTrace(
      TraceGen::ZipfianDistribution(0, keys, keys, 0.7, "v"));
  zip_seq->Add(TraceGen::CycleTrace(keys, keys, "v"));
  zip_seq->Add(
      TraceGen::ZipfianDistribution(0, keys, keys, 0.7, "v"));
  traces["zipf-seq"] = zip_seq;

  // small cycle, all keys, small cycle
  FixedTrace* cycle_seq = new FixedTrace(TraceGen::CycleTrace(keys, keys * .01, "v"));
  cycle_seq->Add(TraceGen::CycleTrace(keys, keys * .01, "v"));
  cycle_seq->Add(TraceGen::CycleTrace(keys, keys, "v"));
  traces["small-big-cycle"] = cycle_seq;

  //
  // Configure caches
  //
  if (FLAGS_minimal) {
    arcs.push_back(new AdaptiveCache<string, string>(keys * .25));
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
        farcs.push_back(
            new FlexARC<string, string>(keys * sz, keys * sz * gs));
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

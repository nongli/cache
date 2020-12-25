#include "include/arc.h"
#include "include/stats.h"
#include "util/table-printer.h"
#include "util/trace-gen.h"

#include <map>

using namespace std;
using namespace cache;

map<string, Trace*> traces;
vector<AdaptiveCache<string, string>*> arcs;
vector<LRUCache<string, string>*> lrus;

template <class Cache> Stats TestTrace(Cache cache, Trace* trace) {
  trace->Reset();
  while (true) {
    const Request* r = trace->next();
    if (r == nullptr) {
      break;
    }
    shared_ptr<string> val = cache.get(r->key);
    if (!val) {
      cache.add_to_cache(r->key, make_shared<string>(r->value));
    }
  }
  return cache.stats();
}

template <class Cache>
void Test(TablePrinter* results, int n, const string& name, Trace* trace,
    Cache* cache, bool arc) {
  trace->Reset();
  cache->Clear();
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

  Stats stats = cache->stats();
  vector<string> row;
  row.push_back(name);
  if (arc) {
    row.push_back("arc-" + to_string(cache->max_size() * 100 / n));
  } else {
    row.push_back("lru-" + to_string(cache->max_size() * 100 / n));
  }
  row.push_back(to_string(stats.num_hits));
  row.push_back(to_string(stats.num_misses));
  row.push_back(to_string(stats.num_evicted));
  row.push_back(to_string(stats.num_hits * 100 / (stats.num_hits + stats.num_misses)));
  results->AddRow(row);
}

void Test(TablePrinter* results, int n) {
  for (auto trace: traces) {
    for (AdaptiveCache<string, string>* cache: arcs) {
      Test(results, n, trace.first, trace.second, cache, true);
    }
    for (LRUCache<string, string>* cache: lrus) {
      Test(results, n, trace.first, trace.second, cache, false);
    }
    vector<string> row;
    row.push_back("");
    row.push_back("");
    row.push_back("");
    row.push_back("");
    row.push_back("");
    row.push_back("");
    results->AddRow(row);
  }
}

int main(int argc, char** argv) {
  TablePrinter results;
  results.AddColumn("trace", true);
  results.AddColumn("cache", true);
  results.AddColumn("hits", false);
  results.AddColumn("misses", false);
  results.AddColumn("evicts", false);
  results.AddColumn("hit %", false);

  int UNIQUE_KEYS = 20000;

  // Configure traces
  traces["seq-unique"] = new FixedTrace(
      TraceGen::CycleTrace(UNIQUE_KEYS, UNIQUE_KEYS, "v"));
  traces["seq-cycle-10%"] = new FixedTrace(
      TraceGen::CycleTrace(UNIQUE_KEYS, UNIQUE_KEYS * .1, "v"));
  traces["seq-cycle-50%"] = new FixedTrace(
      TraceGen::CycleTrace(UNIQUE_KEYS, UNIQUE_KEYS * .5, "v"));
  traces["zipf-1"] = new FixedTrace(
      TraceGen::ZipfianDistribution(0, UNIQUE_KEYS, UNIQUE_KEYS, 1, "v"));
  traces["zipf-.7"] = new FixedTrace(
      TraceGen::ZipfianDistribution(0, UNIQUE_KEYS, UNIQUE_KEYS, 0.7, "v"));

  // zipf, all keys, zipf
  FixedTrace* zip_seq = new FixedTrace(
      TraceGen::ZipfianDistribution(0, UNIQUE_KEYS, UNIQUE_KEYS, 0.7, "v"));
  zip_seq->Add(TraceGen::CycleTrace(UNIQUE_KEYS, UNIQUE_KEYS, "v"));
  zip_seq->Add(TraceGen::ZipfianDistribution(0, UNIQUE_KEYS, UNIQUE_KEYS, 0.7, "v"));
  traces["zipf-seq"] = zip_seq;

  // Configure caches
  arcs.push_back(new AdaptiveCache<string, string>(UNIQUE_KEYS * .05));
  arcs.push_back(new AdaptiveCache<string, string>(UNIQUE_KEYS * .1));
  arcs.push_back(new AdaptiveCache<string, string>(UNIQUE_KEYS * .5));
  arcs.push_back(new AdaptiveCache<string, string>(UNIQUE_KEYS));

  lrus.push_back(new LRUCache<string, string>(UNIQUE_KEYS * .05));
  lrus.push_back(new LRUCache<string, string>(UNIQUE_KEYS * .1));
  lrus.push_back(new LRUCache<string, string>(UNIQUE_KEYS * .5));
  lrus.push_back(new LRUCache<string, string>(UNIQUE_KEYS));

  Test(&results, UNIQUE_KEYS);
  printf("%s\n", results.ToString().c_str());

  for (auto t: traces) {
    delete t.second;
  }
  for (AdaptiveCache<string, string>* c: arcs) {
    delete c;
  }
  for (LRUCache<string, string>* c: lrus) {
    delete c;
  }
  return 0;
}

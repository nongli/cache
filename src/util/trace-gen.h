#pragma once

#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace cache {

// Implementation of generic hash.
// Copied from gcc
// Note it appears they use murmur2 when size_t is 4 or 8 bytes so this is
// not the actual hash.
// https://github.com/gcc-mirror/gcc/blob/master/libstdc%2B%2B-v3/libsupc%2B%2B/hash_bytes.cc
inline size_t hash(const void* ptr, int32_t len, int32_t seed) {
  size_t hash = seed;
  const char* cptr = reinterpret_cast<const char*>(ptr);
  for (; len; --len) {
    hash = (hash * 131) + *cptr++;
  }
  return hash;
}

// Key that references external memory
struct TestKey {
  const char* ptr;
  int32_t len;
  size_t hash_val;

  bool operator==(const TestKey& other) const {
    return len == other.len && memcmp(ptr, other.ptr, len) == 0;
  }

  TestKey() = default;
  TestKey(const char* ptr, int32_t len)
    : ptr(ptr), len(len), hash_val(hash(ptr, len, 0)) {}
};

struct Request {
  std::string key;
  int64_t value;
  TestKey test_key;

  template<typename K>
  const K& get_key() const;

  Request() = default;
  Request(std::string_view k, int64_t v, const char* ext_ptr)
    : key(k), value(v), test_key(ext_ptr, ext_ptr == nullptr ? 0 : k.size()) {
  }
};

// Utility to track all allocations and free them at once.
class MemoryPool {
public:
  inline void* allocate(int32_t size) {
    void* mem = malloc(size);
    _ptrs.push_back(mem);
    return mem;
  }

  inline char* allocate_and_copy(std::string_view s) {
    void* mem = malloc(s.size());
    memcpy(mem, s.data(), s.size());
    _ptrs.push_back(mem);
    return reinterpret_cast<char*>(mem);
  }

  inline void free() {
    for (void* ptr: _ptrs) {
      ::free(ptr);
    }
    _ptrs.clear();
  }

private:
  std::vector<void*> _ptrs;
};

class Trace {
public:
  // Returns nullptr for end
  virtual const Request* next() = 0;

  // Resets the trace to the beginning
  virtual void Reset() = 0;

  virtual ~Trace();
};

class FixedTrace : public Trace {
public:
  FixedTrace(const std::vector<Request>& trace);

  // Adds these requests to the end of the trace
  void Add(const std::vector<Request>& trace);

  virtual const Request* next() {
    if (_idx >= _requests.size()) {
      return nullptr;
    }
    return &_requests[_idx++];
  }

  virtual void Reset() { _idx = 0; }

private:
  std::vector<Request> _requests;
  int _idx = 0;
};

class InterleavdTrace : public Trace {
public:
  void Add(Trace* trace);

  // Randomly picks a value of one of the underlying traces
  virtual const Request* next();
  virtual void Reset();

private:
  std::vector<Trace*> _traces;
  std::vector<Trace*> _active_traces;
};

// Trace
class TraceReader : public Trace {
public:
  TraceReader(std::string& fname, int64_t limit = 0, MemoryPool* pool = nullptr)
      : _file(fname), _file_name(fname), _limit{limit}, _count{0}, _r{}, _pool(pool) {}

  virtual void Reset() {
    _file.close();
    _file.clear();
    _file.open(_file_name);
    _count = 0;
  }

  virtual const Request* next() {
    std::string line;
    if ((_limit == 0 || _count < _limit) && std::getline(_file, line)) {
      std::stringstream l(line);
      l >> _r.key >> _r.value;
      if (_pool != nullptr) {
        char* ext = _pool->allocate_and_copy(_r.key);
        _r.test_key = TestKey(ext, _r.key.size());
      }
      _count++;
      return &_r;
    } else {
      return nullptr;
    }
  }

private:
  std::ifstream _file;
  std::string _file_name;
  int64_t _limit;
  int64_t _count;
  Request _r;
  MemoryPool* _pool;
};

class Zipfian {
public:
  // Initializes Zipfian generator for [1, n]
  Zipfian(int64_t n, double alpha);
  int64_t Gen();

private:
  std::vector<double> _sum_probs;
};

class TraceGen {
public:
  // Generate a trace with n copies of k,v
  static std::vector<Request> SameKeyTrace(int64_t n, std::string_view k,
                                           int64_t v, MemoryPool* pool = nullptr);

  // Generate a trace that cycles from 0 to k up to N values.
  // e.g. k = N generates all unique keys
  static std::vector<Request> CycleTrace(int64_t n, int64_t k, int64_t v,
                                         MemoryPool* pool = nullptr);

  // Generate a trace that follows a normal distribution with mean and stddev
  static std::vector<Request> NormalDistribution(int64_t n, double mean,
                                                 double stdev, int64_t v,
                                                 MemoryPool* pool = nullptr);

  // Generate a trace that follows a poison distribution with mean
  static std::vector<Request> PoissonDistribution(int64_t n, double mean,
                                                  int64_t v,
                                                  MemoryPool* pool = nullptr);

  // Generate a trace that follows a zipfian distribution with values [1, k]
  static std::vector<Request> ZipfianDistribution(int64_t n, int64_t k,
                                                  double alpha, int64_t v,
                                                  MemoryPool* pool = nullptr);

  static std::vector<Request> ZipfianDistribution(uint32_t seed, int64_t n,
                                                  int64_t k, double alpha,
                                                  int64_t v,
                                                  MemoryPool* pool = nullptr);
};

// Stamp out the key types reuqest supports
template <> inline const std::string& Request::get_key<std::string>() const {
  return key;
}

template <> inline const TestKey& Request::get_key<TestKey>() const {
  return test_key;
}

} // namespace cache

namespace std {
template <> struct hash<cache::TestKey> {
  std::size_t operator()(const cache::TestKey& k) const {
    return k.hash_val;
  }
};

}


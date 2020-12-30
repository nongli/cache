#pragma once

#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace cache {

struct Request {
  std::string key;
  int64_t value;

  Request() = default;
  Request(std::string_view k, int64_t v) : key(k), value(v) {}
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
  TraceReader(std::string& fname) : _file(fname), _file_name(fname), _r{} {}
  virtual void Reset() {
    _file.close();
    _file.clear();
    _file.open(_file_name);
  }
  virtual const Request* next() {
    std::string line;
    if (std::getline(_file, line)) {
      std::stringstream l(line);
      l >> _r.key >> _r.value;
      return &_r;
    } else {
      return NULL;
    }
  }

private:
  std::ifstream _file;
  std::string _file_name;
  Request _r;
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
                                           int64_t v);

  // Generate a trace that cycles from 0 to k up to N values.
  // e.g. k = N generates all unique keys
  static std::vector<Request> CycleTrace(int64_t n, int64_t k, int64_t v);

  // Generate a trace that follows a normal distribution with mean and stddev
  static std::vector<Request> NormalDistribution(int64_t n, double mean,
                                                 double stdev, int64_t v);

  // Generate a trace that follows a poison distribution with mean
  static std::vector<Request> PoissonDistribution(int64_t n, double mean,
                                                  int64_t v);

  // Generate a trace that follows a zipfian distribution with values [1, k]
  static std::vector<Request> ZipfianDistribution(int64_t n, int64_t k,
                                                  double alpha, int64_t v);

  static std::vector<Request> ZipfianDistribution(uint32_t seed, int64_t n,
                                                  int64_t k, double alpha,
                                                  int64_t v);
};

} // namespace cache

#pragma once

#include <string_view>
#include <string>
#include <vector>

namespace cache {

struct Request {
  std::string key;
  std::string value;

  Request(std::string_view k, std::string_view v) : key(k), value(v) {}
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
    if (_idx >= _requests.size()) return nullptr;
    return &_requests[_idx++];
  }

  virtual void Reset() { _idx = 0; }

 private:
  std::vector<Request> _requests;
  int _idx = 0;
};

class TraceGen {
 public:
  // Generate a trace with n copies of k,v
  static std::vector<Request> SameKeyTrace(
      int64_t n, std::string_view k, std::string_view v);

  // Generate a trace that cycles from 0 to k up to N values.
  // e.g. k = N generates all unique keys
  static std::vector<Request> CycleTrace(
      int64_t n, int64_t k, std::string_view v);
};

}

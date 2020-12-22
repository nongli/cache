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

class TraceGen {
 public:
  // Generate a trace with n copies of k,v
  static std::vector<Request> SameKeyTrace(
      int64_t n, std::string_view k, std::string_view v);
};

}

#pragma once

#include <map>
#include <string_view>

#include "cache/cache.h"

namespace cache {

class ResultCompare {
 public:
  void AddResult(std::string_view label, Stats stats);

  std::string Report(std::string_view title);

 private:
  std::map<std::string, Stats> _results;
};

}

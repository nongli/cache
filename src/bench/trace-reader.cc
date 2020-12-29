#include "gflags/gflags.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
void parse_trace(const std::string& trace) {
  std::ifstream tr(trace);
  std::string line;
  while (std::getline(tr, line)) {
    std::stringstream l(line);
    std::string key;
    int64_t size;
    l >> key >> size;
    std::cout << "key=" << key << " size=" << size << std::endl;
  }
}

DEFINE_string(trace, "", "Trace filename");
int main(int argc, char* argv[]) {
  gflags::SetUsageMessage("Trace reader");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_trace.empty()) {
    std::cerr << "No trace specified\n";
    std::exit(1);
  }
  parse_trace(FLAGS_trace);
}

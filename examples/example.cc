#include "include/arc.h"

#include <stdio.h>
#include <string>

using namespace cache;
using namespace std;

int main(int argc, char** argv) {
  AdaptiveCache<string, string> cache(10);
  cache.add_to_cache("Baby Yoda", make_shared<string>("Unknown Name"));
  const string& val = *cache.get("Baby Yoda");
  printf("%s\n", val.c_str());
  return 0;
}

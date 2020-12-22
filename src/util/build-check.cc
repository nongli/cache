#include <stdio.h>
#include "gflags/gflags.h"

DEFINE_int32(test, 0, "test");

int main(int argc, char** argv) {
  gflags::SetUsageMessage("Build check utility");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  printf("Done.\n");
  gflags::ShutDownCommandLineFlags();
  return 0;
}

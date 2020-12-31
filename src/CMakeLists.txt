if(PRINT_TRACE)
  add_definitions(-DPRINT_TRACE)
endif(PRINT_TRACE)

ADD_LIBRARY(Util
  util/compare.cc
  util/lock.cc
  util/table-printer.cc
  util/trace-gen.cc
)

FUNCTION(ADD_SIMPLE_EXECUTABLE EXE_NAME SRC)
  add_executable(${EXE_NAME} ${SRC})
  target_link_libraries(${EXE_NAME} ${LIBS} Util)
ENDFUNCTION()

ADD_SIMPLE_EXECUTABLE(build-check util/build-check.cc)
ADD_SIMPLE_EXECUTABLE(benchmark-flex-arc bench/bench.cc)
ADD_SIMPLE_EXECUTABLE(bench-cache bench/bench-cache.cc)
ADD_SIMPLE_EXECUTABLE(key-perf bench/key-perf.cc)
ADD_SIMPLE_EXECUTABLE(trace-reader bench/trace-reader.cc)

if(PRINT_TRACE)
  add_definitions(-DPRINT_TRACE)
endif(PRINT_TRACE)

ADD_LIBRARY(Cache
  cache.cc
)

ADD_LIBRARY(Util
  util/compare.cc
  util/lock.cc
  util/trace-gen.cc
)

FUNCTION(ADD_SIMPLE_EXECUTABLE EXE_NAME SRC)
  add_executable(${EXE_NAME} ${SRC})
  target_link_libraries(${EXE_NAME} ${LIBS} Cache Util)
ENDFUNCTION()

# Add the executables
# Something is weird with this build
#ADD_SIMPLE_EXECUTABLE(build-check util/build-check.cc)

ADD_LIBRARY(Cache
  cache.cc
)

ADD_LIBRARY(Util
  util/trace-gen.cc
)

FUNCTION(ADD_SIMPLE_EXECUTABLE EXE_NAME SRC)
  add_executable(${EXE_NAME} ${SRC})
  target_link_libraries(${EXE_NAME} ${LIBS})
ENDFUNCTION()

# Add the executables
ADD_SIMPLE_EXECUTABLE(build-check util/build-check.cc)

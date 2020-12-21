FUNCTION(ADD_SIMPLE_TEST TEST SRC)
  add_executable(${TEST} ${SRC})
  target_link_libraries(${TEST} ${LIBS})
  add_test(${TEST} ${EXECUTABLE_OUTPUT_PATH}/${TEST})
ENDFUNCTION()

ADD_SIMPLE_TEST(example-test example-test.cc)
ADD_SIMPLE_TEST(lru-test lru-test.cc)

add_executable(example-test example-test.cc)
add_executable(lru-test lru-test.cc)
target_link_libraries(example-test ${LIBS})
target_link_libraries(lru-test ${LIBS})

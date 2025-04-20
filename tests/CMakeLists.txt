enable_testing()

SET(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(INPUT ${TEST_DIR}/input)
set(EXPECTED ${TEST_DIR}/exp)
set(OUTPUT ${TEST_DIR}/.tmp)


add_executable(pptest pptest.cpp)
target_link_libraries(pptest preprocessor)
target_include_directories(pptest PRIVATE ${CMAKE_SOURCE_DIR})


add_test(
  NAME perf 
  COMMAND pptest ${INPUT}/sqliteall.txt
  --perf
)

add_test(
  NAME test 
  COMMAND pptest ${INPUT}/test.cpp
  -e ${EXPECTED}/test.cpp
  -o ${OUTPUT}/test.cpp
)

add_test(
  NAME various-macros
  COMMAND pptest ${INPUT}/various-macros.cpp
  -e ${EXPECTED}/various-macros.cpp
  -o ${OUTPUT}/various-macros.cpp
)

add_test(
  NAME va-args 
  COMMAND pptest ${INPUT}/va-args.cpp
  -e ${EXPECTED}/va-args.cpp
  -o ${OUTPUT}/va-args.cpp
)


add_test(
  NAME sqlite 
  COMMAND pptest ${INPUT}/sqliteall.txt
  -e ${EXPECTED}/sqliteall.txt
  -o ${OUTPUT}/sqliteall.txt
)


add_test(
  NAME spacing 
  COMMAND pptest ${INPUT}/spacing.cpp
  -e ${EXPECTED}/spacing.cpp
  -o ${OUTPUT}/spacing.cpp
)

add_test(
  NAME define-raw-comment 
  COMMAND pptest ${INPUT}/define-raw-comment.cpp
  -e ${EXPECTED}/define-raw-comment.cpp
  -o ${OUTPUT}/define-raw-comment.cpp
)

include("${CMAKE_CURRENT_LIST_DIR}/gitlab_ci.cmake")

# Read the files from the build directory.
ctest_read_custom_files("${CTEST_BINARY_DIRECTORY}")
include("${CMAKE_CURRENT_LIST_DIR}/ctest_submit_multi.cmake")

# Pick up from where the configure left off.
ctest_start(APPEND)

include(ProcessorCount)
ProcessorCount(nproc)

ctest_test(
  PARALLEL_LEVEL "${nproc}"
  RETURN_VALUE test_result)
ctest_submit_multi(PARTS Test)

if (test_result)
  message(FATAL_ERROR
    "Failed to test")
endif ()

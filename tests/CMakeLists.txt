enable_testing()

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.17.0
)
# MSVC-специфично, MinGW игнорирует
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Отключаем quick_exit (не мешает, иногда помогает)
target_compile_definitions(gtest       PUBLIC GTEST_HAS_QUICK_EXIT=0)
target_compile_definitions(gtest_main  PUBLIC GTEST_HAS_QUICK_EXIT=0)
target_compile_definitions(gmock       PUBLIC GTEST_HAS_QUICK_EXIT=0)
target_compile_definitions(gmock_main  PUBLIC GTEST_HAS_QUICK_EXIT=0)

add_executable(aip_tests
    test_unit_grid.cpp
    test_param_grid.cpp
    test_orch_stage1.cpp
    test_index_space.cpp
    test_fixed_string.cpp
    test_param_traits.cpp
    test_control_param.cpp
    test_uniform_range.cpp
    test_parallel_async.cpp
    test_piecewise_model.cpp
    test_constrained_line.cpp
    test_make_index_space.cpp
    test_enumeration_strategy.cpp
    test_orchestrator_strategy_enum.cpp
)

target_link_libraries(aip_tests PRIVATE
  aip
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(aip_tests)

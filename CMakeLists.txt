cmake_minimum_required(VERSION 3.20)
project(aip VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

add_library(aip INTERFACE)
add_library(aip::aip ALIAS aip)

target_include_directories(aip INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(aip INTERFACE cxx_std_20)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/aipConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/aipVersion.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/aipConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aip
)

install(TARGETS aip EXPORT aipTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT aipTargets
  FILE aipTargets.cmake
  NAMESPACE aip::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aip
)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/aipConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/aipConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aip
)

option(AIP_BUILD_EXAMPLES "Build examples" ON)
if (AIP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

option(AIP_BUILD_TESTS "Build tests" ON)
if(AIP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
find_package(Catch2 3 QUIET)
if (Catch2_FOUND)
  message(STATUS "Found Catch2 on system")
else()
  message(STATUS "Fetching Catch2")
  include(FetchContent)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG devel
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(Catch2)
  target_compile_options(Catch2 PRIVATE "-DCATCH_CONFIG_CONSOLE_WIDTH=200")
  list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
endif()

include(Catch)

if (COVERAGE)
  include(CodeCoverage)
endif()

add_library(railroad SHARED railroad.cpp)
target_link_libraries(railroad PUBLIC ta search mtl visualization)

add_library(fischer SHARED fischer.cpp)
target_link_libraries(fischer PUBLIC ta search)

add_executable(test_clock test_clock.cpp)
target_link_libraries(test_clock PRIVATE ta Catch2::Catch2WithMain)
catch_discover_tests(test_clock)

add_executable(testta test_ta.cpp test_ta_region.cpp test_ta_print.cpp test_ta_product.cpp)
target_link_libraries(testta PRIVATE ta PRIVATE Catch2::Catch2WithMain)
catch_discover_tests(testta)

add_executable(testinterval test_interval.cpp)
target_link_libraries(testinterval PRIVATE mtl PRIVATE Catch2::Catch2WithMain PRIVATE utilities)
catch_discover_tests(testinterval)

add_executable(testnumberutilities test_number_utilities.cpp)
target_link_libraries(testnumberutilities PRIVATE utilities PRIVATE Catch2::Catch2WithMain)
catch_discover_tests(testnumberutilities)

add_executable(testmtlformulae test_mtlFormula.cpp test_print_mtl_formula.cpp)
target_link_libraries(testmtlformulae PRIVATE mtl PRIVATE Catch2::Catch2WithMain)
catch_discover_tests(testmtlformulae)

add_executable(testata test_ata_formula.cpp test_ata.cpp test_print_ata.cpp test_print_ata_formula.cpp)
target_link_libraries(testata PRIVATE ta PRIVATE Catch2::Catch2WithMain)
catch_discover_tests(testata)

find_package(spdlog REQUIRED)
add_executable(test_mtl_ata_translation test_mtl_ata_translation.cpp)
target_link_libraries(test_mtl_ata_translation PRIVATE mtl_ata_translation PRIVATE Catch2::Catch2WithMain spdlog::spdlog)
catch_discover_tests(test_mtl_ata_translation)

add_executable(test_synchronous_product test_synchronous_product.cpp test_synchronous_product_print.cpp)
target_link_libraries(test_synchronous_product PRIVATE mtl_ata_translation search Catch2::Catch2WithMain)
catch_discover_tests(test_synchronous_product)

add_executable(test_create_controller test_create_controller.cpp)
target_link_libraries(test_create_controller PRIVATE railroad mtl_ata_translation search Catch2::Catch2WithMain)
if (TARGET visualization)
  message(STATUS "Building test_create_controller with visualization")
  target_link_libraries(test_create_controller PRIVATE visualization)
  target_compile_options(test_create_controller PRIVATE "-DHAVE_VISUALIZATION")
endif()
catch_discover_tests(test_create_controller)

add_executable(test_search test_search.cpp)
target_link_libraries(test_search PRIVATE mtl_ata_translation search visualization Catch2::Catch2WithMain)
catch_discover_tests(test_search)

add_executable(test_railroad test_railroad.cpp)
target_link_libraries(test_railroad PRIVATE railroad mtl_ata_translation search Catch2::Catch2WithMain)
catch_discover_tests(test_railroad)
if (TARGET visualization)
  message(STATUS "Building railroad with visualization")
  target_link_libraries(test_railroad PRIVATE visualization)
  target_compile_options(test_railroad PRIVATE "-DHAVE_VISUALIZATION")
endif()

add_executable(test_fischer test_fischer.cpp)
target_link_libraries(test_fischer PRIVATE fischer mtl_ata_translation search Catch2::Catch2WithMain)
catch_discover_tests(test_fischer)
if (TARGET visualization)
  message(STATUS "Building fischer with visualization")
  target_link_libraries(test_fischer PRIVATE visualization)
  target_compile_options(test_fischer PRIVATE "-DHAVE_VISUALIZATION")
endif()

add_executable(test_priority_thread_pool test_priority_thread_pool.cpp)
target_link_libraries(test_priority_thread_pool PRIVATE utilities Catch2::Catch2WithMain)
catch_discover_tests(test_priority_thread_pool)

#add_executable(test_tree_iteration test_tree_iteration.cpp)
#target_link_libraries(test_tree_iteration PRIVATE  search Catch2::Catch2WithMain)
#catch_discover_tests(test_tree_iteration)

add_executable(test_heuristics test_heuristics.cpp)
target_link_libraries(test_heuristics PRIVATE search Catch2::Catch2WithMain)
catch_discover_tests(test_heuristics)

find_package(Protobuf QUIET)

if (Protobuf_FOUND)

  add_executable(test_ta_proto test_ta_proto.cpp)
  target_link_libraries(test_ta_proto PRIVATE ta ta_proto Catch2::Catch2WithMain)
  catch_discover_tests(test_ta_proto)

  add_executable(test_mtl_proto test_mtl_proto.cpp)
  target_link_libraries(test_mtl_proto PRIVATE mtl mtl_proto Catch2::Catch2WithMain)
  catch_discover_tests(test_mtl_proto)

endif()

if (TARGET graphviz)
  add_executable(test_graphviz test_graphviz.cpp)
  target_link_libraries(test_graphviz PRIVATE graphviz Catch2::Catch2WithMain)
  catch_discover_tests(test_graphviz)
endif()

if (TARGET visualization)
  add_executable(test_tree_visualization test_tree_visualization.cpp)
  target_link_libraries(test_tree_visualization PRIVATE visualization Catch2::Catch2WithMain)
  catch_discover_tests(test_tree_visualization)

  find_package(fmt REQUIRED)
  add_executable(test_ta_visualization test_ta_visualization.cpp)
  target_link_libraries(test_ta_visualization PRIVATE visualization fmt::fmt Catch2::Catch2WithMain)
  catch_discover_tests(test_ta_visualization)
endif()

file(COPY data DESTINATION .)
add_executable(test_app test_app.cpp)
target_link_libraries(test_app PRIVATE app Catch2::Catch2WithMain)
catch_discover_tests(test_app WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

if (COVERAGE)
  # Depend on all targets in the current directory.
  get_property(test_targets DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTY BUILDSYSTEM_TARGETS)
  setup_target_for_coverage_lcov(
    NAME coverage
    EXECUTABLE ctest
    DEPENDENCIES ${test_targets}
    EXCLUDE "/usr/*"
    EXCLUDE "test/*"
    EXCLUDE "${FETCHCONTENT_BASE_DIR}/*"
  )
endif()

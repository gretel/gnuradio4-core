set(TESTS_BINARY_PATH ${CMAKE_CURRENT_BINARY_DIR})
set(TESTS_SOURCE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

configure_file(build_configure.hpp.in build_configure.hpp @ONLY)

function(add_app_test TEST_NAME)
  add_executable(${TEST_NAME} ${TEST_NAME}.cpp)
  setup_test(${TEST_NAME})
  target_link_libraries(${TEST_NAME} PRIVATE gnuradio-core gnuradio-blocklib-core gnuradio-plugin)
  add_dependencies(
    ${TEST_NAME}
    GoodBasePlugin
    good_math_plugin
    good_conversion_plugin
    bad_plugin)
endfunction()

set(_CORE_TESTS
    qa_buffer
    qa_AtomicBitset
    qa_AtomicRef
    qa_Value
    qa_ValueHelper
    qa_ComputeDomain
    qa_DataSet
    qa_DynamicPort
    qa_LifeCycle
    qa_MemoryAllocators
    qa_PmtCollections
    qa_Port
    qa_Profiler
    qa_Tensor
    qa_TensorMath
    qa_SVD
    qa_TriggerMatcher
    qa_YamlPmt
    qa_PortableTypeName
    qa_thread_affinity
    qa_thread_pool)

set(_STANDARD_BLOCK_INTEGRATION_TESTS
    qa_DynamicBlock
    qa_Block
    qa_BlockingSync
    qa_BlockModel
    qa_Scheduler
    qa_Graph
    qa_Merge
    qa_ManagedSubGraph
    qa_UnmanagedSubGraph
    qa_Tags
    qa_Messages
    qa_PerformanceMonitor)

if(GR4_ENABLE_BLOCKS)
  list(APPEND _CORE_TESTS ${_STANDARD_BLOCK_INTEGRATION_TESTS})
  set(GR_TEST_HELPER_LIBRARIES gr-basic gr-fileio gr-math gr-testing gr-filter)
else()
  message(STATUS "Skipping standard-block integration tests in core-only mode: ${_STANDARD_BLOCK_INTEGRATION_TESTS}")
endif()

foreach(TEST_NAME IN LISTS _CORE_TESTS)
  add_ut_test(${TEST_NAME})
endforeach()

add_ut_test(qa_SubGraphAssets)
set(_QA_SUBGRAPH_HTTP_PORT 18765)
target_compile_definitions(qa_SubGraphAssets PRIVATE HTTP_SERVER_PORT=${_QA_SUBGRAPH_HTTP_PORT})
if(EMSCRIPTEN)
  set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
  set(HTTP_SERVER_PORT ${_QA_SUBGRAPH_HTTP_PORT})
  configure_file(subgraph_assets_pre.js.in subgraph_assets_pre.js @ONLY)
  target_link_options(
    qa_SubGraphAssets
    PRIVATE
    --pre-js=${CMAKE_CURRENT_BINARY_DIR}/subgraph_assets_pre.js
    "SHELL:-s PROXY_TO_PTHREAD=1")
  set_property(
    TARGET qa_SubGraphAssets
    APPEND
    PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/subgraph_assets_pre.js)
else()
  target_link_libraries(qa_SubGraphAssets PRIVATE httplib::httplib)
endif()
if(GR_ENABLE_BLOCK_REGISTRY AND INTERNAL_ENABLE_BLOCK_PLUGINS)
  add_subdirectory(plugins)

  add_dependencies(
    qa_SubGraphAssets
    GoodBasePlugin
    good_math_plugin
    good_conversion_plugin
    bad_plugin)
endif()

if(GR4_ENABLE_BLOCKS AND GR_ENABLE_BLOCK_REGISTRY AND INTERNAL_ENABLE_BLOCK_PLUGINS)
  add_ut_test(qa_Settings)

  add_app_test(qa_grc)
  gr_generate_block_instantiations(qa_grc HEADERS CollectionTestBlocks.hpp)
  target_link_libraries(qa_grc PUBLIC gnuradio4::GrBasicBlocksShared gnuradio4::GrTestingBlocksShared)

  add_ut_test(qa_GraphMessages)
  target_link_libraries(qa_GraphMessages PUBLIC gnuradio4::GrBasicBlocksShared gnuradio4::GrTestingBlocksShared)

  add_ut_test(qa_SchedulerMessages)
  target_link_libraries(qa_SchedulerMessages PUBLIC gnuradio4::GrBasicBlocksShared gnuradio4::GrTestingBlocksShared)

  add_app_test(qa_plugins_test)
  target_link_libraries(qa_plugins_test PUBLIC gnuradio4::GrBasicBlocksShared gnuradio4::GrTestingBlocksShared)

  add_app_test(qa_plugin_schedulers_test)
  target_link_libraries(qa_plugin_schedulers_test PUBLIC gnuradio4::GrBasicBlocksShared
                                                         gnuradio4::GrTestingBlocksShared)
elseif(GR_ENABLE_BLOCK_REGISTRY AND INTERNAL_ENABLE_BLOCK_PLUGINS)
  message(STATUS "Skipping standard-block registry/message integration tests in core-only mode")
endif()

if(GR_USE_ADAPTIVE_CPP)
  add_executable(qa_adaptivecpp qa_adaptivecpp.cpp)

  target_compile_options(qa_adaptivecpp PRIVATE --acpp-targets=omp)
  target_link_options(qa_adaptivecpp PRIVATE --acpp-targets=omp)

  add_test(NAME qa_adaptivecpp COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:qa_adaptivecpp>)

  set_tests_properties(
    qa_adaptivecpp
    PROPERTIES LABELS
               "sycl;adaptivecpp"
               TIMEOUT
               60)
endif()

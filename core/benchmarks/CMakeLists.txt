if(NOT EMSCRIPTEN)
  function(add_gr_benchmark BM_NAME)
    add_benchmark(${BM_NAME})
    target_link_libraries(
      ${BM_NAME}
      PRIVATE gnuradio-options
              gnuradio-core
              gnuradio-blocklib-core
              gnuradio-plugin
              ut
              ut-benchmark)
    if(GR4_ENABLE_BLOCKS)
      target_link_libraries(${BM_NAME} PRIVATE gr-basic gr-fileio gr-math gr-testing gr-filter)
    endif()
    if(EMSCRIPTEN) # performance benchmarks should be optimised
      target_compile_options(${BM_NAME} PRIVATE -O3)
    else()
      target_compile_options(${BM_NAME} PRIVATE -O3 -march=native)
    endif()

    target_include_directories(${BM_NAME} PRIVATE "../test")
  endfunction()

  add_gr_benchmark(bm_Buffer)
  add_gr_benchmark(bm_HistoryBuffer)
  add_gr_benchmark(bm_Profiler)
  add_gr_benchmark(bm_tensor)
  add_gr_benchmark(bm_portLimits)
  if(GR4_ENABLE_BLOCKS AND GR4_ENABLE_ALGORITHM)
    add_gr_benchmark(bm_Scheduler)
    add_gr_benchmark(bm_MergeApi)
    add_gr_benchmark(bm_sync)
  else()
    message(STATUS "Skipping standard-block/algorithm benchmarks in core-only mode")
  endif()
endif()

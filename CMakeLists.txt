cmake_minimum_required(VERSION 3.10)
project(ccalg LANGUAGES C CXX)

# ── include directory + library target ────────────────────────────────────
set(CCALG_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")

# header-only library — downstream uses target_link_libraries(myapp ccalg)
add_library(ccalg INTERFACE)
target_include_directories(ccalg INTERFACE ${CCALG_INCLUDE_DIR})

# ── common compile settings ──────────────────────────────────────────────
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/W4 /wd4996)
else()
  add_compile_options(-Wall -Wextra -Wno-missing-field-initializers)
endif()

# ── sanitizer support ────────────────────────────────────────────────────
option(CCALG_SANITIZE "Enable AddressSanitizer + UndefinedBehaviorSanitizer" ON)

set(_ccalg_sanitize_flags "")
if(CCALG_SANITIZE)
  if(MINGW OR MSYS)
    # MinGW does not ship ASan/UBSan runtime libraries.
    message(STATUS "CCALG_SANITIZE: MinGW/MSYS — sanitizers unavailable, skipping")
  elseif(MSVC)
    # MSVC ASAN is available from VS 16.9+, but ClangCL (clang-cl) cannot
    # link ASan with debug CRT (/MDd).  Only enable for plain MSVC.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
      set(_ccalg_sanitize_flags /fsanitize=address)
    endif()
  else()
    set(_ccalg_sanitize_flags -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined)
  endif()
endif()

# ── ctest integration ────────────────────────────────────────────────────
enable_testing()

# ── test targets (C) ─────────────────────────────────────────────────────
macro(add_ccalg_test name)
  add_executable(${name} tests/${name}.c)
  target_include_directories(${name} PRIVATE ${CCALG_INCLUDE_DIR})
  if(_ccalg_sanitize_flags)
    target_compile_options(${name} PRIVATE ${_ccalg_sanitize_flags})
    target_link_options(${name} PRIVATE ${_ccalg_sanitize_flags})
  endif()
  add_test(
    NAME        ${name}
    COMMAND     $<TARGET_FILE:${name}>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )
  set_tests_properties(${name} PROPERTIES
    LABELS "unit"
    PASS_REGULAR_EXPRESSION "0 failed"
    FAIL_REGULAR_EXPRESSION "[1-9][0-9]* failed"
    TIMEOUT 30
  )
endmacro()

add_ccalg_test(test_ccmap)
add_ccalg_test(test_cchashmap)
add_ccalg_test(test_cchashmap_fp)
add_ccalg_test(test_cclist)
add_ccalg_test(test_cclink)
add_ccalg_test(test_ccheap)
add_ccalg_test(test_ccvector)
add_ccalg_test(test_ccflatmap)
add_ccalg_test(test_cctreap)
add_ccalg_test(test_ccrandom)
add_ccalg_test(test_ccunicode)
add_ccalg_test(test_ccunicode_verify)
add_ccalg_test(test_ccshuffle)
add_ccalg_test(test_ccheap_update)
add_ccalg_test(test_ccbi)
add_ccalg_test(test_ccbits)

# ── benchmark targets (C++) ──────────────────────────────────────────────
macro(add_ccalg_bench name)
  add_executable(${name} bench/${name}.cpp)
  target_include_directories(${name} PRIVATE ${CCALG_INCLUDE_DIR})
  # MSVC Debug adds /RTC1 which conflicts with /O2; skip explicit -O2 for MSVC
  target_compile_options(${name} PRIVATE
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O2>
  )
endmacro()

add_ccalg_bench(bench_ccmap)
add_ccalg_bench(bench_cchashmap)
add_ccalg_bench(bench_cclist)
add_ccalg_bench(bench_cclink)
add_ccalg_bench(bench_ccheap)
add_ccalg_bench(bench_ccvector)
add_ccalg_bench(bench_ccflatmap)
add_ccalg_bench(bench_cctreap)
add_ccalg_bench(bench_ccrandom)
add_ccalg_bench(bench_ccunicode)
add_ccalg_bench(bench_ccshuffle)
add_ccalg_bench(bench_ccbits)
add_ccalg_bench(bench_ccbi)

# prefetch benchmark — needs two compilations (with / without CCMAP_PREFETCH)
add_executable(bench_ccmap_prefetch      bench/bench_ccmap_prefetch.cpp)
add_executable(bench_ccmap_prefetch_on   bench/bench_ccmap_prefetch.cpp)
target_include_directories(bench_ccmap_prefetch    PRIVATE ${CCALG_INCLUDE_DIR})
target_include_directories(bench_ccmap_prefetch_on PRIVATE ${CCALG_INCLUDE_DIR})
target_compile_options(bench_ccmap_prefetch    PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O2>)
target_compile_options(bench_ccmap_prefetch_on PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O2> -DCCMAP_PREFETCH)

# ── convenience targets ──────────────────────────────────────────────────

# `cmake --build build --target check` — build + run all tests
add_custom_target(check
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L unit
  DEPENDS test_ccmap test_cchashmap test_cchashmap_fp test_cclink test_cclist test_ccheap test_ccvector test_ccflatmap test_cctreap test_ccrandom test_ccunicode test_ccunicode_verify test_ccshuffle test_ccheap_update test_ccbi test_ccbits
  USES_TERMINAL
  COMMENT "Running all unit tests"
)

# `cmake --build build --target bench` — build + run all benchmarks
add_custom_target(bench
  COMMAND ${CMAKE_COMMAND} -E echo "=== ccmap vs std::map ==="
  COMMAND $<TARGET_FILE:bench_ccmap>
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "=== cchashmap vs std::unordered_map ==="
  COMMAND $<TARGET_FILE:bench_cchashmap>
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "=== cclink vs std::forward_list ==="
  COMMAND $<TARGET_FILE:bench_cclink>
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "=== cclist vs std::list ==="
  COMMAND $<TARGET_FILE:bench_cclist>
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "=== ccheap vs std::priority_queue ==="
  COMMAND $<TARGET_FILE:bench_ccheap>
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "=== ccvector vs std::vector ==="
  COMMAND $<TARGET_FILE:bench_ccvector>
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "=== cctreap vs std::map ==="
  COMMAND $<TARGET_FILE:bench_cctreap>
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "=== ccflatmap vs std::map ==="
  COMMAND $<TARGET_FILE:bench_ccflatmap>
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "=== ccrandom128 / ccrandom256 vs std::mt19937 ==="
  COMMAND $<TARGET_FILE:bench_ccrandom>
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "=== ccunicode encode/decode throughput ==="
  COMMAND $<TARGET_FILE:bench_ccunicode>
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "=== ccmap prefetch OFF ==="
  COMMAND $<TARGET_FILE:bench_ccmap_prefetch>
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "=== ccmap prefetch ON  ==="
  COMMAND $<TARGET_FILE:bench_ccmap_prefetch_on>
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "=== ccshuffle vs std::shuffle ==="
  COMMAND $<TARGET_FILE:bench_ccshuffle>
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "=== ccbits bit primitives ==="
  COMMAND $<TARGET_FILE:bench_ccbits>
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "=== ccbi big-integer arithmetic ==="
  COMMAND $<TARGET_FILE:bench_ccbi>
  DEPENDS bench_ccmap bench_cchashmap bench_cclink bench_cclist bench_ccheap bench_ccvector bench_ccflatmap bench_cctreap bench_ccrandom bench_ccunicode bench_ccmap_prefetch bench_ccmap_prefetch_on bench_ccshuffle bench_ccbits bench_ccbi
  USES_TERMINAL
  COMMENT "Running all benchmarks"
)

# ── docs → HTML ──────────────────────────────────────────────────────────
find_program(PYTHON3 python3)
if(PYTHON3)
  add_custom_target(docs-html
    COMMAND ${PYTHON3} "${CMAKE_SOURCE_DIR}/scripts/md2html.py"
            "${CMAKE_SOURCE_DIR}/docs" "${CMAKE_CURRENT_BINARY_DIR}/docs-html"
    COMMENT "Generating HTML docs from docs/*.md"
  )
else()
  message(STATUS "python3 not found — `docs-html` target disabled")
endif()

# ── install ──────────────────────────────────────────────────────────────
include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/ccalg"
        FILES_MATCHING PATTERN "*.h")
install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/ccalg"
        OPTIONAL)
install(FILES README.md DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/ccalg"
        OPTIONAL)

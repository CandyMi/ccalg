-- premake5.lua — build configuration for alg
-- Usage: premake5 gmake2  (or vs2022 / xcode4)

workspace "alg"
  configurations { "Release", "Debug" }
  location "build"

  includedirs { "include" }

  -- ── tests (C) ─────────────────────────────────────────────────────────
  project "alg_tests"
    kind "ConsoleApp"
    language "C"
    cdialect "C99"
    targetdir "bin/%{cfg.buildcfg}"

    files { "tests/test_*.c" }

    filter "configurations:Debug"
      symbols "On"
      buildoptions { "-Wall", "-Wextra", "-Wno-missing-field-initializers" }

    filter "configurations:Release"
      optimize "On"
      buildoptions { "-Wall", "-Wextra", "-Wno-missing-field-initializers" }

  -- ── benchmarks (C++) ──────────────────────────────────────────────────
  project "alg_bench"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++11"
    targetdir "bin/%{cfg.buildcfg}"

    files { "bench/bench_*.cpp" }

    filter "configurations:Debug"
      symbols "On"
      buildoptions { "-Wall", "-O2" }

    filter "configurations:Release"
      optimize "On"
      buildoptions { "-Wall", "-O2" }

  filter {}

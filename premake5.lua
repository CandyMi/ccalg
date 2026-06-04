-- premake5.lua — build configuration for alg
-- Usage: premake5 gmake2  (or vs2022 / xcode4)
---@diagnostic disable: undefined-global

workspace "alg"
  configurations { "Release", "Debug" }
  location "build"

  includedirs { "include" }

  filter "configurations:Debug"
    symbols "On"

  filter "configurations:Release"
    optimize "On"

  filter {}

  -- ── tests (C) ─────────────────────────────────────────────────────────
  local tests = {
    "test_ccmap", "test_cchashmap", "test_cclink", "test_cclist", "test_ccheap"
  }
  for _, name in ipairs(tests) do
    project(name)
      kind "ConsoleApp"
      language "C"
      cdialect "C99"
      targetdir "build/bin/%{cfg.buildcfg}"
      files { "tests/" .. name .. ".c" }
      filter "toolset:gcc or toolset:clang"
        buildoptions { "-Wall", "-Wextra", "-Wno-missing-field-initializers" }
      filter {}
  end

  -- ── benchmarks (C++) ──────────────────────────────────────────────────
  local benches = {
    "bench_ccmap", "bench_cchashmap", "bench_cclink", "bench_cclist", "bench_ccheap"
  }
  for _, name in ipairs(benches) do
    project(name)
      kind "ConsoleApp"
      language "C++"
      cppdialect "C++11"
      targetdir "build/bin/%{cfg.buildcfg}"
      files { "bench/" .. name .. ".cpp" }
      filter "toolset:gcc or toolset:clang"
        buildoptions { "-Wall", "-O2" }
      filter {}
  end

  -- ── install (via script) ──────────────────────────────────────────────
  --  PREFIX=/usr/local sh scripts/install.sh

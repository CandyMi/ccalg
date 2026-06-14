-- premake5.lua — build configuration for ccalg
-- Usage: premake5 gmake2  (or vs2022 / xcode4)
---@diagnostic disable: undefined-field
---@diagnostic disable: undefined-global

workspace "ccalg"
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
    "test_ccmap", "test_cchashmap", "test_cclink", "test_cclist",
    "test_ccheap", "test_ccvector", "test_ccflatmap", "test_cctreap",
    "test_ccrandom", "test_ccunicode", "test_ccunicode_verify"
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
      filter "toolset:msc"
        buildoptions { "/W4", "/wd4996" }
      filter {}
  end

  -- ── benchmarks (C++) ──────────────────────────────────────────────────
  local benches = {
    "bench_ccmap", "bench_cchashmap", "bench_cclink", "bench_cclist",
    "bench_ccheap", "bench_ccvector", "bench_ccflatmap", "bench_cctreap",
    "bench_ccrandom", "bench_ccunicode"
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
      filter "toolset:msc"
        buildoptions { "/W4", "/O2" }
      filter {}
  end

  -- prefetch benchmark — needs two compilations (with / without CCMAP_PREFETCH)
  for _, suffix in ipairs({ "", "_on" }) do
    local defs = (suffix == "_on") and { "CCMAP_PREFETCH" } or {}
    project("bench_ccmap_prefetch" .. suffix)
      kind "ConsoleApp"
      language "C++"
      cppdialect "C++11"
      targetdir "build/bin/%{cfg.buildcfg}"
      files { "bench/bench_ccmap_prefetch.cpp" }
      defines(defs)
      filter "toolset:gcc or toolset:clang"
        buildoptions { "-Wall", "-O2" }
      filter "toolset:msc"
        buildoptions { "/W4", "/O2" }
      filter {}
  end

  -- ── `check` target — build & run all tests ────────────────────────────
  project("check")
    kind "Makefile"
    dependson (tests)
    -- on gmake2: `make check` builds all tests then runs each one
    local cmds = { 'echo "Running all unit tests"' }
    for _, t in ipairs(tests) do
      table.insert(cmds, '%{cfg.buildcfg}/' .. t)
    end
    postbuildcommands (cmds)

  -- ── `bench` target — build & run all benchmarks ───────────────────────
  project("bench")
    kind "Makefile"
    dependson (benches)
    postbuildcommands {
      'echo ""',
      'echo "=== ccmap vs std::map ==="',
      '%{cfg.buildcfg}/bench_ccmap',
      'echo ""',
      'echo "=== cchashmap vs std::unordered_map ==="',
      '%{cfg.buildcfg}/bench_cchashmap',
      'echo ""',
      'echo "=== cclink vs std::forward_list ==="',
      '%{cfg.buildcfg}/bench_cclink',
      'echo ""',
      'echo "=== cclist vs std::list ==="',
      '%{cfg.buildcfg}/bench_cclist',
      'echo ""',
      'echo "=== ccheap vs std::priority_queue ==="',
      '%{cfg.buildcfg}/bench_ccheap',
      'echo ""',
      'echo "=== ccvector vs std::vector ==="',
      '%{cfg.buildcfg}/bench_ccvector',
      'echo ""',
      'echo "=== ccflatmap vs std::map ==="',
      '%{cfg.buildcfg}/bench_ccflatmap',
      'echo ""',
      'echo "=== ccrandom128 / ccrandom256 vs std::mt19937 ==="',
      '%{cfg.buildcfg}/bench_ccrandom',
      'echo ""',
      'echo "=== ccunicode encode/decode throughput ==="',
      '%{cfg.buildcfg}/bench_ccunicode',
      'echo ""',
      'echo "=== cctreap vs std::map ==="',
      '%{cfg.buildcfg}/bench_cctreap',
      'echo ""',
      'echo "=== ccmap prefetch OFF ==="',
      '%{cfg.buildcfg}/bench_ccmap_prefetch',
      'echo ""',
      'echo "=== ccmap prefetch ON  ==="',
      '%{cfg.buildcfg}/bench_ccmap_prefetch_on',
      'echo ""',
    }

  -- ── `docs-html` target ────────────────────────────────────────────────
  project("docs-html")
    kind "Makefile"
    local python = os.host() == "windows" and "python" or "python3"
    postbuildcommands {
      python .. ' scripts/md2html.py docs "%{wks.location}/docs-html"'
    }

  -- ── install ───────────────────────────────────────────────────────────
  -- premake5 install: headers + license + readme
  --   Usage: premake5 install --prefix=/usr/local
  newaction {
    trigger     = "install",
    description = "Install headers and documentation",
    execute     = function()
      local prefix = _OPTIONS["prefix"] or "/usr/local"
      local incdir  = prefix .. "/include/ccalg"
      local docdir  = prefix .. "/share/doc/ccalg"

      -- install headers
      os.mkdir(incdir)
      for _, h in ipairs(os.matchfiles("include/*.h")) do
        os.copyfile(h, incdir .. "/" .. path.getname(h))
        print("  install  " .. incdir .. "/" .. path.getname(h))
      end

      -- install license + readme (optional)
      os.mkdir(docdir)
      for _, f in ipairs({ "LICENSE", "README.md" }) do
        if os.isfile(f) then
          os.copyfile(f, docdir .. "/" .. f)
          print("  install  " .. docdir .. "/" .. f)
        end
      end
    end
  }



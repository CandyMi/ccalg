#!/usr/bin/env python3
"""Convert docs/*.md → build/docs-html/*.html with sidebar + responsive layout.

Requires: pip install markdown
Usage:    python3 scripts/md2html.py docs/ build/docs-html/
          cmake --build build --target docs-html
"""
import sys, os, re
from markdown import markdown

CUSTOM_CSS = """
/* ── sidebar collapse (spanning multiple elements — CSS is cleaner than JS class toggling) ── */
.sidebar-collapsed .sidebar { width: 44px !important; padding: 8px 4px !important; }
.sidebar-collapsed .sidebar h2,
.sidebar-collapsed .sidebar nav { display: none; }
.sidebar-collapsed .main { margin-left: 44px !important; max-width: none; }
@media (max-width: 767px) {
  .sidebar-collapsed .sidebar { width: 100% !important; padding: 1rem !important; }
  .sidebar-collapsed .sidebar h2,
  .sidebar-collapsed .sidebar nav { display: block; }
  .sidebar-collapsed .main { margin-left: 0 !important; }
}
/* ── TOC collapse ── */
.toc-collapsed .toc { display: none; }
.toc-collapsed .toc-icon { transform: rotate(180deg); }
/* ── TOC scrollbar ── */
.toc-links { scrollbar-width: none; }
.toc-links::-webkit-scrollbar { display: none; }
/* ── TOC active link (Tailwind prose overrides need !important) ── */
.toc a.active { color: #2563eb !important; border-left-color: #2563eb !important; font-weight: 500; }
@media (prefers-color-scheme: dark) {
  .toc a.active { color: #60a5fa !important; border-left-color: #60a5fa !important; }
}
/* ── citation links inside prose ── */
a.citation {
  text-decoration: none; color: inherit; font-weight: 500;
  border: 1px solid #d1d5db; border-radius: 3px;
  padding: 0 0.25em; font-size: 0.9em; vertical-align: super;
  line-height: 1; transition: background 0.15s;
}
a.citation:hover { background: #2563eb; color: #fff; border-color: #2563eb; }
@media (prefers-color-scheme: dark) {
  a.citation { border-color: #475569; }
  a.citation:hover { background: #3b82f6; color: #fff; border-color: #3b82f6; }
}
li[id^="ref-"] { scroll-margin-top: 80px; }
/* ── responsive: hide TOC on narrow screens ── */
@media (max-width: 1100px) {
  .toc, .toc-toggle { display: none !important; }
}
"""

PAGES = [
    ("index",             "基本介绍", "ccalg - 高性能数据结构与算法实现"),
    ("getting-started",   "快速开始", "ccalg - "),
    ("api-reference",     "接口参考", "ccalg - "),
    ("algorithms",        "算法原理", "ccalg - "),
    ("building",          "构建指南", "ccalg - "),
    ("benchmarks",        "性能基准", "ccalg - "),
    ("thread-safety",     "线程安全", "ccalg - "),
    ("random",            "伪随机数", "ccalg - "),
    ("contributing",      "参与贡献", "ccalg - "),
]

items = [
  '一、',
  '二、',
  '三、',
  '四、',
  '五、',
  '六、',
  '七、',
  '八、',
  '九、',
  '十、',
]

def nav_html(active_slug):
    links = []
    i = 0
    base = (
        'block py-2 px-3 rounded-md text-sm font-medium no-underline '
        'text-teal-700 dark:text-teal-300 '
        'hover:bg-blue-50 hover:text-blue-700 '
        'dark:hover:bg-blue-950 dark:hover:text-blue-300 '
        'transition-colors duration-150'
    )
    active_cls = ' bg-blue-600 text-white hover:bg-blue-700 hover:text-white dark:bg-blue-600 dark:text-white'
    for slug, name, title in PAGES:
        cls = base + (active_cls if slug == active_slug else '')
        links.append(f'      <a href="{slug}.html" class="{cls}">{items[i] + name}</a>')
        i = i + 1
    return "\n".join(links)


def toc_html(body_html):
    """Generate TOC links from rendered HTML h2/h3 elements."""
    items = []
    base = (
        'block py-1 no-underline border-l-2 border-transparent '
        'text-teal-500 dark:text-teal-400 '
        'hover:text-blue-600 dark:hover:text-blue-400 '
        'transition-colors duration-150 leading-snug'
    )
    for m in re.finditer(r'<(h[23])(?:\s+id="([^"]*)")?[^>]*>(.+?)</\1>', body_html):
        tag, aid, title = m.group(1), m.group(2), m.group(3).strip()
        if not aid or title.lower() == 'alg docs':
            continue
        indent = ' pl-3 text-xs' if tag == 'h3' else ' pl-2 text-[0.82em]'
        items.append(f'<a href="#{aid}" class="{base}{indent}" data-target="{aid}">{title}</a>')
    return '\n'.join(items)


def process_citations(body_html):
    """Convert `[N]` citation markers to links, and anchor reference list items.

    Steps:
      1. Find `<h2>参考文献</h2>` heading, then the `<ol>` immediately following it.
         Number each `<li>` with `id="ref-1"`, `id="ref-2"`, …
      2. In all non-code/non-pre text, replace standalone `[N]` (N=1..9) with
         `<a href="#ref-N" class="citation">[N]</a>`.

    Returns the modified HTML string.
    """
    # ── Step 1: anchor reference list items ──────────────────────────────
    ref_pat = re.compile(
        r'(<h2[^>]*>参考文献</h2>\s*)<ol>\s*(.*?)\s*</ol>',
        re.DOTALL
    )
    def anchor_refs(m):
        prefix = m.group(1)
        inner  = m.group(2)
        idx    = 1
        def tag_li(m_li):
            nonlocal idx
            li = m_li.group(0)
            tagged = li.replace('<li>', f'<li id="ref-{idx}">', 1) if '<li>' in li else li
            tagged = tagged.replace('<li ', f'<li id="ref-{idx}" ', 1) if '<li ' in li and 'id=' not in li else tagged
            idx += 1
            return tagged
        inner_tagged = re.sub(r'<li[^>]*>', tag_li, inner)
        return prefix + '<ol>' + inner_tagged + '</ol>'

    body_html = ref_pat.sub(anchor_refs, body_html)

    # ── Step 2: link citation markers in non-code text ───────────────────
    def link_citations(segment):
        # Replace [N] where N is 1-9, but not inside identifiers (ASCII \w only,
        # because Python 3's default \w matches CJK — we want 重构[2] to match).
        return re.sub(r'(?<![a-zA-Z0-9_])\[([1-9])\](?![a-zA-Z0-9_])', r'<a href="#ref-\1" class="citation">[\1]</a>', segment)

    parts = re.split(r'(<pre[^>]*>.*?</pre>|<code[^>]*>.*?</code>)', body_html, flags=re.DOTALL)
    for i in range(0, len(parts), 2):
        parts[i] = link_citations(parts[i])
    body_html = ''.join(parts)

    return body_html


def main():
    src_dir = sys.argv[1] if len(sys.argv) > 1 else "docs"
    out_dir = sys.argv[2] if len(sys.argv) > 2 else "build/docs-html"
    os.makedirs(out_dir, exist_ok=True)

    for slug, name, title in PAGES:
        src = os.path.join(src_dir, slug + ".md")
        dst = os.path.join(out_dir, slug + ".html")
        if not os.path.isfile(src):
            print(f"  skip {src} (not found)")
            continue

        raw = open(src, encoding="utf-8").read()
        body = markdown(raw, extensions=["fenced_code", "tables", "toc"],
                        extension_configs={"toc": {"permalink": False, "baselevel": 1}})
        body = process_citations(body)
        toc = toc_html(body)
        nav = nav_html(slug)

        baidu = '<meta name="baidu-site-verification" content="codeva-IOubXz4IUX" />\n' if slug == "index" else ""
        html = f"""<!DOCTYPE html>
<html lang="zh-CN" class="scroll-smooth">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
{baidu}<title>{ slug == 'index' and title or (title + name)}</title>
<script src="https://cdn.tailwindcss.com?plugins=typography"></script>
<script>
tailwind.config = {{
  darkMode: 'media',
  theme: {{
    extend: {{
      typography: {{
        DEFAULT: {{
          css: {{
            maxWidth: 'none',
            'code::before': {{ content: 'none' }},
            'code::after': {{ content: 'none' }},
            'blockquote p:first-of-type::before': {{ content: 'none' }},
            'blockquote p:last-of-type::after': {{ content: 'none' }},
          }}
        }}
      }}
    }}
  }}
}}
</script>
<style>{CUSTOM_CSS}</style>
</head>
<body class="flex min-h-screen bg-white dark:bg-teal-950 text-teal-900 dark:text-teal-100 font-sans antialiased">
<aside class="sidebar w-full h-auto static border-b border-teal-200 dark:border-teal-800 p-4
              md:fixed md:left-0 md:top-0 md:h-full md:w-[220px] md:border-r md:border-b-0 md:p-8
              bg-teal-50 dark:bg-teal-900 overflow-hidden hover:overflow-y-auto z-30
              transition-[width,padding] duration-300"
       id="sidebar">
  <button class="block w-8 h-8 ml-auto border border-teal-300 dark:border-teal-700
                 rounded-md bg-white dark:bg-teal-800 text-teal-600 dark:text-teal-300
                 text-lg cursor-pointer leading-7 text-center
                 hover:bg-blue-600 hover:text-white hover:border-blue-600
                 dark:hover:bg-blue-600 dark:hover:text-white
                 transition-colors duration-200 flex-shrink-0"
          id="toggleSidebar" title="收起侧边栏">☰</button>
  <h2 class="text-2xl font-bold mt-3 mb-4 pb-2 border-b border-teal-200 dark:border-teal-700
             text-teal-800 dark:text-teal-100 text-center">目录</h2>
  <nav class="flex flex-wrap gap-1 md:flex-col md:gap-0.5">
{nav}
  </nav>
</aside>
<main class="main flex-1 px-4 py-6 md:ml-[220px] md:px-10 md:py-8 max-w-[900px]
             transition-[margin] duration-300" id="main">
  <article class="prose prose-slate dark:prose-invert max-w-none
                  prose-headings:scroll-mt-20
                  prose-pre:rounded-lg prose-pre:border prose-pre:border-teal-200
                  dark:prose-pre:border-teal-800
                  prose-img:rounded-lg
                  prose-table:block prose-table:overflow-x-auto
                  md:prose-table:table">
{body}
  </article>
  <footer class="mt-12 pt-4 border-t border-teal-200 dark:border-teal-800
                 text-sm text-teal-500 dark:text-teal-400">
    <p>ccalg — header-only C data-structure library · BSD 3-Clause ·
    <a href="https://github.com/CandyMi/ccalg" class="text-blue-600 dark:text-blue-400 hover:underline">GitHub</a></p>
  </footer>
</main>
<button class="toc-toggle fixed right-3 top-[50px] z-[101] px-2.5 py-1
               border border-teal-300 dark:border-teal-700 rounded-t-md
               bg-white dark:bg-teal-900 text-teal-600 dark:text-teal-300
               text-sm cursor-pointer whitespace-nowrap
               hover:bg-blue-600 hover:text-white hover:border-blue-600
               dark:hover:bg-blue-600 dark:hover:text-white
               transition-colors duration-200"
        id="tocToggle" title="折叠目录">点击收起 <span class="toc-icon inline-block transition-transform duration-300">▶</span></button>
<nav class="toc fixed right-3 top-20 w-[210px] max-h-[calc(100vh-120px)]
            flex flex-col text-sm z-[100] bg-white dark:bg-teal-900
            border border-teal-200 dark:border-teal-800 rounded-xl
            shadow-lg shadow-teal-200/50 dark:shadow-teal-950/50
            opacity-90 hover:opacity-100 transition-opacity duration-200"
     id="toc">
  <div class="flex justify-between items-center flex-shrink-0 font-semibold
              text-teal-800 dark:text-teal-100 text-sm
              px-3 py-2.5 border-b border-teal-200 dark:border-teal-800">
    <span>本页目录</span>
    <a href="#" class="text-teal-400 dark:text-teal-500 text-base font-normal
                       no-underline hover:text-blue-600 dark:hover:text-blue-400 px-1"
       title="回到顶部">↑</a>
  </div>
  <div class="toc-links overflow-y-auto flex-1 px-3 py-2">
{toc}
  </div>
</nav>
<script src="https://cdn.jsdelivr.net/npm/mermaid@10/dist/mermaid.min.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.11.1/highlight.min.js"></script>
<script>
mermaid.initialize({{ startOnLoad: false, theme: 'base' }});
// protect mermaid blocks from highlight.js — convert before hljs runs
document.querySelectorAll('pre code.language-mermaid').forEach(function(el) {{
  el.parentElement.classList.add('mermaid');
  el.parentElement.innerHTML = el.textContent;
}});
hljs.highlightAll();
mermaid.run();

// ── sidebar toggle ─────────────────────────────────────────────────
(function() {{
  var btn = document.getElementById('toggleSidebar');
  var body = document.body;
  if (!btn) return;

  function apply(state) {{
    body.classList.toggle('sidebar-collapsed', state);
    btn.title = state ? '展开侧边栏' : '收起侧边栏';
    btn.innerHTML = state ? '☰' : '☰';
  }}

  btn.addEventListener('click', function() {{
    var collapsed = !body.classList.contains('sidebar-collapsed');
    apply(collapsed);
    try {{ localStorage.setItem('sidebar-collapsed', collapsed ? '1' : '0'); }} catch(e) {{}}
  }});

  // restore saved state
  try {{
    if (localStorage.getItem('sidebar-collapsed') === '1') apply(true);
  }} catch(e) {{}}
}})();

// ── TOC collapse toggle ──────────────────────────────────────────────
(function() {{
  var btn = document.getElementById('tocToggle');
  var toc = document.getElementById('toc');
  if (!btn || !toc) return;

  function apply(state) {{
    document.body.classList.toggle('toc-collapsed', state);
    btn.innerHTML = state ? '点击展开 <span class="toc-icon inline-block transition-transform duration-300">▶</span>' : '点击收起 <span class="toc-icon inline-block transition-transform duration-300">▶</span>';
    btn.title = state ? '展开目录' : '折叠目录';
  }}

  btn.addEventListener('click', function() {{
    var collapsed = !document.body.classList.contains('toc-collapsed');
    apply(collapsed);
    try {{ localStorage.setItem('toc-collapsed', collapsed ? '1' : '0'); }} catch(e) {{}}
  }});

  try {{
    if (localStorage.getItem('toc-collapsed') === '1') apply(true);
  }} catch(e) {{}}
}})();

// ── TOC scroll-spy (midpoint activation) ────────────────────────────
(function() {{
  var toc = document.getElementById('toc');
  if (!toc) return;
  var links = toc.querySelectorAll('.toc-links a');
  if (!links.length) {{ toc.style.display = 'none'; return; }}

  // collect heading targets
  var headings = [];
  links.forEach(function(a) {{
    var el = document.getElementById(a.getAttribute('data-target'));
    if (el) headings.push({{ el: el, link: a }});
  }});

  // find which heading's midpoint is active
  function updateActive() {{
    var mid = window.scrollY + window.innerHeight / 2;
    var activeIdx = -1;
    for (var i = headings.length - 1; i >= 0; i--) {{
      if (headings[i].el.offsetTop <= mid) {{ activeIdx = i; break; }}
    }}

    links.forEach(function(a, idx) {{
      a.classList.toggle('active', idx === activeIdx);
    }});

    if (activeIdx >= 0 && headings[activeIdx]) {{
      headings[activeIdx].link.scrollIntoView({{ block: 'nearest', behavior: 'instant' }});
    }}
  }}

  updateActive();

  var ticking = false;
  window.addEventListener('scroll', function() {{
    if (!ticking) {{
      requestAnimationFrame(function() {{ ticking = false; updateActive(); }});
      ticking = true;
    }}
  }}, {{ passive: true }});
}})();
</script>
<script>
// Load HLJS theme CSS AFTER Tailwind CDN has injected its styles.
// Tailwind CDN injects styles async (DOMContentLoaded / rAF) — we use
// double rAF to guarantee HLJS loads last and wins cascade ties.
requestAnimationFrame(function() {{
  requestAnimationFrame(function() {{
    var light = document.createElement('link');
    light.rel = 'stylesheet';
    light.media = '(prefers-color-scheme: light)';
    light.href = 'https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.11.1/styles/vs.min.css';
    document.head.appendChild(light);
    var dark = document.createElement('link');
    dark.rel = 'stylesheet';
    dark.media = '(prefers-color-scheme: dark)';
    dark.href = 'https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.11.1/styles/vs2015.min.css';
    document.head.appendChild(dark);
  }});
}});
</script>
</body>
</html>"""
        with open(dst, "w", encoding="utf-8") as f:
            f.write(html)
        print(f"  {src} → {dst}")

    # --- sitemap.xml ---
    BASE = "https://ccalg.dev"
    now = __import__("datetime").datetime.now().strftime("%Y-%m-%d")
    urls = [f"""  <url>
    <loc>{BASE}/</loc>
    <lastmod>{now}</lastmod>
    <changefreq>weekly</changefreq>
    <priority>1.0</priority>
  </url>"""]
    for slug, name, _title in PAGES:
        prio = "1.0" if slug == "index" else "0.8"
        urls.append(f"""  <url>
    <loc>{BASE}/{slug}.html</loc>
    <lastmod>{now}</lastmod>
    <changefreq>weekly</changefreq>
    <priority>{prio}</priority>
  </url>""")
    sitemap = '<?xml version="1.0" encoding="UTF-8"?>\n' \
              '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n' + \
              "\n".join(urls) + '\n</urlset>\n'
    sitemap_path = os.path.join(out_dir, "sitemap.xml")
    with open(sitemap_path, "w", encoding="utf-8") as f:
        f.write(sitemap)
    print(f"  sitemap → {sitemap_path}")

    print(f"\nDone. Open {out_dir}/index.html in a browser.")


if __name__ == "__main__":
    main()

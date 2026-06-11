#!/usr/bin/env python3
"""Convert docs/*.md → build/docs-html/*.html with sidebar + responsive layout.

Requires: pip install markdown
Usage:    python3 scripts/md2html.py docs/ build/docs-html/
          cmake --build build --target docs-html
"""
import sys, os, re
from markdown import markdown

CSS = """
:root { --bg: #fafafa; --fg: #222; --link: #0366d6; --code-bg: #f0f0f0;
        --border: #e1e4e8; --accent: #f6f8fa; --sidebar-w: 220px; }
@media (prefers-color-scheme: dark) {
  :root { --bg: #0d1117; --fg: #c9d1d9; --link: #58a6ff; --code-bg: #161b22;
          --border: #30363d; --accent: #161b22; }
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font: 15px/1.6 -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
       background: var(--bg); color: var(--fg); display: flex; min-height: 100vh; }
html { scroll-behavior: smooth; }
/* ── sidebar + collapse ─────────────────────────────────────────────── */
.sidebar { width: var(--sidebar-w); flex-shrink: 0; background: var(--accent);
           border-right: 1px solid var(--border); padding: 2rem 1.2rem;
           position: sticky; top: 0; height: 100vh; overflow: hidden;
           transition: width .3s ease, padding .3s ease, border-color .3s ease; }
.sidebar:hover { overflow-y: auto; }
.sidebar-toggle { display: block; width: 32px; height: 32px; margin: 0 0 0 auto;
                  border: 1px solid var(--border); border-radius: 6px;
                  background: var(--bg); color: var(--fg); font-size: 18px;
                  cursor: pointer; line-height: 30px; text-align: center;
                  flex-shrink: 0; }
.sidebar-toggle:hover { background: var(--link); color: #fff; border-color: var(--link); }
.sidebar-collapsed .sidebar { width: 44px; padding: 8px 4px; }
.sidebar-collapsed .sidebar h2,
.sidebar-collapsed .sidebar nav { display: none; }
.sidebar-collapsed .main { max-width: none; }
.sidebar h2 { font-size: 1.9em; margin-bottom: 1rem; color: var(--fg);
              border-bottom: 1px solid var(--border); padding-bottom: .5em;
              text-align: center; }
.sidebar a { display: block; padding: .35em 0; color: var(--link); text-decoration: none;
             font-size: 0.95em; border-radius: 4px; padding-left: .5em; }
.sidebar a:hover, .sidebar a.active { background: var(--link); color: #fff; }
.main { flex: 1; padding: 2rem 2.5rem; max-width: 900px; }
/* ── floating TOC ─────────────────────────────────────────────────── */
.toc { position: fixed; right: 12px; top: 80px; width: 210px; max-height: calc(100vh - 120px);
       display: flex; flex-direction: column; font-size: 0.82em; z-index: 100;
       background: var(--bg); border: 1px solid var(--border);
       border-radius: 10px; box-shadow: 0 4px 20px rgba(0,0,0,.08);
       opacity: 0.92; transition: opacity .2s, width .3s ease; }
.toc:hover { opacity: 1; }
.toc-title { display: flex; justify-content: space-between; align-items: center;
             flex-shrink: 0; font-weight: 600; color: var(--fg);
             font-size: 0.9em; padding: .6rem .8rem .4em;
             border-bottom: 1px solid var(--border); }
.toc-links { overflow-y: auto; flex: 1; padding: .4rem .8rem .8rem;
             scrollbar-width: none; }
.toc-links::-webkit-scrollbar { display: none; }
.toc a { display: block; padding: .25em 0 .25em .6em; color: #666;
         text-decoration: none; border-left: 2px solid transparent;
         line-height: 1.35; transition: color .15s, border-color .15s; }
.toc a:hover { color: var(--link); }
.toc a.active { color: var(--link); border-left-color: var(--link); font-weight: 500; }
.toc .toc-h3 { padding-left: 1.2em; font-size: 0.95em; }
.toc-top { color: #999; font-size: 1.1em; font-weight: 400; padding: 0 .3em; }
.toc-top:hover { color: var(--link); text-decoration: none; }
.toc-toggle { position: fixed; right: 12px; top: 50px; z-index: 101;
              padding: 4px 10px; border: 1px solid var(--border);
              border-radius: 6px 6px 0 0;
              background: var(--bg); color: var(--fg);
              font-size: 0.82em; cursor: pointer;
              white-space: nowrap; }
.toc-toggle:hover { background: var(--link); color: #fff; border-color: var(--link); }
.toc-icon { display: inline-block; transition: transform .3s ease; }
.toc-collapsed .toc { display: none; }
.toc-collapsed .toc-icon { transform: rotate(180deg); }
@media (prefers-color-scheme: dark) {
  .toc { box-shadow: 0 4px 20px rgba(0,0,0,.35); }
  .toc a { color: #8b949e; }
}
@media (max-width: 1100px) {
  .toc, .toc-toggle { display: none; }
}
h1 { border-bottom: 1px solid var(--border); padding-bottom: .3em; margin-bottom: 1em; }
h2 { margin-top: 1.8em; margin-bottom: .5em; }
h3 { margin-top: 1.4em; }
a { color: var(--link); text-decoration: none; }
a:hover { text-decoration: underline; }
pre { background: var(--code-bg); border: 1px solid var(--border);
      border-radius: 6px; padding: 1em; overflow-x: auto; margin: 1em 0; }
code { font: 13px SFMono-Regular, Consolas, "Liberation Mono", Menlo, monospace;
       background: var(--code-bg); padding: .2em .4em; border-radius: 3px; }
pre code { background: none; padding: 0; }
table { border-collapse: collapse; width: 100%; margin: 1em 0; }
th, td { border: 1px solid var(--border); padding: 8px 12px; text-align: center; }
th { background: var(--accent); font-weight: 600; }
blockquote { border-left: 3px solid var(--link); padding: .5em 1em; margin: 1em 0;
             background: var(--accent); border-radius: 0 6px 6px 0; }
a.citation { text-decoration: none; color: inherit; font-weight: 500;
              border: 1px solid var(--border); border-radius: 3px;
              padding: 0 .25em; font-size: 0.9em; vertical-align: super;
              line-height: 1; transition: background .15s; }
a.citation:hover { background: var(--link); color: #fff; border-color: var(--link); }
li[id^="ref-"] { scroll-margin-top: 60px; }
footer { margin-top: 3rem; padding-top: 1rem; border-top: 1px solid var(--border);
         font-size: 0.85em; color: #888; }
@media (max-width: 768px) {
  body { flex-direction: column; }
  .sidebar { width: 100%; height: auto; position: static; border-right: none;
             border-bottom: 1px solid var(--border); padding: 1rem 1.2rem; }
  .sidebar a { display: inline-block; margin-right: .8em; }
  .main { padding: 1.5rem; }
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
]

def nav_html(active_slug):
    links = []
    i = 0
    for slug, name, title  in PAGES:
        cls = ' class="active"' if slug == active_slug else ""
        links.append(f'      <a href="{slug}.html"{cls}>{items[i] + name}</a>')
        i = i + 1
    return "\n".join(links)


def toc_html(body_html):
    """Generate TOC links from rendered HTML h2/h3 elements."""
    items = []
    for m in re.finditer(r'<(h[23])(?:\s+id="([^"]*)")?[^>]*>(.+?)</\1>', body_html):
        tag, aid, title = m.group(1), m.group(2), m.group(3).strip()
        if not aid or title.lower() == 'alg docs':
            continue
        cls = 'toc-h3' if tag == 'h3' else 'toc-h2'
        items.append(f'<a href="#{aid}" class="{cls}" data-target="{aid}">{title}</a>')
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
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
{baidu}<title>{ slug == 'index' and title or (title + name)}</title>
<link rel="stylesheet" media="(prefers-color-scheme: light)" href="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.11.1/styles/monokai.min.css">
<link rel="stylesheet" media="(prefers-color-scheme: dark)"  href="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.11.1/styles/monokai-sublime.min.css">
<style>{CSS}</style>
</head>
<body>
<aside class="sidebar" id="sidebar">
  <button class="sidebar-toggle" id="toggleSidebar" title="收起侧边栏">亖</button>
  <h2>目录</h2>
{nav}
</aside>
<main class="main">
{body}
  <footer>
    alg — header-only C data-structure library · BSD 3-Clause · <a href="https://github.com/CandyMi/alg">GitHub</a>
  </footer>
</main>
<button class="toc-toggle" id="tocToggle" title="折叠目录">点击收起 <span class="toc-icon">▶</span></button>
<nav class="toc" id="toc">
  <div class="toc-title"><span>本页目录</span><a href="#" class="toc-top" title="回到顶部">↑</a></div>
  <div class="toc-links">{toc}</div>
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
    btn.innerHTML = state ? '点击展开 <span class="toc-icon">▶</span>' : '点击收起 <span class="toc-icon">▶</span>';
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

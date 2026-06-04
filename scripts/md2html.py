#!/usr/bin/env python3
"""Convert docs/*.md → build/docs-html/*.html with sidebar + responsive layout.

Requires: pip install markdown
Usage:    python3 scripts/md2html.py docs/ build/docs-html/
          cmake --build build --target docs-html
"""
import sys, os
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
.sidebar { width: var(--sidebar-w); flex-shrink: 0; background: var(--accent);
           border-right: 1px solid var(--border); padding: 2rem 1.2rem;
           position: sticky; top: 0; height: 100vh; overflow-y: auto; }
.sidebar h2 { font-size: 1.1em; margin-bottom: 1rem; color: var(--fg);
              border-bottom: 1px solid var(--border); padding-bottom: .5em; }
.sidebar a { display: block; padding: .35em 0; color: var(--link); text-decoration: none;
             font-size: 0.95em; border-radius: 4px; padding-left: .5em; }
.sidebar a:hover, .sidebar a.active { background: var(--link); color: #fff; }
.main { flex: 1; padding: 2rem 2.5rem; max-width: 900px; }
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
th, td { border: 1px solid var(--border); padding: 8px 12px; text-align: left; }
th { background: var(--accent); font-weight: 600; }
blockquote { border-left: 3px solid var(--link); padding: .5em 1em; margin: 1em 0;
             background: var(--accent); border-radius: 0 6px 6px 0; }
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
    ("index",             "基本介绍"),
    ("getting-started",   "快速开始"),
    ("api-reference",     "接口参考"),
    ("building",          "构建指南"),
    ("benchmarks",        "性能基准"),
]


def nav_html(active_slug):
    links = []
    for slug, title in PAGES:
        cls = ' class="active"' if slug == active_slug else ""
        links.append(f'      <a href="{slug}.html"{cls}>{title}</a>')
    return "\n".join(links)


def main():
    src_dir = sys.argv[1] if len(sys.argv) > 1 else "docs"
    out_dir = sys.argv[2] if len(sys.argv) > 2 else "build/docs-html"
    os.makedirs(out_dir, exist_ok=True)

    for slug, title in PAGES:
        src = os.path.join(src_dir, slug + ".md")
        dst = os.path.join(out_dir, slug + ".html")
        if not os.path.isfile(src):
            print(f"  skip {src} (not found)")
            continue

        body = markdown(open(src, encoding="utf-8").read(),
                        extensions=["fenced_code", "tables"])
        nav = nav_html(slug)

        html = f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>{title}</title>
<style>{CSS}</style>
</head>
<body>
<aside class="sidebar">
  <h2>alg docs</h2>
{nav}
</aside>
<main class="main">
{body}
  <footer>
    alg — header-only C data-structure library · BSD 3-Clause · <a href="https://github.com/CandyMi/alg">GitHub</a>
  </footer>
</main>
</body>
</html>"""
        with open(dst, "w", encoding="utf-8") as f:
            f.write(html)
        print(f"  {src} → {dst}")

    print(f"\nDone. Open {out_dir}/index.html in a browser.")


if __name__ == "__main__":
    main()

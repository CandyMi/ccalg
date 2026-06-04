#!/usr/bin/env python3
"""Convert docs/*.md → build/docs-html/*.html with a shared page template.

Requires: pip install markdown
Usage:    python3 scripts/md2html.py docs/ build/docs-html/
          cmake --build build --target docs-html
"""
import sys, os, re
from markdown import markdown

TEMPLATE = """<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>alg — {title}</title>
<style>
  :root {{ --bg: #fafafa; --fg: #222; --link: #0366d6; --code-bg: #f0f0f0;
          --border: #e1e4e8; --accent: #f6f8fa; }}
  @media (prefers-color-scheme: dark) {{
    :root {{ --bg: #0d1117; --fg: #c9d1d9; --link: #58a6ff; --code-bg: #161b22;
             --border: #30363d; --accent: #161b22; }}
  }}
  body {{ font: 15px/1.6 -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
          max-width: 900px; margin: 0 auto; padding: 2rem 1.5rem;
          background: var(--bg); color: var(--fg); }}
  h1 {{ border-bottom: 1px solid var(--border); padding-bottom: .3em; }}
  h2 {{ margin-top: 1.8em; }}
  a {{ color: var(--link); text-decoration: none; }}
  a:hover {{ text-decoration: underline; }}
  pre {{ background: var(--code-bg); border: 1px solid var(--border);
         border-radius: 6px; padding: 1em; overflow-x: auto; }}
  code {{ font: 13px SFMono-Regular, Consolas, "Liberation Mono", Menlo, monospace;
          background: var(--code-bg); padding: .2em .4em; border-radius: 3px; }}
  pre code {{ background: none; padding: 0; }}
  table {{ border-collapse: collapse; width: 100%; }}
  th, td {{ border: 1px solid var(--border); padding: 8px 12px; text-align: left; }}
  th {{ background: var(--accent); }}
  nav {{ margin-bottom: 2rem; padding: .8em 1em; background: var(--accent);
         border: 1px solid var(--border); border-radius: 6px; }}
  nav a {{ margin-right: 1.2em; }}
  footer {{ margin-top: 3rem; padding-top: 1rem; border-top: 1px solid var(--border);
            font-size: 0.85em; color: #888; }}
  .active {{ font-weight: bold; }}
</style>
</head>
<body>
<nav>
  <a href="index.html"{idx_active}>简介</a>
  <a href="getting-started.html"{gs_active}>快速开始</a>
  <a href="api-reference.html"{api_active}>API 参考</a>
  <a href="building.html"{bld_active}>构建指南</a>
</nav>
{body}
<footer>
  alg — header-only C data-structure library · BSD 3-Clause
</footer>
</body>
</html>"""

PAGE_NAMES = {
    "index":             "简介",
    "getting-started":   "快速开始",
    "api-reference":     "API 参考",
    "building":          "构建指南",
}

ACTIVE_MAP = {
    "index":             "idx_active",
    "getting-started":   "gs_active",
    "api-reference":     "api_active",
    "building":          "bld_active",
}


def convert_file(src_path: str) -> str:
    with open(src_path, "r", encoding="utf-8") as f:
        md = f.read()
    return markdown(md, extensions=["fenced_code", "tables"])


def main():
    src_dir = sys.argv[1] if len(sys.argv) > 1 else "docs"
    out_dir = sys.argv[2] if len(sys.argv) > 2 else "build/docs-html"
    os.makedirs(out_dir, exist_ok=True)

    for basename, title in PAGE_NAMES.items():
        src = os.path.join(src_dir, basename + ".md")
        dst = os.path.join(out_dir, basename + ".html")
        if not os.path.isfile(src):
            print(f"  skip {src} (not found)")
            continue

        body = convert_file(src)
        active_key = ACTIVE_MAP.get(basename, "")
        active_kwargs = {}
        for k in ACTIVE_MAP.values():
            active_kwargs[k] = ' class="active"' if k == active_key else ""

        html = TEMPLATE.format(title=title, body=body, **active_kwargs)
        with open(dst, "w", encoding="utf-8") as f:
            f.write(html)
        print(f"  {src} → {dst}")

    print(f"\nDone. Open {out_dir}/index.html in a browser.")


if __name__ == "__main__":
    main()

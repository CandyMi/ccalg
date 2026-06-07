#!/bin/sh
# Sync docs/*.md → gh-pages branch as HTML.
# Run from master branch after updating docs/ or headers.
# Usage: sh scripts/deploy-gh-pages.sh

set -e
TMP="_gh-pages-tmp"
BRANCH="gh-pages"
CUR="$(git branch --show-current)"

if [ "$CUR" != "master" ]; then
  echo "Error: must run from master branch (current: $CUR)" >&2
  exit 1
fi

echo "==> Generating HTML"
rm -rf "$TMP"
python3 scripts/md2html.py docs "$TMP"

echo "==> Deploying to $BRANCH"
git checkout --orphan "$BRANCH-tmp" 2>/dev/null || true
git rm -rfq --cached . 2>/dev/null || true
cp "$TMP"/*.html .
echo "ccalg.dev" > CNAME
git add CNAME *.html
git commit -m "docs: deploy $(date +%Y-%m-%d)"

echo "==> Pushing $BRANCH"
git branch -D "$BRANCH" 2>/dev/null || true
git branch -m "$BRANCH"
git push -f origin "$BRANCH"

echo "==> Back to $CUR"
git clean -fd
git checkout "$CUR"
rm -rf "$TMP"
echo "Done. https://ccalg.dev/"

# Sync docs/*.md → gh-pages branch as HTML (PowerShell).
# Run from master branch after updating docs/ or headers.
# Usage: .\scripts\deploy-gh-pages.ps1

$ErrorActionPreference = "Stop"

$cur = git rev-parse --abbrev-ref HEAD
if ($cur -ne "master") {
    Write-Error "Error: must run from master branch (current: $cur)"
    exit 1
}

Write-Host "==> Generating HTML"
$tmp = "_gh-pages-tmp"
if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
python scripts/md2html.py docs $tmp

Write-Host "==> Deploying to gh-pages"
git checkout --orphan gh-pages-tmp 2>$null
git rm -rfq --cached . 2>$null

Copy-Item "$tmp/*.html" . -Force
if (Test-Path "$tmp/sitemap.xml") { Copy-Item "$tmp/sitemap.xml" . -Force }
"ccalg.dev" | Set-Content CNAME

git add CNAME, *.html, sitemap.xml 2>$null
git commit -m "docs: deploy $(Get-Date -Format yyyy-MM-dd)"

Write-Host "==> Pushing gh-pages"
git branch -D gh-pages 2>$null
git branch -m gh-pages
git push -f origin gh-pages

Write-Host "==> Back to master"
git clean -fd
git checkout master
if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
Write-Host "Done. https://ccalg.dev/"

@echo off
REM Sync docs/*.md → gh-pages branch as HTML (Windows CMD).
REM Run from master branch after updating docs/ or headers.
REM Usage: scripts\deploy-gh-pages.cmd

setlocal enabledelayedexpansion

for /f %%i in ('git rev-parse --abbrev-ref HEAD') do set CUR=%%i
if not "!CUR!"=="master" (
    echo Error: must run from master branch ^(current: !CUR!^) >&2
    exit /b 1
)

echo ==^> Generating HTML
set TMP=_gh-pages-tmp
if exist "%TMP%" rmdir /s /q "%TMP%"
python scripts/md2html.py docs "%TMP%"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo ==^> Deploying to gh-pages
git checkout --orphan gh-pages-tmp 2>nul
git rm -rfq --cached . 2>nul

copy "%TMP%\*.html" . 2>nul
if exist "%TMP%\sitemap.xml" copy "%TMP%\sitemap.xml" .
echo ccalg.dev > CNAME

git add CNAME *.html sitemap.xml 2>nul
git commit -m "docs: deploy %DATE%"

echo ==^> Pushing gh-pages
git branch -D gh-pages 2>nul
git branch -m gh-pages
git push -f origin gh-pages

echo ==^> Back to master
git clean -fd
git checkout master
if exist "%TMP%" rmdir /s /q "%TMP%"
echo Done. https://ccalg.dev/
endlocal

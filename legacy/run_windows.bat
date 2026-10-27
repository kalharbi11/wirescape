@echo off
setlocal
cd /d "%~dp0"
REM If necessary, uncomment the next line and point it at YOUR compiled Surge module.
REM set "SURGEPY_DIR=${USERPROFILE}\Documents\<project-parent>\CHAOS\surgepy_bin"
py -3.12 -m wirescape %*

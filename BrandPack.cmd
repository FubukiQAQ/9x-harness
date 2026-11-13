@echo off
REM ====================================================================
REM  NeuroBench Brand Packager - launcher
REM  Double-click this file to open the visual packager.
REM  (Requires Python 3 with tkinter.)
REM ====================================================================
setlocal
REM This file sits in the repo root, so stay here (do not go up one level).
cd /d "%~dp0"

set PYEXE=

where python >nul 2>nul && set PYEXE=python
if not defined PYEXE (
    where py >nul 2>nul && set PYEXE=py -3
)
if not defined PYEXE (
    for %%P in (C:\Python314\python.exe C:\Python313\python.exe C:\Python312\python.exe C:\Python311\python.exe) do (
        if not defined PYEXE if exist "%%P" set PYEXE=%%P
    )
)
if not defined PYEXE (
    if exist "%DSH_PYTHON%" set PYEXE=%DSH_PYTHON%
)

if not defined PYEXE (
    echo.
    echo   Python 3 with tkinter not found.
    echo   Run manually:  "C:\path\to\python.exe" tools\brandpack.py
    echo.
    pause
    exit /b 1
)

%PYEXE% tools\brandpack.py %*
if errorlevel 1 pause
endlocal

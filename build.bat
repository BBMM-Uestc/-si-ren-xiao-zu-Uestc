@echo off
rem ============================================================
rem  MiniLogisim build script (VS2022 + wxWidgets 3.2.2.1)
rem  Usage:  build.bat            -> Debug|x64
rem          build.bat Release    -> Release|x64
rem  Note: keep this file ASCII-only; cmd.exe cannot parse
rem        UTF-8 Chinese comments reliably.
rem ============================================================
setlocal

set CFG=%1
if "%CFG%"=="" set CFG=Debug

set MSB=
for %%P in (
  "D:\vs2022\MSBuild\Current\Bin\amd64\MSBuild.exe"
  "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
  "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\amd64\MSBuild.exe"
  "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe"
  "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe"
) do (
  if not defined MSB if exist %%P set MSB=%%P
)
if not defined MSB (
  echo [ERROR] MSBuild.exe not found. Edit this script and set MSB manually.
  exit /b 1
)

if "%WX_ROOT%"=="" set WX_ROOT=C:\wxWidgets-3.2.2.1
if not exist "%WX_ROOT%\include\wx\wx.h" (
  echo [ERROR] wxWidgets not found at %WX_ROOT%
  echo         Set the WX_ROOT environment variable to your wxWidgets 3.2.2.1 folder.
  exit /b 1
)

echo MSBuild   : %MSB%
echo wxWidgets : %WX_ROOT%
echo Config    : %CFG% ^| x64
echo.

"%MSB%" "%~dp0MiniLogisim.sln" /p:Configuration=%CFG% /p:Platform=x64 /m /v:minimal /nologo
if errorlevel 1 (
  echo.
  echo [FAILED] Build error.
  exit /b 1
)

echo.
echo [OK] Output: %~dp0bin\%CFG%\MiniLogisim.exe
endlocal

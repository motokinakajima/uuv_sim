@echo off
setlocal EnableExtensions

set "ROOT_DIR=%~dp0"
if "%ROOT_DIR:~-1%"=="\" set "ROOT_DIR=%ROOT_DIR:~0,-1%"
set "BUILD_DIR=%ROOT_DIR%\build"
set "OUTPUT_JSON=%ROOT_DIR%\simulation_data.json"

echo Configuring project...
cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%"
if errorlevel 1 (
  echo Error: configure failed.
  exit /b 1
)

echo Building project...
cmake --build "%BUILD_DIR%" --config Debug
if errorlevel 1 (
  echo Error: build failed.
  exit /b 1
)

set "EXECUTABLE="
if exist "%BUILD_DIR%\uuv_sim.exe" set "EXECUTABLE=%BUILD_DIR%\uuv_sim.exe"
if not defined EXECUTABLE if exist "%BUILD_DIR%\uuv_sim" set "EXECUTABLE=%BUILD_DIR%\uuv_sim"
if not defined EXECUTABLE if exist "%BUILD_DIR%\Debug\uuv_sim.exe" set "EXECUTABLE=%BUILD_DIR%\Debug\uuv_sim.exe"
if not defined EXECUTABLE if exist "%BUILD_DIR%\Debug\uuv_sim" set "EXECUTABLE=%BUILD_DIR%\Debug\uuv_sim"

if not defined EXECUTABLE (
  echo Error: Could not find built executable in "%BUILD_DIR%"
  exit /b 1
)

echo Running simulation...
pushd "%ROOT_DIR%"
"%EXECUTABLE%"
set "RUN_EXIT=%ERRORLEVEL%"
popd

if not "%RUN_EXIT%"=="0" (
  echo Error: simulation process failed with exit code %RUN_EXIT%.
  exit /b %RUN_EXIT%
)

if exist "%OUTPUT_JSON%" (
  echo Done: %OUTPUT_JSON%
  exit /b 0
) else (
  echo Warning: simulation ran, but "%OUTPUT_JSON%" was not found.
  exit /b 1
)

@echo off
setlocal EnableExtensions

set "ROOT_DIR=%~dp0"
if "%ROOT_DIR:~-1%"=="\" set "ROOT_DIR=%ROOT_DIR:~0,-1%"

set "CONDA_BAT=%USERPROFILE%\anaconda3\condabin\conda.bat"
if not exist "%CONDA_BAT%" (
  echo Error: conda.bat was not found at "%CONDA_BAT%".
  exit /b 1
)

call "%CONDA_BAT%" activate hayabusa
if errorlevel 1 (
  echo Error: failed to activate conda environment "hayabusa".
  exit /b 1
)

pushd "%ROOT_DIR%"
python tree_ga_depth3.py --field-count 10 --trials-per-field 10 %*
set "EXIT_CODE=%ERRORLEVEL%"
popd

if not "%EXIT_CODE%"=="0" (
  echo Error: tree GA failed with exit code %EXIT_CODE%.
  exit /b %EXIT_CODE%
)

echo Done: tree_ga_depth3.py
exit /b 0
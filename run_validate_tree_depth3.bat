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
python validate_tree_depth3.py --runs 4500 --field-seed 42 --skip-build %*
set "EXIT_CODE=%ERRORLEVEL%"
popd

if not "%EXIT_CODE%"=="0" (
  echo Error: validation failed with exit code %EXIT_CODE%.
  exit /b %EXIT_CODE%
)

echo Done: tree_validation_runs.csv
exit /b 0
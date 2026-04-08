@echo off
setlocal EnableExtensions

set "ROOT_DIR=%~dp0"
if "%ROOT_DIR:~-1%"=="\" set "ROOT_DIR=%ROOT_DIR:~0,-1%"

where py >nul 2>&1
if errorlevel 1 (
  echo Error: Python launcher 'py' was not found.
  exit /b 1
)

pushd "%ROOT_DIR%"
py run_agent_count_experiments.py --total-runs 5000 --agent-counts 3,5,8,10,15,20,30 --worker-ratio 0.7 --csv-path experiment_results_summary.csv
set "EXIT_CODE=%ERRORLEVEL%"
popd

if not "%EXIT_CODE%"=="0" (
  echo Error: batch experiments failed with exit code %EXIT_CODE%.
  exit /b %EXIT_CODE%
)

echo Done: experiment_results_summary.csv
exit /b 0

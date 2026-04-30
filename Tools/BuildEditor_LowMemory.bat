@echo off
REM Одна задача компиляции за раз — меньше пик RAM/PCH (C3859/C1076/1455).
REM При необходимости поправь путь к UE_5.3.

set "UE_ROOT=D:\vlads\Documents\UE\UE_5.3"
set "UPROJECT=%~dp0..\MyProject.uproject"

"%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" MyProjectEditor Win64 Development -Project="%UPROJECT%" -WaitMutex -MaxParallelActions=1

pause

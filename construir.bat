@echo off
rem Gera dist\mw_rumble.exe, dist\instalar.exe e dist\ajustes_rework.exe (precisa de gcc no PATH, ex.: WinLibs)
cd /d "%~dp0"
if not exist dist mkdir dist
cd src
gcc -O2 -Wall -Wextra -static -s -o ..\dist\mw_rumble.exe mw_rumble.c -lwinmm || goto erro
gcc -O2 -Wall -Wextra -static -s -o ..\dist\instalar.exe instalar.c -lshell32 -lole32 -luuid || goto erro
gcc -O2 -Wall -Wextra -static -s -o ..\dist\ajustes_rework.exe ajustes_rework.c || goto erro
cd ..
echo.
echo Pronto: dist\mw_rumble.exe, dist\instalar.exe e dist\ajustes_rework.exe
pause
exit /b 0
:erro
echo.
echo Falhou. Confira se o gcc esta no PATH.
pause
exit /b 1

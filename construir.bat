@echo off
rem Gera os executaveis em dist\ (precisa de gcc no PATH, ex.: WinLibs, e windres nao e necessario)
rem   dist\NFSMW_Rework_Plus_Setup.exe  instalador unico (janela) - e o que se distribui
rem   dist\mw_rumble.exe, instalar.exe, ajustes_rework.exe  ferramentas avulsas
rem Os .asi (src\asi\*.c) sao 32 bits: ja estao embutidos em src\blobs.h. Para recompila-los use um clang i686
rem (ex.: llvm-mingw) e rode tools\gerar_dados.py (veja o cabecalho do script).
cd /d "%~dp0"
if not exist dist mkdir dist
if not exist build mkdir build
cd src
gcc -O2 -Wall -Wextra -static -s -o ..\dist\mw_rumble.exe mw_rumble.c -lwinmm || goto erro
gcc -O2 -Wall -Wextra -static -s -o ..\dist\instalar.exe instalar.c -lshell32 -lole32 -luuid || goto erro
gcc -O2 -Wall -Wextra -static -s -o ..\dist\ajustes_rework.exe ajustes_rework.c || goto erro
gcc -O2 -Wall -Wextra -c -Dmain=instalar_main instalar.c -o ..\build\instalar.o || goto erro
gcc -O2 -Wall -Wextra -c -Dmain=ajustes_main ajustes_rework.c -o ..\build\ajustes.o || goto erro
gcc -O2 -Wall -Wextra -mwindows -static -s -o ..\dist\NFSMW_Rework_Plus_Setup.exe setup.c ..\build\instalar.o ..\build\ajustes.o -lshell32 -lole32 -luuid -lcomctl32 -lgdi32 -luser32 -lwinmm || goto erro
cd ..
echo.
echo Pronto: dist\NFSMW_Rework_Plus_Setup.exe (instalador unico) e as ferramentas avulsas em dist\
pause
exit /b 0
:erro
echo.
echo Falhou. Confira se o gcc esta no PATH.
pause
exit /b 1

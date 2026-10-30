// NFSMWRumbleLauncher.asi — inicia o mw_rumble.exe (escondido) junto com o jogo.
// Caminho do exe em scripts/NFSMWRumble.ini:  exe=C:\pasta\mw_rumble.exe
// O mw_rumble e iniciado com --sair-com-jogo e fecha sozinho quando o speed.exe fecha.
// Se ja houver um mw_rumble.exe rodando, nao inicia outro. Apagar o .asi desfaz.
#include <windows.h>
#include <tlhelp32.h>

static int ja_rodando(void) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32 e; e.dwSize = sizeof e; int achou = 0;
    for (BOOL ok = Process32First(s, &e); ok; ok = Process32Next(s, &e))
        if (!lstrcmpiA(e.szExeFile, "mw_rumble.exe")) { achou = 1; break; }
    CloseHandle(s);
    return achou;
}

static DWORD WINAPI iniciar(LPVOID p) {
    (void)p;
    char exe[MAX_PATH] = "", ini[MAX_PATH];
    GetFullPathNameA("scripts/NFSMWRumble.ini", MAX_PATH, ini, NULL);
    GetPrivateProfileStringA("rumble", "exe", "", exe, MAX_PATH, ini);
    if (!exe[0] || GetFileAttributesA(exe) == INVALID_FILE_ATTRIBUTES || ja_rodando()) return 0;
    char dir[MAX_PATH], cmd[MAX_PATH + 64];
    lstrcpyA(dir, exe);
    char* b = dir + lstrlenA(dir);
    while (b > dir && *b != 92 && *b != '/') b--;
    *b = 0;
    wsprintfA(cmd, "\"%s\" --log --sair-com-jogo", exe);
    STARTUPINFOA si; ZeroMemory(&si, sizeof si); si.cb = sizeof si;
    PROCESS_INFORMATION pi;
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS, NULL, dir, &si, &pi)) {
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD why, LPVOID r) {
    (void)h; (void)r;
    if (why == DLL_PROCESS_ATTACH) {
        HANDLE t = CreateThread(NULL, 0, iniciar, NULL, 0, NULL);
        if (t) CloseHandle(t);
    }
    return TRUE;
}

// NFSMWDifficulty.asi — pergunta de dificuldade ao criar carreira (etapa 1: so a cadeia de dialogos)
// speed.exe 1.3 (Rework). Remover o .asi desfaz tudo.
#include <windows.h>

typedef void (__thiscall *fn_resp)(void*, DWORD, DWORD, DWORD, DWORD, DWORD);
typedef void (__thiscall *fn_state)(void*, int);
typedef void (__thiscall *fn_dlg)(void*, DWORD, DWORD);
typedef void (__thiscall *fn_btn)(void*, int, int, int);
typedef void (__thiscall *fn_void)(void*);
typedef void (__cdecl *fn_settext)(DWORD, DWORD, DWORD);
typedef void (__cdecl *fn_focus)(DWORD, DWORD);

#define RESP_ORIG   0x563380
#define STATE_ORIG  0x544370
#define DLG_YESNO   0x543cc0   // dialogo de 2 botoes da tela MC_ProfileManager (tem 3 slots de botao)
#define DLG_BUTTON  0x543920   // (idx, mostrar, 0)
#define DLG_CLEAN   0x54f9a0
#define SET_TEXT    0x525220   // (pacote, objeto, rotulo)
#define SET_FOCUS   0x515e90   // (pacote, mensagem do botao)
#define FLAGS       ((volatile DWORD*)0x91cbac)
#define FLAGS_PREV  ((volatile DWORD*)0x91cbcc)

#define LBL_TITLE   0x4d156786 // QR_OPT_DIFFICULTY
#define LBL_EASY    0x61973e01
#define LBL_MEDIUM  0x3747f6d0
#define LBL_HARD    0x6198e2ee
#define TYPE_DIFF   0xe000000  // tipo de dialogo nao usado pelo jogo
#define OBJ_TXT0    0xf9363f30 // texto do botao 1 (msg 0xb8a7c6cc)
#define OBJ_TXT1    0xfb8b67d1 // botao 2 (0xb8a7c6cd)
#define OBJ_TXT2    0xfde09072 // botao 3 (0xb8a7c6ce)

static int g_choice = -1; // 0 facil, 1 medio, 2 dificil

static void logf_(const char* s) {
    HANDLE f = CreateFileA("scripts/NFSMWDifficulty.log", FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    static const char crlf[2] = {13, 10};
    DWORD n; WriteFile(f, s, lstrlenA(s), &n, NULL); WriteFile(f, crlf, 2, &n, NULL); CloseHandle(f);
}

static void apply_choice(int c) {
    static const char* nm[3] = {"easy", "medium", "hard"};
    static const char* ext[2] = {"bin", "lzc"};
    for (int i = 0; i < 2; i++) {
        char src[128], dst[128], msg[200];
        wsprintfA(src, "scripts/NFSMWDifficulty/gameplay_%s.%s", nm[c], ext[i]);
        wsprintfA(dst, "GLOBAL/gameplay.%s", ext[i]);
        BOOL ok = CopyFileA(src, dst, FALSE);
        wsprintfA(msg, "copia %s -> %s: %s (erro %lu)", src, dst, ok ? "ok" : "FALHOU", ok ? 0UL : GetLastError());
        logf_(msg);
    }
}

static void show_selector(void* t) {
    ((fn_dlg)DLG_YESNO)(t, LBL_TITLE, TYPE_DIFF);          // monta titulo + 2 botoes
    DWORD pkg = *(DWORD*)((BYTE*)t + 0x10);
    ((fn_btn)DLG_BUTTON)(t, 2, 1, 0);                      // liga o 3o botao
    ((fn_settext)SET_TEXT)(pkg, OBJ_TXT0, LBL_EASY);
    ((fn_settext)SET_TEXT)(pkg, OBJ_TXT1, LBL_MEDIUM);
    ((fn_settext)SET_TEXT)(pkg, OBJ_TXT2, LBL_HARD);
    ((fn_focus)SET_FOCUS)(pkg, 0xb8a7c6cd);                // foco em Medio
}

static void __thiscall resp_hook(void* t, DWORD a1, DWORD msg, DWORD a3, DWORD a4, DWORD a5) {
    DWORD flags = *FLAGS;
    if ((flags & 0xf000000) == TYPE_DIFF) {
        DWORD id = *(DWORD*)(msg + 0x10);
        *FLAGS_PREV = TYPE_DIFF;
        *FLAGS = flags & 0xf0ffffff;
        ((fn_void)DLG_CLEAN)(t);
        g_choice = id == 0xb8a7c6ce ? 2 : id == 0xb8a7c6cc ? 0 : 1;
        logf_(g_choice == 2 ? "escolha: dificil" : g_choice == 1 ? "escolha: medio" : "escolha: facil");
        apply_choice(g_choice);
        ((fn_state)STATE_ORIG)(t, 3);
        return;
    }
    ((fn_resp)RESP_ORIG)(t, a1, msg, a3, a4, a5);
}

static void __thiscall state_hook(void* t, int st) {
    if (st == 3) { show_selector(t); return; }
    ((fn_state)STATE_ORIG)(t, st);
}

static int patch_call(DWORD site, DWORD expect_target, void* hook) {
    BYTE* p = (BYTE*)site;
    if (p[0] != 0xE8 || site + 5 + *(int*)(p + 1) != expect_target) return 0;
    DWORD old;
    if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old)) return 0;
    *(int*)(p + 1) = (int)((DWORD)hook - (site + 5));
    VirtualProtect(p, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
    return 1;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD why, LPVOID r) {
    if (why == DLL_PROCESS_ATTACH) {
        int a = patch_call(0x5640FD, RESP_ORIG, resp_hook);   // despacho da resposta de dialogo
        int b = patch_call(0x563660, STATE_ORIG, state_hook); // aceitar o 2o aviso -> estado 3
        logf_(a && b ? "hooks ok" : "hooks FALHARAM (bytes inesperados)");
    }
    return TRUE;
}

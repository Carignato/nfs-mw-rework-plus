// NFSMWDifficulty.asi — seletor de dificuldade (Facil / Medio / Dificil) ao criar uma carreira.
// speed.exe 1.3 (Rework). Remover o .asi desfaz tudo.
//
// Como a dificuldade funciona no jogo (carreira): o speed.exe le um int16 do perfil da carreira
// (perfil+0xb8, escala -32768..32767 = -1..+1). Esse valor "f" define a forca do catch-up da IA
// ((f+1)/2) e reduz a habilidade dos oponentes quando e negativo. O proprio jogo reescreve esse
// valor no fim de cada corrida (dificuldade adaptativa; chamada em 0x5ff7bb -> setter 0x5db470).
// Este ASI: (1) neutraliza essa chamada, fixando o valor; (2) grava o nivel escolhido no perfil
// novo: Facil = -1, Medio = 0, Dificil = +1. O valor fica no save, entao cada carreira tem o seu.
#include <windows.h>
#include <string.h>

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

#define PERFIL_PTR   ((DWORD*)0x91cf90)   // objeto cujo +0x10 aponta para o perfil da carreira
#define RACESTATUS   ((DWORD*)0x91e000)   // GRaceStatus (existe quando o mundo/corrida esta carregado)
#define TEMPO_JANELA 180000               // ms entre escolher e o perfil novo aparecer

static volatile int g_pendente = 0;
static volatile short g_alvo = 0;
static volatile DWORD g_t0 = 0;
static volatile int g_mudou = 0;          // o perfil ja foi recriado (nome mudou)?
static char g_nome0[16];

static BYTE* perfil(void) {
    if (IsBadReadPtr(PERFIL_PTR, 4) || !*PERFIL_PTR || IsBadReadPtr((void*)(*PERFIL_PTR + 0x10), 4)) return NULL;
    BYTE* p = (BYTE*)*(DWORD*)(*PERFIL_PTR + 0x10);
    if (!p || IsBadReadPtr(p, 0xc0)) return NULL;
    return p;
}

static void log_val(const char* txt, BYTE* p) {
    char m[160], nome[17]; int i;
    for (i = 0; i < 16 && p[i]; i++) nome[i] = (char)p[i];
    nome[i] = 0;
    wsprintfA(m, "%s (perfil '%s', valor agora %d)", txt, nome, (int)*(short*)(p + 0xb8));
    logf_(m);
}

static DWORD WINAPI vigia(LPVOID x) {
    (void)x;
    for (;; Sleep(100)) {
        if (!g_pendente) continue;
        BYTE* p = perfil();
        DWORD dt = GetTickCount() - g_t0;
        if (dt > TEMPO_JANELA && !g_mudou) { g_pendente = 0; logf_("escolha descartada: o perfil novo nao apareceu a tempo"); continue; }
        if (!p) continue;
        if (!g_mudou) {
            if (memcmp(p, g_nome0, 16) == 0) continue;
            g_mudou = 1; log_val("perfil novo detectado", p);
        } else if (!IsBadReadPtr(RACESTATUS, 4) && *RACESTATUS) {   // entrou no jogo: terminou
            g_pendente = 0; g_mudou = 0; log_val("dificuldade fixada", p); continue;
        } else if (dt > TEMPO_JANELA + 600000) { g_pendente = 0; g_mudou = 0; continue; }
        if (*(short*)(p + 0xb8) != g_alvo) *(short*)(p + 0xb8) = g_alvo;
    }
}

static void escolher(int c) {
    static const short alvo[3] = {-32768, 0, 32767};   // facil, medio, dificil
    BYTE* p = perfil();
    memset(g_nome0, 0, sizeof g_nome0);
    if (p) memcpy(g_nome0, p, 16);
    g_alvo = alvo[c]; g_mudou = 0; g_t0 = GetTickCount(); g_pendente = 1;
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
        escolher(g_choice);
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

// call 0x5db470 (grava a dificuldade adaptativa no perfil) -> add esp,4 ; nop ; nop  (o setter desempilhava 4 bytes)
static int congelar_adaptacao(void) {
    BYTE* p = (BYTE*)0x5ff7bb;
    if (p[0] != 0xE8 || 0x5ff7bb + 5 + *(int*)(p + 1) != 0x5db470) return 0;
    static const BYTE novo[5] = {0x83, 0xC4, 0x04, 0x90, 0x90};
    DWORD old;
    if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old)) return 0;
    memcpy(p, novo, 5);
    VirtualProtect(p, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
    return 1;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD why, LPVOID r) {
    if (why == DLL_PROCESS_ATTACH) {
        int a = patch_call(0x5640FD, RESP_ORIG, resp_hook);   // despacho da resposta de dialogo
        int b = patch_call(0x563660, STATE_ORIG, state_hook); // aceitar o 2o aviso -> estado 3
        int c = congelar_adaptacao();
        logf_(a && b && c ? "hooks ok" : "hooks FALHARAM (bytes inesperados)");
        if (a && b && c) CreateThread(NULL, 0, vigia, NULL, 0, NULL);
    }
    return TRUE;
}

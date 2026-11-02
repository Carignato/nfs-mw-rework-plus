/* setup - instalador unico do "NFS Most Wanted Rework Plus" (janela: escolhe a pasta do jogo e instala tudo).
 *
 * Reaproveita o instalador do painel (instalar.c) e os ajustes do Rework (ajustes_rework.c), compilados com
 * main renomeado (instalar_main / ajustes_main), e acrescenta:
 *   - copia do mw_rumble.exe + launcher .asi (inicia a vibracao sozinha com o jogo);
 *   - seletor de dificuldade ao criar carreira (so o .asi; o nivel fica no perfil da carreira);
 *   - recarga do nitro 40% mais lenta e limites de infracao (opcionais).
 * Nenhum arquivo do jogo e distribuido: tudo e gerado/alterado a partir do que o usuario ja tem, com backup (*.bak_vibracao).
 */
#define _WIN32_IE 0x0600
#include "comum.h"
#include <shlobj.h>
#include <commctrl.h>
#include <stdarg.h>
#include "blobs.h"

int instalar_main(int argc, char **argv);
int ajustes_main(int argc, char **argv);

#define TAM_SPEED_EXE 6049792u
#define DIF_TAM_BIN 2294432u          /* tamanho do GLOBAL/gameplay.bin do Rework v2.0 */

enum { ID_PASTA = 100, ID_PROCURAR, ID_RUMBLE, ID_FIX, ID_DIF, ID_REC40, ID_INFRA, ID_INSTALAR, ID_DESINST, ID_LOG };

static HWND g_wnd, g_log;
static HICON g_icone;    /* icone do instalador (resource 1), mostrado no topo da janela */
static HFONT g_fonte, g_fonte_pequena;
#define ALTURA_TOPO 128  /* espaco do cabecalho (icone + "by carignato"); o resto da janela desce esse tanto */
static int g_teste;      /* --teste "pasta": instala (e desinstala) sem janela, para testes automaticos em pastas falsas */

/* ------------------------------- log na janela ------------------------------- */
static void log_texto(const char *t) {
    size_t n = strlen(t), extra = 0;
    for (size_t i = 0; i < n; i++) if (t[i] == '\n' && (i == 0 || t[i - 1] != '\r')) extra++;
    char *b = (char *)malloc(n + extra + 1), *o = b;
    for (size_t i = 0; i < n; i++) { if (t[i] == '\n' && (i == 0 || t[i - 1] != '\r')) *o++ = '\r'; *o++ = t[i]; }
    *o = 0;
    SendMessageA(g_log, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageA(g_log, EM_REPLACESEL, FALSE, (LPARAM)b);
    free(b);
}
static void dizer(const char *fmt, ...) {
    char b[1024]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    log_texto(b); log_texto("\n");
}
/* roda uma main() dos outros instaladores capturando o que ela imprime */
static int chamar(int (*fn)(int, char **), int argc, char **argv) {
    char dir[MAX_PATH], tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, dir); GetTempFileNameA(dir, "nfs", 0, tmp);
    fflush(stdout);
    if (!freopen(tmp, "w", stdout)) return -1;
    int r = fn(argc, argv);
    fflush(stdout);
    freopen("NUL", "w", stdout);
    char *t = ler_arquivo(tmp, NULL);
    if (t) { log_texto(t); free(t); }
    DeleteFileA(tmp);
    return r;
}

/* ------------------------------- utilitarios ------------------------------- */
static int escrever_blob(const char *path, const unsigned char *d, size_t n) {
    char dir[MAX_PATH]; snprintf(dir, sizeof dir, "%s", path);
    char *s = strrchr(dir, '\\'); if (s) { *s = 0; criar_pastas(dir); }
    return gravar_arquivo(path, (const char *)d, n);
}
static void apagar(const char *path) { if (arquivo_existe(path)) { DeleteFileA(path); dizer("  removido: %s", path); } }
static long tamanho(const char *path) {
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &d)) return -1;
    return (long)d.nFileSizeLow;
}
static int trocar_ini(const char *ini, const char *chave, const char *valor) {
    char *t = ler_arquivo(ini, NULL); if (!t) return 0;
    char *a = ini_get(t, chave);
    if (!a) { dizer("  chave %s nao existe no .ini", chave); free(t); return 0; }
    char tok[32] = {0}; sscanf(a, "%31s", tok); free(a);
    int ok = 1;
    if (strcmp(tok, valor) != 0) { char *n = ini_set_token(t, chave, valor); ok = gravar_arquivo(ini, n, strlen(n)); free(n); dizer("  %s: %s -> %s", chave, tok, valor); }
    else dizer("  %s ja era %s", chave, valor);
    free(t); return ok;
}

/* ------------------------------- recarga 40% mais lenta ------------------------------- */
/* RECHARGE_MIN (+0x0C) e RECHARGE_MAX (+0x14) dos 4 registros 'nos' do attributes.bin do Rework: tempo em segundos */
static const float REC_ORIG[NREG_NITRO][2] = {{50, 30}, {50, 30}, {50, 30}, {30, 30}};
static const float REC_LENTA[NREG_NITRO][2] = {{83.33f, 50}, {83.33f, 50}, {83.33f, 50}, {50, 50}};
static void recarga_lenta(const char *attr) {
    size_t n; char *b = ler_arquivo(attr, &n);
    if (!b || n != TAM_ATTR_REWORK) { dizer("  recarga mais lenta: attributes.bin ausente ou de outra versao - nada alterado"); free(b); return; }
    int orig = 0, lenta = 0;
    for (int i = 0; i < NREG_NITRO; i++) {
        float v[2]; memcpy(&v[0], b + REG_NITRO[i] + 0x0C, 4); memcpy(&v[1], b + REG_NITRO[i] + 0x14, 4);
        if (v[0] == REC_ORIG[i][0] && v[1] == REC_ORIG[i][1]) orig++;
        else if (v[0] == REC_LENTA[i][0] && v[1] == REC_LENTA[i][1]) lenta++;
    }
    if (lenta == NREG_NITRO) dizer("  recarga do nitro 40%% mais lenta: ja aplicada");
    else if (orig != NREG_NITRO) dizer("  recarga mais lenta: valores do attributes.bin diferentes do esperado - nada alterado");
    else {
        copiar_backup(attr);
        for (int i = 0; i < NREG_NITRO; i++) { memcpy(b + REG_NITRO[i] + 0x0C, &REC_LENTA[i][0], 4); memcpy(b + REG_NITRO[i] + 0x14, &REC_LENTA[i][1], 4); }
        dizer(gravar_arquivo(attr, b, n) ? "  recarga do nitro 40%% mais lenta: aplicada" : "  falha ao gravar attributes.bin");
    }
    free(b);
}

/* ------------------------------- dificuldade ------------------------------- */
/* O seletor so precisa do .asi: ele grava o nivel no perfil da carreira. Versoes antigas (v1.1.0) trocavam o
   gameplay.bin/.lzc; aqui o original volta para o lugar, se ainda estiver trocado. */
static void dif_restaurar_gameplay(const char *pasta) {
    char vdir[MAX_PATH], f1[MAX_PATH], f2[MAX_PATH], gbin[MAX_PATH], glzc[MAX_PATH];
    juntar(vdir, sizeof vdir, pasta, "scripts\\NFSMWDifficulty");
    juntar(gbin, sizeof gbin, pasta, "GLOBAL\\gameplay.bin");
    juntar(glzc, sizeof glzc, pasta, "GLOBAL\\gameplay.lzc");
    juntar(f1, sizeof f1, vdir, "gameplay_easy.bin"); juntar(f2, sizeof f2, vdir, "gameplay_easy.lzc");
    if (!arquivo_existe(f1) || !arquivo_existe(f2)) return;            /* nada da versao antiga */
    if (tamanho(f1) == (long)DIF_TAM_BIN && tamanho(f2) == (long)DIF_TAM_BIN + 16 && CopyFileA(f1, gbin, FALSE) && CopyFileA(f2, glzc, FALSE))
        dizer("  gameplay.bin/.lzc originais de volta (a versao antiga do seletor os tinha alterado)");
    static const char *velhos[] = {"gameplay_easy.bin", "gameplay_easy.lzc", "gameplay_medium.bin", "gameplay_medium.lzc", "gameplay_hard.bin", "gameplay_hard.lzc"};
    for (size_t i = 0; i < sizeof velhos / sizeof *velhos; i++) { char v[MAX_PATH]; juntar(v, sizeof v, vdir, velhos[i]); apagar(v); }
    RemoveDirectoryA(vdir);
}
static void instalar_dificuldade(const char *pasta) {
    char exe[MAX_PATH], asi[MAX_PATH];
    juntar(exe, sizeof exe, pasta, "speed.exe");
    juntar(asi, sizeof asi, pasta, "scripts\\NFSMWDifficulty.asi");
    if (tamanho(exe) != (long)TAM_SPEED_EXE) { dizer("  dificuldade: speed.exe de outra versao (esperado %u bytes) - nao instalada", TAM_SPEED_EXE); return; }
    dif_restaurar_gameplay(pasta);
    if (escrever_blob(asi, BLOB_DIFASI, BLOB_DIFASI_N))
        dizer("  seletor de dificuldade instalado. Ao criar uma carreira nova aparece a tela Facil / Medio / Dificil.\n"
              "  O nivel fica gravado na carreira e o jogo deixa de ajustar a dificuldade sozinho.");
    else dizer("  falha ao gravar NFSMWDifficulty.asi");
}

/* ------------------------------- instalar / desinstalar ------------------------------- */
static int pasta_valida(char *pasta) {
    GetWindowTextA(GetDlgItem(g_wnd, ID_PASTA), pasta, MAX_PATH);
    size_t n = strlen(pasta);
    while (n && (pasta[n - 1] == '\\' || pasta[n - 1] == '/' || pasta[n - 1] == ' ' || pasta[n - 1] == '"')) pasta[--n] = 0;
    if (pasta[0] == '"') memmove(pasta, pasta + 1, strlen(pasta));
    char exe[MAX_PATH]; juntar(exe, sizeof exe, pasta, "speed.exe");
    if (!pasta[0] || !arquivo_existe(exe)) { dizer("Nao achei o speed.exe nessa pasta. Escolha a pasta do jogo (a que tem o speed.exe)."); return 0; }
    if (!g_teste && achar_pid("speed.exe")) { dizer("Feche o jogo (speed.exe) e tente de novo."); return 0; }
    if (!g_teste && achar_pid("mw_rumble.exe")) { dizer("Feche o mw_rumble.exe (vibracao) e tente de novo."); return 0; }
    return 1;
}
static int marcado(int id) { return IsDlgButtonChecked(g_wnd, id) == BST_CHECKED; }

static void fazer_instalar(void) {
    char pasta[MAX_PATH]; if (!pasta_valida(pasta)) return;
    char scripts[MAX_PATH], rdir[MAX_PATH], p[MAX_PATH];
    juntar(scripts, sizeof scripts, pasta, "scripts");
    juntar(rdir, sizeof rdir, pasta, "scripts\\NFSMWRumble");
    if (!pasta_existe(scripts)) { dizer("Nao achei a pasta \"scripts\" do jogo: este instalador e para o NFS Most Wanted com o mod Rework (ASI Loader)."); return; }
    SetCursor(LoadCursor(NULL, IDC_WAIT));
    dizer("== Instalando em %s ==", pasta);

    if (marcado(ID_RUMBLE)) {
        dizer("\n[Vibracao + painel]");
        juntar(p, sizeof p, rdir, "mw_rumble.exe");
        if (!escrever_blob(p, BLOB_RUMBLE, BLOB_RUMBLE_N)) dizer("  falha ao gravar %s", p);
        _putenv_s("NFSMW_DIR_PROGRAMA", rdir);
        char *av[4] = {"instalar", "--sem-pausa", pasta, "--teste"};
        chamar(instalar_main, g_teste ? 4 : 3, av);
        char asi[MAX_PATH], ini[MAX_PATH];
        juntar(asi, sizeof asi, pasta, "scripts\\NFSMWRumbleLauncher.asi");
        juntar(ini, sizeof ini, pasta, "scripts\\NFSMWRumble.ini");
        {   /* a vibracao sempre inicia sozinha com o jogo (e fecha junto) */
            char exe_barra[MAX_PATH], txt[MAX_PATH + 32];
            snprintf(exe_barra, sizeof exe_barra, "%s", p); for (char *c = exe_barra; *c; c++) if (*c == '\\') *c = '/';
            int n = snprintf(txt, sizeof txt, "[rumble]\r\nexe=%s\r\n", exe_barra);
            if (escrever_blob(asi, BLOB_LAUNCHER, BLOB_LAUNCHER_N) && gravar_arquivo(ini, txt, (size_t)n))
                dizer("  a vibracao agora inicia sozinha com o jogo (e fecha junto).");
            else dizer("  falha ao instalar o iniciador automatico");
        }
    }
    if (marcado(ID_FIX)) {
        dizer("\n[Correcoes do Rework]");
        char *av[4] = {"ajustes", pasta, "--sem-pausa", "--teste"};
        chamar(ajustes_main, g_teste ? 4 : 3, av);
    }
    if (marcado(ID_REC40)) {
        dizer("\n[Recarga do nitro 40%% mais lenta]");
        juntar(p, sizeof p, pasta, "GLOBAL\\attributes.bin"); recarga_lenta(p);
    }
    {   /* sempre aplicado: corrige o bug do Rework (limites de infracao impossiveis de atingir) */
        dizer("\n[Limites de infracao de velocidade]");
        juntar(p, sizeof p, pasta, "scripts\\NFSMWExtraOptionsSettings.ini");
        if (arquivo_existe(p)) {
            copiar_backup(p);
            trocar_ini(p, "SpeedingLimit", "40.0"); trocar_ini(p, "ExcessiveSpeedingLimit", "52.0"); trocar_ini(p, "RecklessDrivingLimit", "65.0");
        } else dizer("  NFSMWExtraOptionsSettings.ini nao encontrado");
    }
    if (marcado(ID_DIF)) { dizer("\n[Seletor de dificuldade]"); instalar_dificuldade(pasta); }
    dizer("\n== Pronto. Abra o jogo normalmente. ==");
    SetCursor(LoadCursor(NULL, IDC_ARROW));
}

static void fazer_desinstalar(void) {
    char pasta[MAX_PATH]; if (!pasta_valida(pasta)) return;
    char rdir[MAX_PATH], p[MAX_PATH], q[MAX_PATH];
    juntar(rdir, sizeof rdir, pasta, "scripts\\NFSMWRumble");
    SetCursor(LoadCursor(NULL, IDC_WAIT));
    dizer("== Desinstalando de %s ==", pasta);
    _putenv_s("NFSMW_DIR_PROGRAMA", rdir);
    { char *av[4] = {"instalar", "--desinstalar", "--sem-pausa", "--teste"}; chamar(instalar_main, g_teste ? 4 : 3, av); }
    { char *av[5] = {"ajustes", "--restaurar", pasta, "--sem-pausa", "--teste"}; chamar(ajustes_main, g_teste ? 5 : 4, av); }
    juntar(p, sizeof p, pasta, "GLOBAL\\gameplay.bin"); snprintf(q, sizeof q, "%s.bak_vibracao", p);
    if (arquivo_existe(q) && CopyFileA(q, p, FALSE)) dizer("  restaurado: %s", p);
    juntar(p, sizeof p, pasta, "GLOBAL\\gameplay.lzc"); snprintf(q, sizeof q, "%s.bak_vibracao", p);
    if (arquivo_existe(q) && CopyFileA(q, p, FALSE)) dizer("  restaurado: %s", p);
    static const char *arqs[] = {"scripts\\NFSMWDifficulty.asi", "scripts\\NFSMWDifficulty.log", "scripts\\NFSMWRumbleLauncher.asi", "scripts\\NFSMWRumble.ini",
        "scripts\\NFSMWDifficulty\\gameplay_easy.bin", "scripts\\NFSMWDifficulty\\gameplay_easy.lzc", "scripts\\NFSMWDifficulty\\gameplay_medium.bin",
        "scripts\\NFSMWDifficulty\\gameplay_medium.lzc", "scripts\\NFSMWDifficulty\\gameplay_hard.bin", "scripts\\NFSMWDifficulty\\gameplay_hard.lzc",
        "scripts\\NFSMWRumble\\mw_rumble.exe", "scripts\\NFSMWRumble\\mw_rumble.cfg", "scripts\\NFSMWRumble\\mw_rumble.log"};
    for (size_t i = 0; i < sizeof arqs / sizeof *arqs; i++) { juntar(p, sizeof p, pasta, arqs[i]); apagar(p); }
    juntar(p, sizeof p, pasta, "scripts\\NFSMWDifficulty"); RemoveDirectoryA(p);
    RemoveDirectoryA(rdir);
    dizer("\n== Desinstalado. ==");
    SetCursor(LoadCursor(NULL, IDC_ARROW));
}

/* ------------------------------- janela ------------------------------- */
static void procurar(void) {
    CoInitialize(NULL);
    BROWSEINFOA bi; memset(&bi, 0, sizeof bi);
    bi.hwndOwner = g_wnd; bi.lpszTitle = "Escolha a pasta do jogo (a que tem o speed.exe)";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST il = SHBrowseForFolderA(&bi);
    if (il) { char p[MAX_PATH]; if (SHGetPathFromIDListA(il, p)) SetDlgItemTextA(g_wnd, ID_PASTA, p); CoTaskMemFree(il); }
}
static void pasta_inicial(void) {
    char p[MAX_PATH] = {0};
    DWORD pid = achar_pid("speed.exe");
    if (pid) {
        HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (h) { DWORD n = MAX_PATH; if (QueryFullProcessImageNameA(h, 0, p, &n)) { char *s = strrchr(p, '\\'); if (s) *s = 0; } else p[0] = 0; CloseHandle(h); }
    }
    if (!p[0]) { char d[MAX_PATH], e[MAX_PATH]; dir_do_programa(d, sizeof d); juntar(e, sizeof e, d, "speed.exe"); if (arquivo_existe(e)) snprintf(p, sizeof p, "%s", d); }
    SetDlgItemTextA(g_wnd, ID_PASTA, p);
}
static void desenhar_topo(HDC dc) {
    if (g_icone) DrawIconEx(dc, (590 - 96) / 2, 8, g_icone, 96, 96, 0, NULL, DI_NORMAL);
    RECT r = {0, 106, 590, 122};
    HGDIOBJ antes = SelectObject(dc, g_fonte_pequena);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(100, 100, 100));
    DrawTextA(dc, "by carignato", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, antes);
}
static void utf8_w(const char *s, wchar_t *o, int cap) { if (!MultiByteToWideChar(CP_UTF8, 0, s, -1, o, cap)) o[0] = 0; }
static HWND ctl(const char *cls, const char *txt, DWORD estilo, int x, int y, int w, int h, int id) {
    wchar_t c[32], t[256]; utf8_w(cls, c, 32); utf8_w(txt, t, 256);
    return CreateWindowExW(cls[0] == 'E' ? WS_EX_CLIENTEDGE : 0, c, t, WS_CHILD | WS_VISIBLE | estilo, x, y + ALTURA_TOPO, w, h, g_wnd, (HMENU)(INT_PTR)id, GetModuleHandleA(NULL), NULL);
}
static BOOL CALLBACK por_fonte(HWND h, LPARAM f) { SendMessageA(h, WM_SETFONT, (WPARAM)f, TRUE); return TRUE; }

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        g_wnd = h;
        ctl("STATIC", "Pasta do jogo (a que tem o speed.exe):", 0, 16, 12, 400, 20, 0);
        ctl("EDIT", "", ES_AUTOHSCROLL | WS_TABSTOP, 16, 34, 440, 24, ID_PASTA);
        ctl("BUTTON", "Procurar...", BS_PUSHBUTTON | WS_TABSTOP, 466, 33, 106, 26, ID_PROCURAR);
        ctl("STATIC", "O que instalar:", 0, 16, 72, 300, 20, 0);
        ctl("BUTTON", "Vibração no controle (abre sozinha) + painel de opções no menu Home", BS_AUTOCHECKBOX | WS_TABSTOP, 24, 94, 548, 22, ID_RUMBLE);
        ctl("BUTTON", "Correções do Rework: cutscenes, rastro e recarga do nitro", BS_AUTOCHECKBOX | WS_TABSTOP, 24, 120, 540, 22, ID_FIX);
        ctl("BUTTON", "Seletor de dificuldade ao criar uma carreira (Fácil / Médio / Difícil)", BS_AUTOCHECKBOX | WS_TABSTOP, 24, 144, 548, 22, ID_DIF);
        ctl("BUTTON", "Opcional: recarga do nitro 40% mais lenta", BS_AUTOCHECKBOX | WS_TABSTOP, 24, 168, 540, 22, ID_REC40);
        ctl("BUTTON", "Correção do bug do Rework: limites de infração de velocidade (obrigatória)", BS_AUTOCHECKBOX | WS_TABSTOP, 24, 192, 548, 22, ID_INFRA);
        ctl("BUTTON", "Instalar", BS_DEFPUSHBUTTON | WS_TABSTOP, 16, 226, 130, 32, ID_INSTALAR);
        ctl("BUTTON", "Desinstalar", BS_PUSHBUTTON | WS_TABSTOP, 156, 226, 130, 32, ID_DESINST);
        g_log = ctl("EDIT", "", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, 16, 272, 556, 214, ID_LOG);
        CheckDlgButton(h, ID_RUMBLE, BST_CHECKED); CheckDlgButton(h, ID_INFRA, BST_CHECKED);
        CheckDlgButton(h, ID_FIX, BST_CHECKED); CheckDlgButton(h, ID_DIF, BST_CHECKED);
        g_icone = (HICON)LoadImageA(GetModuleHandleA(NULL), MAKEINTRESOURCEA(1), IMAGE_ICON, 96, 96, 0);
        g_fonte_pequena = CreateFontA(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
        g_fonte = CreateFontA(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
        EnumChildWindows(h, por_fonte, (LPARAM)g_fonte);
        pasta_inicial();
        dizer("Feche o jogo antes de instalar. Cada arquivo alterado ganha um backup (*.bak_vibracao).");
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(w)) {
        case ID_PROCURAR: procurar(); break;
        case ID_INSTALAR: fazer_instalar(); break;
        case ID_DESINST: fazer_desinstalar(); break;
        case ID_INFRA: CheckDlgButton(h, ID_INFRA, BST_CHECKED); break;   /* e correcao de bug: nao da para desmarcar */
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        desenhar_topo(dc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
    (void)prev;
    SetProcessDPIAware();
    INITCOMMONCONTROLSEX ic = {sizeof ic, ICC_STANDARD_CLASSES}; InitCommonControlsEx(&ic);
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = proc; wc.hInstance = inst; wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIconA(inst, MAKEINTRESOURCEA(1));
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1); wc.lpszClassName = "NFSMWPlusSetup";
    RegisterClassA(&wc);
    RECT r = {0, 0, 590, 502 + ALTURA_TOPO}; AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    HWND w = CreateWindowExA(0, wc.lpszClassName, "NFS Most Wanted Rework Plus - Instalador", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                             CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, inst, NULL);
    if (strstr(cmd, "--teste")) {
        g_teste = 1;
        const char *q = strchr(cmd, '"'); char pasta[MAX_PATH] = {0};
        if (q) { snprintf(pasta, sizeof pasta, "%s", q + 1); char *e = strchr(pasta, '"'); if (e) *e = 0; }
        SetDlgItemTextA(w, ID_PASTA, pasta);
        for (int id = ID_RUMBLE; id <= ID_INFRA; id++) CheckDlgButton(w, id, BST_CHECKED);
        fazer_instalar();
        if (strstr(cmd, "--desinstalar")) fazer_desinstalar();
        char lp[MAX_PATH], *b = (char *)malloc(65536); juntar(lp, sizeof lp, pasta, "setup_log.txt");
        int n = GetWindowTextA(g_log, b, 65536); gravar_arquivo(lp, b, (size_t)n); free(b);
        return 0;
    }
    ShowWindow(w, show);
    MSG m;
    while (GetMessageA(&m, NULL, 0, 0) > 0) { if (!IsDialogMessageA(w, &m)) { TranslateMessage(&m); DispatchMessageA(&m); } }
    return 0;
}

/* mw_rumble - vibracao e painel de opcoes para NFS Most Wanted (2005) PC 1.3 (speed.exe).
 *
 * O jogo so usa DirectInput e nao vibra controles Xbox/PS. Este programa fica aberto ao lado do jogo,
 * LE a memoria do speed.exe (somente leitura) e manda a vibracao via XInput para o controle (no DualSense,
 * o DS4Windows repassa ao controle).
 *
 * As opcoes aparecem no menu do ReShade (tecla Home, pagina "NFS_MW_Opcoes"). Este programa le o que foi
 * marcado no preset do ReShade e aplica. Ajustes de arquivo (cutscenes, rastro e recarga do nitro) sao
 * gravados quando o jogo esta fechado e valem na proxima vez que abrir.
 *
 * Uso:  mw_rumble.exe            roda (abra o jogo antes ou depois)
 *       mw_rumble.exe --test     testa os motores do controle por ~3 s e sai
 *       mw_rumble.exe --probe    mostra os valores lidos do jogo (depuracao)
 *       mw_rumble.exe --log      grava eventos em mw_rumble.log
 */
#include "comum.h"
#include <math.h>
#include <stdarg.h>
#include <time.h>
#include <mmsystem.h>

/* ----------------------------- ajustes ----------------------------- */
#define LIMIAR_BATIDA 2.5f   /* queda de velocidade (m/s) em ~0.12 s que conta como batida */
#define FORCA_NITRO   0.50f
#define FORCA_ESTRADA 0.10f
#define FORCA_DERRAPE 0.18f
#define FORCA_POUSO   0.70f
#define TEMPO_PAUSA   0.25   /* s sem nenhuma mudanca na simulacao = jogo pausado */

/* enderecos do speed.exe 1.3 (NFSPluginSDK) e deslocamentos medidos neste jogo */
#define END_ESTADO     0x925E90u   /* GameFlowState; 6 = em jogo */
#define END_EM_NIS     0x91606Cu   /* bool: cutscene/replay */
#define END_SEM_FOCO   0x982C50u   /* bool: janela sem foco */
#define END_VEICULOS   0x9352B0u   /* array de {PVehicle*, bool} */
#define VTBL_PVEHICLE  0x8AA9D8u
#define PV_ENGINE      0xF4u       /* PVehicle -> IEngine* */
#define PV_CAMPOS      0x11Cu      /* mSpeed ... mLocalVel (0x24 bytes) */
#define PV_DRIVERCLASS 0x140u      /* 0 = jogador humano */
#define EN_NOS_BOOST   0xA8u       /* 1.0 normal, >1 com nitro ligado */
#define EN_ROTACAO     0x30u       /* bloco do motor que muda sempre; congela no pause */
#define TAM_PV         0x24
#define TAM_ROT        0x80

/* ----------------------------- estado global ----------------------------- */
static HANDLE g_proc;
static uint32_t g_pv;
static FILE *g_log;
static int g_probe;
static char g_jogo[MAX_PATH], g_preset[MAX_PATH], g_ini_extra[MAX_PATH], g_attr[MAX_PATH];

static void anotar(const char *fmt, ...) {
    char msg[512]; va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof msg, fmt, ap); va_end(ap);
    time_t t = time(NULL); struct tm *lt = localtime(&t); char hora[16]; strftime(hora, sizeof hora, "%H:%M:%S", lt);
    printf("%s %s\n", hora, msg); fflush(stdout);
    if (g_log) { fprintf(g_log, "%s %s\n", hora, msg); fflush(g_log); }
}
static double agora(void) {
    static LARGE_INTEGER f; LARGE_INTEGER c;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
}
static float limitar(float x, float a, float b) { return x < a ? a : x > b ? b : x; }
static float lim01(float x) { return limitar(x, 0.f, 1.f); }

/* ----------------------------- XInput ----------------------------- */
typedef struct { WORD esq, dir; } XVib;
typedef struct { DWORD pacote; WORD botoes; BYTE lt, rt; SHORT lx, ly, rx, ry; } XEstado;
typedef DWORD (WINAPI *FnGet)(DWORD, XEstado *);
typedef DWORD (WINAPI *FnSet)(DWORD, XVib *);
static FnGet XGet; static FnSet XSet;

static int carregar_xinput(void) {
    HMODULE m = LoadLibraryA("xinput1_4.dll");
    if (!m) m = LoadLibraryA("xinput1_3.dll");
    if (!m) return 0;
    XGet = (FnGet)(void *)GetProcAddress(m, "XInputGetState");
    XSet = (FnSet)(void *)GetProcAddress(m, "XInputSetState");
    return XGet && XSet;
}
static volatile int g_idx = -1;      /* controle em uso, para zerar a vibracao ao sair */
static int achar_controle(void) {
    XEstado e;
    for (int i = 0; i < 4; i++) if (XGet(i, &e) == 0) return i;
    return -1;
}
static void vibrar(int idx, float esq, float dir, float escala) {
    XVib v; v.esq = (WORD)(lim01(esq * escala) * 65535.f); v.dir = (WORD)(lim01(dir * escala) * 65535.f);
    XSet((DWORD)idx, &v);
}

/* ----------------------------- memoria do jogo ----------------------------- */
static int ler_mem(uint32_t end, void *dst, size_t n) {
    SIZE_T got = 0;
    return ReadProcessMemory(g_proc, (LPCVOID)(uintptr_t)end, dst, n, &got) && got == n;
}
static int u32(uint32_t end, uint32_t *v) { return ler_mem(end, v, 4); }
static int jogo_vivo(void) { uint32_t x; return u32(END_ESTADO, &x); }

static uint32_t achar_jogador(void) {
    uint32_t t, d;
    if (g_pv && u32(g_pv, &t) && t == VTBL_PVEHICLE && u32(g_pv + PV_DRIVERCLASS, &d) && d == 0) return g_pv;
    g_pv = 0;
    for (int i = 0; i < 48; i++) {
        uint32_t par[2];
        if (!ler_mem(END_VEICULOS + 8u * (uint32_t)i, par, 8)) break;
        uint32_t ptr = par[0];
        if (ptr && u32(ptr, &t) && t == VTBL_PVEHICLE && u32(ptr + PV_DRIVERCLASS, &d) && d == 0) { g_pv = ptr; break; }
    }
    return g_pv;
}

typedef struct {
    float vel, ar, derrapa, boost, envol;
    uint32_t rodas;
    int nis, sem_foco;
    uint8_t vivo[TAM_PV + TAM_ROT];
} Amostra;

/* 1 = em corrida e leu tudo; 0 = fora de corrida */
static int amostrar(Amostra *s) {
    uint32_t est;
    if (!u32(END_ESTADO, &est) || est != 6) return 0;
    uint8_t nis, foco;
    if (!ler_mem(END_EM_NIS, &nis, 1) || !ler_mem(END_SEM_FOCO, &foco, 1)) return 0;
    uint32_t pv = achar_jogador(); if (!pv) return 0;
    uint8_t b[TAM_PV];
    if (!ler_mem(pv + PV_CAMPOS, b, TAM_PV)) return 0;
    float f[5]; uint32_t rodas;
    memcpy(f, b, 20); memcpy(&rodas, b + 20, 4);           /* speed, abs, speedometer, timeInAir, slipAngle, wheelsOnGround */
    s->vel = f[0]; s->ar = f[3]; s->derrapa = f[4]; s->rodas = rodas;
    s->boost = 1.f; s->envol = 0.f;
    memcpy(s->vivo, b, TAM_PV); memset(s->vivo + TAM_PV, 0, TAM_ROT);
    uint32_t eng;
    if (u32(pv + PV_ENGINE, &eng) && eng) {
        float e[2];
        if (ler_mem(eng + EN_NOS_BOOST, e, 8)) { s->boost = e[0]; s->envol = e[1]; }
        ler_mem(eng + EN_ROTACAO, s->vivo + TAM_PV, TAM_ROT);
    }
    s->nis = nis; s->sem_foco = foco;
    return 1;
}

/* ----------------------------- opcoes (menu do ReShade) ----------------------------- */
enum { O_LIGADA, O_INTENS, O_BATIDAS, O_NITRO, O_ESTRADA, O_DERRAPA, O_POUSO, O_PULAR, O_RASTRO, O_RECARGA, O_N };
static const char *NOMES[O_N] = {"Vibracao_Ligada", "Vibracao_Intensidade", "Vibracao_Batidas", "Vibracao_Nitro",
    "Vibracao_Estrada", "Vibracao_Derrapagem", "Vibracao_Pouso", "Jogo_PularCutscenes", "Jogo_RastroNitro", "Jogo_RecargaNitro"};
static const float PADRAO[O_N] = {1, 0.85f, 1, 1, 1, 1, 1, 0, 1, 1};
typedef struct { float v[O_N]; int tem[O_N]; FILETIME mtime; int lido; } Opcoes;

static float op_num(const Opcoes *o, int k) { return o->tem[k] ? o->v[k] : PADRAO[k]; }
static int   op_bool(const Opcoes *o, int k) { return op_num(o, k) >= 0.5f; }

/* 1 se mudou algo */
static int op_atualizar(Opcoes *o) {
    if (!g_preset[0]) return 0;
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (!GetFileAttributesExA(g_preset, GetFileExInfoStandard, &a)) return 0;
    if (o->lido && CompareFileTime(&a.ftLastWriteTime, &o->mtime) == 0) return 0;
    o->mtime = a.ftLastWriteTime; o->lido = 1;
    char *t = ler_arquivo(g_preset, NULL); if (!t) return 0;
    /* recorta a secao [NFS_MW_Opcoes.fx] */
    char *ini = strstr(t, "[NFS_MW_Opcoes.fx]");
    Opcoes novo; memset(&novo, 0, sizeof novo); novo.mtime = o->mtime; novo.lido = 1;
    if (ini) {
        ini = strchr(ini, '\n'); ini = ini ? ini + 1 : (char *)"";
        char *fim = ini;
        while (*fim) { if (*fim == '[' && (fim == ini || fim[-1] == '\n')) break; fim++; }
        char c = *fim; *fim = 0;
        for (int k = 0; k < O_N; k++) {
            char *v = ini_get(ini, NOMES[k]);
            if (v && *v) { novo.v[k] = (float)atof(v); novo.tem[k] = 1; }
            free(v);
        }
        *fim = c;
    }
    free(t);
    int mudou = memcmp(novo.v, o->v, sizeof novo.v) != 0 || memcmp(novo.tem, o->tem, sizeof novo.tem) != 0;
    memcpy(o->v, novo.v, sizeof o->v); memcpy(o->tem, novo.tem, sizeof o->tem);
    return mudou;
}

/* ----------------------------- caminhos do jogo ----------------------------- */
static void definir_jogo(const char *pasta, const char *preset) {
    snprintf(g_jogo, sizeof g_jogo, "%s", pasta);
    char tmp[MAX_PATH];
    juntar(tmp, sizeof tmp, pasta, "SCRIPTS\\NFSMWExtraOptionsSettings.ini"); snprintf(g_ini_extra, sizeof g_ini_extra, "%s", tmp);
    juntar(tmp, sizeof tmp, pasta, "GLOBAL\\attributes.bin");                snprintf(g_attr, sizeof g_attr, "%s", tmp);
    g_preset[0] = 0;
    if (preset && *preset) snprintf(g_preset, sizeof g_preset, "%s", preset);
    else {                          /* descobre o preset pelo ReShade.ini do jogo */
        juntar(tmp, sizeof tmp, pasta, "ReShade.ini");
        char *t = ler_arquivo(tmp, NULL);
        if (t) { char *p = ini_get(t, "PresetPath"); if (p && *p) resolver(g_preset, sizeof g_preset, pasta, p); free(p); free(t); }
    }
}
static void carregar_cfg(void) {
    Cfg c; if (cfg_ler(&c) && pasta_existe(c.jogo)) definir_jogo(c.jogo, c.preset);
}

/* ----------------------------- ajustes de arquivo (jogo fechado) ----------------------------- */
static void ajuste_ini(const char *nome, const char *quer, const char *rotulo) {
    if (!g_ini_extra[0]) return;
    char *t = ler_arquivo(g_ini_extra, NULL); if (!t) return;
    char *atual = ini_get(t, nome);                        /* "0   // comentario..." -> pega so o 1o token */
    if (atual) {
        char tok[32] = {0}; sscanf(atual, "%31s", tok);
        if (strcmp(tok, quer) != 0) {
            char *novo = ini_set_token(t, nome, quer);
            if (gravar_arquivo(g_ini_extra, novo, strlen(novo))) anotar("%s = %s (%s)", nome, quer, rotulo);
            free(novo);
        }
    }
    free(atual); free(t);
}
static void aplicar_ajustes(const Opcoes *o) {
    if (o->tem[O_PULAR])  ajuste_ini("SkipMovies", op_bool(o, O_PULAR) ? "1" : "0", op_bool(o, O_PULAR) ? "cutscenes puladas" : "cutscenes tocam");
    if (o->tem[O_RASTRO]) ajuste_ini("NosTrailRepeatCount", op_bool(o, O_RASTRO) ? "8" : "0", op_bool(o, O_RASTRO) ? "rastro do nitro ligado" : "rastro do nitro desligado");
    if (o->tem[O_RECARGA] && g_attr[0]) {
        int quer = op_bool(o, O_RECARGA), atual = estado_recarga(g_attr);
        if (atual >= 0 && atual != quer) {
            if (gravar_recarga(g_attr, quer)) anotar("recarga do nitro %s", quer ? "corrigida (como no original)" : "como no Rework (sem recarga)");
            else anotar("nao consegui alterar o attributes.bin");
        }
    }
}

/* ----------------------------- laco principal ----------------------------- */
#define NHIST 128
static int rodar(void) {
    carregar_cfg();
    Opcoes op; memset(&op, 0, sizeof op); op_atualizar(&op);
    int idx = -1; int conectado = 0;
    double hist_t[NHIST]; float hist_v[NHIST]; int nh = 0;
    float impacto = 0, pouso = 0; double t_imp = 0, t_pouso = 0;
    float no_ar_antes = 0; uint32_t rodas_antes = 4; int nos_antes = 0;
    uint8_t bloco_antes[TAM_PV + TAM_ROT]; int tem_bloco = 0; double t_mudou = agora(); int em_pausa = 0;
    int vibrando = 0;
    double t_status = 0, t_opcoes = 0; int pendente = 1;
    printf("mw_rumble ativo. Feche a janela (ou Ctrl+C) para sair.\n");
    for (;;) {
        double t = agora();
        if (t - t_opcoes > 0.4) {
            t_opcoes = t;
            if (op_atualizar(&op)) { anotar("opcoes do menu atualizadas"); pendente = 1; }
        }
        if (idx < 0) {
            idx = achar_controle();
            if (idx < 0) { if (t - t_status > 5) { printf("aguardando controle...\n"); t_status = t; } Sleep(1000); continue; }
            g_idx = idx; anotar("controle encontrado no slot %d", idx);
        }
        if (!conectado || !jogo_vivo()) {
            if (conectado) { anotar("jogo fechado"); vibrar(idx, 0, 0, 1); CloseHandle(g_proc); g_proc = NULL; conectado = 0; pendente = 1; g_pv = 0; }
            DWORD pid = achar_pid("speed.exe");
            if (!pid) {
                if (pendente) { aplicar_ajustes(&op); pendente = 0; }
                if (t - t_status > 10) { printf("aguardando speed.exe...\n"); t_status = t; }
                Sleep(1000); continue;
            }
            g_proc = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
            if (!g_proc) { printf("nao consegui abrir o speed.exe (rode como o mesmo usuario do jogo)\n"); Sleep(3000); continue; }
            conectado = 1; pendente = 0; tem_bloco = 0;
            anotar("conectado ao speed.exe (pid %lu)", (unsigned long)pid);
            if (!g_jogo[0]) {
                char caminho[MAX_PATH]; DWORD n = MAX_PATH;
                if (QueryFullProcessImageNameA(g_proc, 0, caminho, &n)) {
                    char *p = strrchr(caminho, '\\'); if (p) *p = 0;
                    definir_jogo(caminho, NULL); anotar("pasta do jogo: %s", g_jogo);
                    Cfg c; memset(&c, 0, sizeof c); snprintf(c.jogo, sizeof c.jogo, "%s", g_jogo); snprintf(c.preset, sizeof c.preset, "%s", g_preset);
                    cfg_gravar(&c);
                    memset(&op, 0, sizeof op); op_atualizar(&op);
                }
            }
        }
        Amostra s; int em_jogo = amostrar(&s);
        int pausado = 0;
        if (em_jogo) {
            if (!tem_bloco || memcmp(s.vivo, bloco_antes, sizeof bloco_antes) != 0) { memcpy(bloco_antes, s.vivo, sizeof bloco_antes); tem_bloco = 1; t_mudou = t; }
            else if (t - t_mudou > TEMPO_PAUSA) pausado = 1;
        }
        int desligada = !op_bool(&op, O_LIGADA);
        if (!em_jogo || s.nis || s.sem_foco || pausado || desligada) {
            if (vibrando) { vibrar(idx, 0, 0, 1); vibrando = 0; }
            if (pausado && !em_pausa) { anotar("jogo pausado"); em_pausa = 1; }
            impacto = pouso = 0; nh = 0;
            Sleep(50); continue;
        }
        if (em_pausa) { anotar("jogo retomado"); em_pausa = 0; }

        /* ----- batida: queda brusca de velocidade em ~0.12 s ----- */
        if (nh == NHIST) { memmove(hist_t, hist_t + 1, (NHIST - 1) * sizeof(double)); memmove(hist_v, hist_v + 1, (NHIST - 1) * sizeof(float)); nh--; }
        hist_t[nh] = t; hist_v[nh] = s.vel; nh++;
        int ini = 0; while (ini < nh && t - hist_t[ini] > 0.12) ini++;
        if (ini) { memmove(hist_t, hist_t + ini, (size_t)(nh - ini) * sizeof(double)); memmove(hist_v, hist_v + ini, (size_t)(nh - ini) * sizeof(float)); nh -= ini; }
        float vmax = hist_v[0]; for (int i = 1; i < nh; i++) if (hist_v[i] > vmax) vmax = hist_v[i];
        float queda = vmax - s.vel;
        if (queda >= LIMIAR_BATIDA) {
            float forca = lim01(0.35f + queda / 18.f);
            if (forca > impacto * 0.9f || t - t_imp > 0.4) {
                if (t - t_imp > 0.4) anotar("batida: queda=%.1f m/s forca=%.2f", queda, forca);
                if (forca > impacto) impacto = forca;
                t_imp = t;
            }
        }
        float imp = impacto > 0 ? lim01(impacto - (float)(t - t_imp) / 0.45f) : 0;
        if (imp <= 0) impacto = 0;
        if (!op_bool(&op, O_BATIDAS)) imp = 0;

        /* ----- pouso depois de um salto ----- */
        if (rodas_antes <= 1 && s.rodas >= 3 && no_ar_antes > 0.25f) { pouso = FORCA_POUSO; t_pouso = t; anotar("pouso: %.2f s no ar", no_ar_antes); }
        no_ar_antes = s.rodas <= 1 ? (s.ar > no_ar_antes ? s.ar : no_ar_antes) : 0;
        rodas_antes = s.rodas;
        float pou = pouso > 0 ? lim01(pouso - (float)(t - t_pouso) / 0.35f) : 0;
        if (!op_bool(&op, O_POUSO)) pou = 0;

        /* ----- nitro: ligado (boost>1) ou o fade depois de soltar ----- */
        int ligado = s.boost > 1.05f;
        if (ligado != nos_antes) { anotar(ligado ? "nitro ligado" : "nitro desligado"); nos_antes = ligado; }
        float nos = (ligado || s.envol > 0.02f) ? FORCA_NITRO * lim01(s.envol) : 0;
        if (!op_bool(&op, O_NITRO)) nos = 0;

        /* ----- estrada e derrapagem (leves) ----- */
        float estrada = (s.rodas >= 3 && op_bool(&op, O_ESTRADA)) ? FORCA_ESTRADA * lim01(s.vel / 60.f) : 0;
        float derr = (s.vel > 8 && op_bool(&op, O_DERRAPA)) ? FORCA_DERRAPE * lim01((fabsf(s.derrapa) - 0.15f) / 0.35f) : 0;

        float esq = imp + pou + nos * 0.55f;
        float dir = imp * 0.8f + pou * 0.6f + nos * 0.45f + estrada + derr;
        float escala = op_num(&op, O_INTENS);
        vibrar(idx, esq, dir, escala); vibrando = 1;
        if (g_probe)
            printf("vel=%5.1f queda=%4.1f ar=%.2f rodas=%u derrapa=%+.2f boost=%.2f envol=%.2f | esq=%.2f dir=%.2f escala=%.2f\n",
                   s.vel, queda, s.ar, s.rodas, s.derrapa, s.boost, s.envol, esq, dir, escala);
        Sleep(16);
    }
    return 0;
}

static int testar(void) {
    int idx = achar_controle();
    if (idx < 0) { printf("nenhum controle XInput encontrado (o DS4Windows esta aberto?)\n"); return 1; }
    printf("controle no slot %d: motor ESQUERDO (grave) 1 s, DIREITO (agudo) 1 s, os dois 1 s\n", idx);
    const float p[3][2] = {{1, 0}, {0, 1}, {1, 1}};
    for (int i = 0; i < 3; i++) { vibrar(idx, p[i][0], p[i][1], 1); Sleep(1000); vibrar(idx, 0, 0, 1); Sleep(400); }
    printf("fim do teste.\n");
    return 0;
}

static BOOL WINAPI ao_sair(DWORD tipo) {
    (void)tipo;
    if (g_idx >= 0 && XSet) { XVib z = {0, 0}; XSet((DWORD)g_idx, &z); }
    return FALSE;                    /* deixa o Windows encerrar normalmente */
}

/* --opcoes: mostra as opcoes do painel como o programa as le (util para conferir a instalacao) */
static int mostrar_opcoes(void) {
    carregar_cfg();
    if (!g_preset[0]) { printf("Sem preset configurado (rode o instalar.exe).\n"); return 1; }
    printf("preset: %s\n", g_preset);
    Opcoes op; memset(&op, 0, sizeof op); op_atualizar(&op);
    for (int k = 0; k < O_N; k++)
        printf("  %-22s = %-8g %s\n", NOMES[k], op_num(&op, k), op.tem[k] ? "(do menu)" : "(padrao)");
    return 0;
}

int main(int argc, char **argv) {
    int teste = 0, log = 0, opcoes = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--opcoes")) opcoes = 1;
        if (!strcmp(argv[i], "--test")) teste = 1;
        else if (!strcmp(argv[i], "--log")) log = 1;
        else if (!strcmp(argv[i], "--probe")) g_probe = 1;
    }
    if (opcoes) return mostrar_opcoes();
    timeBeginPeriod(1);
    if (!carregar_xinput()) { printf("XInput nao encontrado no Windows.\n"); return 1; }
    SetConsoleCtrlHandler(ao_sair, TRUE);
    if (teste) return testar();
    if (log) { char d[MAX_PATH], p[MAX_PATH]; dir_do_programa(d, sizeof d); juntar(p, sizeof p, d, "mw_rumble.log"); g_log = fopen(p, "a"); }
    return rodar();
}

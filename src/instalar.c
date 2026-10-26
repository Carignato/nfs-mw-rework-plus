/* instalar - instalador do painel de opcoes (menu do ReShade, tecla Home) do mod de vibracao do NFS Most Wanted.
 *
 *   instalar.exe                       procura o jogo sozinho (ou pergunta a pasta) e instala
 *   instalar.exe "C:\pasta\do\jogo"    instala nessa pasta (a que tem o speed.exe)
 *   instalar.exe --desinstalar         desfaz tudo (restaura o que foi alterado)
 *   (--sem-pausa nao espera Enter no fim)
 *
 * Feche o jogo antes. Antes de alterar qualquer arquivo e feito backup (*.bak_vibracao).
 *
 * Dois modos, escolhidos sozinho:
 *   integrado  o ReShade ja carrega efeitos (EffectSearchPaths aponta para uma pasta com .fx): o painel entra
 *              nessa pasta e e ativado no preset atual.
 *   dedicado   o ReShade nao carrega nada (EffectSearchPaths vazio/invalido): cria uma pasta e um preset so do
 *              painel e aponta o ReShade para eles, sem ligar os efeitos antigos do preset original.
 */
#include "comum.h"
#include <shlobj.h>
#include "shader_data.h"

#define TECNICA "NFS_MW_Opcoes@NFS_MW_Opcoes.fx"
#define PASTA_DEDICADA "reshade-shaders\\NFSMW_Opcoes"
#define PRESET_DEDICADO "NFS_MW_Opcoes.ini"

static int g_pausa = 1;
static int g_teste = 0;      /* --teste: so para os testes automaticos em pastas falsas; ignora o jogo aberto */

/* ---------- texto dinamico ---------- */
static char *concat(const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b); char *r = (char *)malloc(la + lb + 1);
    memcpy(r, a, la); memcpy(r + la, b, lb + 1); return r;
}
static char *substituir(const char *txt, const char *de, const char *para) {   /* primeira ocorrencia */
    const char *p = strstr(txt, de); if (!p) return _strdup(txt);
    size_t pre = (size_t)(p - txt), ld = strlen(de), lp = strlen(para), pos = strlen(p + ld);
    char *r = (char *)malloc(pre + lp + pos + 1);
    memcpy(r, txt, pre); memcpy(r + pre, para, lp); memcpy(r + pre + lp, p + ld, pos + 1);
    return r;
}
static char *crlf(const char *txt) {                                            /* \n -> \r\n */
    size_t n = 0; for (const char *p = txt; *p; p++) n += (*p == '\n') ? 2 : 1;
    char *r = (char *)malloc(n + 1), *o = r;
    for (const char *p = txt; *p; p++) { if (*p == '\n') *o++ = '\r'; *o++ = *p; }
    *o = 0; return r;
}
static void trim(char *s) {
    size_t n = strlen(s); while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) s[--n] = 0;
    char *p = s; while (*p == ' ' || *p == '\t') p++; if (p != s) memmove(s, p, strlen(p) + 1);
}

/* ---------- lista Techniques= ---------- */
static int tem_item(const char *lista, const char *item) {
    size_t li = strlen(item);
    for (const char *p = lista; *p;) {
        const char *v = strchr(p, ','); size_t n = v ? (size_t)(v - p) : strlen(p);
        if (n == li && strncmp(p, item, li) == 0) return 1;
        if (!v) break;
        p = v + 1;
    }
    return 0;
}
static char *lista_sem(const char *lista, const char *item) {
    char *r = (char *)calloc(strlen(lista) + 1, 1); size_t li = strlen(item);
    for (const char *p = lista; *p;) {
        const char *v = strchr(p, ','); size_t n = v ? (size_t)(v - p) : strlen(p);
        if (n && !(n == li && strncmp(p, item, li) == 0)) { if (*r) strcat(r, ","); strncat(r, p, n); }
        if (!v) break;
        p = v + 1;
    }
    return r;
}
static char *adicionar_tecnica(const char *txt) {
    const char *chaves[2] = {"Techniques", "TechniqueSorting"};
    char *cur = _strdup(txt);
    const char *sec = NULL;                                   /* so olha o cabecalho (antes da primeira secao) */
    for (const char *p = cur; *p; p++) if (*p == '[' && (p == cur || p[-1] == '\n')) { sec = p; break; }
    size_t pos_sec = sec ? (size_t)(sec - cur) : strlen(cur);
    for (int i = 0; i < 2; i++) {
        const char *l = ini_linha(cur, chaves[i]);
        if (l && (size_t)(l - cur) < pos_sec) {
            char *v = ini_get(cur, chaves[i]);
            if (!tem_item(v, TECNICA)) {
                char *nv = *v ? concat(v, "," TECNICA) : _strdup(TECNICA);
                char *n2 = ini_set(cur, chaves[i], nv); free(cur); cur = n2; free(nv);
            }
            free(v);
        }
    }
    const char *l = ini_linha(cur, "Techniques");
    sec = NULL; for (const char *p = cur; *p; p++) if (*p == '[' && (p == cur || p[-1] == '\n')) { sec = p; break; }
    if (!l || (sec && l > sec)) {                             /* sem linha Techniques no cabecalho: cria */
        const char *nl = strstr(cur, "\r\n") ? "\r\n" : "\n";
        size_t pre = sec ? (size_t)(sec - cur) : strlen(cur);
        while (pre && (cur[pre - 1] == '\r' || cur[pre - 1] == '\n')) pre--;
        char *r = (char *)malloc(strlen(cur) + 64);
        memcpy(r, cur, pre); r[pre] = 0;
        strcat(r, nl); strcat(r, "Techniques=" TECNICA); strcat(r, nl);
        strcat(r, sec ? sec : ""); free(cur); cur = r;
    }
    return cur;
}
static char *remover_tecnica(const char *txt) {
    const char *chaves[2] = {"Techniques", "TechniqueSorting"};
    char *cur = _strdup(txt);
    for (int i = 0; i < 2; i++) {
        char *v = ini_get(cur, chaves[i]);
        if (v) { char *nv = lista_sem(v, TECNICA); char *n2 = ini_set(cur, chaves[i], nv); free(cur); cur = n2; free(nv); free(v); }
    }
    return cur;
}

/* ---------- ReShade ---------- */
static int tem_fx(const char *pasta) {
    char padrao[MAX_PATH]; juntar(padrao, sizeof padrao, pasta, "*.fx");
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(padrao, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    FindClose(h); return 1;
}
/* primeira pasta de EffectSearchPaths que existe e tem .fx; 0 se o ReShade nao carrega nada */
static int pasta_de_efeitos(const char *jogo, const char *ini_txt, char *out, size_t cap) {
    char *bruto = ini_get(ini_txt, "EffectSearchPaths"); if (!bruto) return 0;
    int achou = 0;
    for (char *p = bruto; *p && !achou;) {
        char *v = strchr(p, ','); if (v) *v = 0;
        char item[MAX_PATH]; snprintf(item, sizeof item, "%s", p); trim(item);
        size_t n = strlen(item);
        while (n && (item[n - 1] == '*' || item[n - 1] == '\\' || item[n - 1] == '/')) item[--n] = 0;
        if (n) {
            char cand[MAX_PATH]; resolver(cand, sizeof cand, jogo, item);
            if (pasta_existe(cand) && tem_fx(cand)) { snprintf(out, cap, "%s", cand); achou = 1; }
        }
        if (!v) break;
        p = v + 1;
    }
    free(bruto);
    return achou;
}

static char *gerar_shader(const char *jogo, int *recarga_desligada) {
    char ajustes[MAX_PATH], attr[MAX_PATH];
    juntar(ajustes, sizeof ajustes, jogo, "SCRIPTS\\NFSMWExtraOptionsSettings.ini");
    juntar(attr, sizeof attr, jogo, "GLOBAL\\attributes.bin");
    int tem_ajustes = arquivo_existe(ajustes);
    int rec = estado_recarga(attr);                           /* -1 se nao e o Rework v2.0 */
    *recarga_desligada = (rec == 0);
    char *txt = _strdup(SHADER_BASE);
    if (tem_ajustes) {
        char *t = ler_arquivo(ajustes, NULL);
        char *skip = ini_get(t ? t : "", "SkipMovies"), *rastro = ini_get(t ? t : "", "NosTrailRepeatCount");
        char sk[8] = "0", ra[8] = "8"; if (skip) sscanf(skip, "%7s", sk); if (rastro) sscanf(rastro, "%7s", ra);
        char *a = substituir(SHADER_AJUSTES, "%SKIP%", strcmp(sk, "1") == 0 ? "true" : "false");
        char *b = substituir(a, "%RASTRO%", strcmp(ra, "0") == 0 ? "false" : "true");
        char *n = concat(txt, b); free(txt); txt = n; free(a); free(b); free(skip); free(rastro); free(t);
    }
    if (rec >= 0) {
        char *a = substituir(SHADER_RECARGA, "%RECARGA%", rec == 1 ? "true" : "false");
        char *n = concat(txt, a); free(txt); txt = n; free(a);
    }
    char *n = concat(txt, SHADER_FIM); free(txt);
    return n;
}

static int escrever_crlf(const char *path, const char *txt_lf) {
    char *c = crlf(txt_lf); int ok = gravar_arquivo(path, c, strlen(c)); free(c); return ok;
}

/* ---------- instalar / desinstalar ---------- */
static int desinstalar(int silencioso);

static int instalar(const char *pasta) {
    char p[MAX_PATH];
    juntar(p, sizeof p, pasta, "speed.exe");
    if (!arquivo_existe(p)) { printf("Nao achei speed.exe em: %s\n", pasta); return 1; }
    if (!g_teste && achar_pid("speed.exe")) { printf("Feche o jogo (speed.exe) e rode o instalador de novo.\n"); return 1; }
    Cfg anterior;
    if (cfg_ler(&anterior)) { printf("Instalacao anterior encontrada: refazendo do zero.\n"); desinstalar(1); }

    Cfg cfg; memset(&cfg, 0, sizeof cfg); snprintf(cfg.jogo, sizeof cfg.jogo, "%s", pasta);
    char ini_path[MAX_PATH]; juntar(ini_path, sizeof ini_path, pasta, "ReShade.ini");
    if (!arquivo_existe(ini_path)) {
        printf("ReShade nao encontrado nessa pasta. A vibracao funciona, mas sem o painel no menu do Home.\n"
               "Instale o ReShade 4.9.1 (32-bit, DirectX 9) no jogo para ter o painel, e rode este instalador de novo.\n");
        snprintf(cfg.modo, sizeof cfg.modo, "sem-reshade");
        cfg_gravar(&cfg); return 0;
    }
    char *ini_txt = ler_arquivo(ini_path, NULL);
    int recarga_desligada; char *shader = gerar_shader(pasta, &recarga_desligada);
    copiar_backup(ini_path);

    char efeitos[MAX_PATH];
    if (pasta_de_efeitos(pasta, ini_txt, efeitos, sizeof efeitos)) {
        snprintf(cfg.modo, sizeof cfg.modo, "integrado");
        char *pp = ini_get(ini_txt, "PresetPath");
        resolver(cfg.preset, sizeof cfg.preset, pasta, pp && *pp ? pp : ".\\ReShade.ini"); free(pp);
        if (!arquivo_existe(cfg.preset)) {
            printf("Preset do ReShade nao encontrado (%s). Abra o jogo, salve um preset e rode de novo.\n", cfg.preset); return 1;
        }
        juntar(cfg.shader, sizeof cfg.shader, efeitos, "NFS_MW_Opcoes.fx");
        escrever_crlf(cfg.shader, shader);
        copiar_backup(cfg.preset);
        char *pre = ler_arquivo(cfg.preset, NULL); char *novo = adicionar_tecnica(pre);
        gravar_arquivo(cfg.preset, novo, strlen(novo)); free(pre); free(novo);
    } else {
        snprintf(cfg.modo, sizeof cfg.modo, "dedicado");
        juntar(cfg.pasta_dedicada, sizeof cfg.pasta_dedicada, pasta, PASTA_DEDICADA);
        criar_pastas(cfg.pasta_dedicada);
        juntar(cfg.shader, sizeof cfg.shader, cfg.pasta_dedicada, "NFS_MW_Opcoes.fx");
        escrever_crlf(cfg.shader, shader);
        juntar(cfg.preset, sizeof cfg.preset, pasta, PRESET_DEDICADO);
        escrever_crlf(cfg.preset, "Techniques=" TECNICA "\nTechniqueSorting=" TECNICA "\n");
        char *v;
        v = ini_get(ini_txt, "EffectSearchPaths"); snprintf(cfg.effect_paths_original, sizeof cfg.effect_paths_original, "%s", v ? v : ""); free(v);
        v = ini_get(ini_txt, "PresetPath");        snprintf(cfg.preset_path_original, sizeof cfg.preset_path_original, "%s", v ? v : ""); free(v);
        char *a = ini_set(ini_txt, "EffectSearchPaths", ".\\" PASTA_DEDICADA); free(ini_txt); ini_txt = a;
        a = ini_set(ini_txt, "PresetPath", ".\\" PRESET_DEDICADO); free(ini_txt); ini_txt = a;
    }
    char *pm = ini_get(ini_txt, "PerformanceMode");
    if (pm && strcmp(pm, "1") == 0) {                         /* o menu so permite editar valores com o Performance Mode desligado */
        char *a = ini_set(ini_txt, "PerformanceMode", "0"); free(ini_txt); ini_txt = a;
        cfg.performance_mode_original = 1;
        printf("Performance Mode do ReShade desligado (necessario para o menu). Pode custar um pouco de GPU.\n");
    }
    free(pm);
    gravar_arquivo(ini_path, ini_txt, strlen(ini_txt));
    cfg_gravar(&cfg);
    printf("\nInstalado (modo %s) em: %s\n", cfg.modo, pasta);
    if (!strcmp(cfg.modo, "dedicado"))
        printf("Obs.: o ReShade deste jogo nao carregava nenhum efeito (EffectSearchPaths vazio). Foi criado um preset so\n"
               "      para o painel; seus presets/efeitos antigos nao foram ligados nem alterados.\n");
    printf("Abra o mw_rumble.exe, depois o jogo. No jogo, aperte Home: a pagina \"NFS_MW_Opcoes\" aparece.\n");
    if (recarga_desligada)
        printf("Rework detectado: a opcao \"Recarga do nitro\" vem DESLIGADA (estado do mod). Ligue-a no menu para corrigir.\n");
    free(ini_txt); free(shader);
    return 0;
}

static int desinstalar(int silencioso) {
    Cfg cfg;
    if (!cfg_ler(&cfg)) { printf("Nada para desinstalar (mw_rumble.cfg nao encontrado).\n"); return 1; }
    if (!g_teste && achar_pid("speed.exe")) { printf("Feche o jogo (speed.exe) e rode de novo.\n"); return 1; }
    char ini_path[MAX_PATH]; juntar(ini_path, sizeof ini_path, cfg.jogo, "ReShade.ini");
    int tem_ini = arquivo_existe(ini_path);
    int integrado = strcmp(cfg.modo, "dedicado") != 0 && strcmp(cfg.modo, "sem-reshade") != 0;
    if (integrado && cfg.preset[0] && arquivo_existe(cfg.preset)) {
        char *pre = ler_arquivo(cfg.preset, NULL); char *novo = remover_tecnica(pre);
        gravar_arquivo(cfg.preset, novo, strlen(novo)); free(pre); free(novo); printf("Efeito removido do preset.\n");
    }
    if (cfg.shader[0] && arquivo_existe(cfg.shader)) { DeleteFileA(cfg.shader); printf("Shader removido.\n"); }
    if (!strcmp(cfg.modo, "dedicado")) {
        if (tem_ini) {
            char *t = ler_arquivo(ini_path, NULL);
            char *a = ini_set(t, "EffectSearchPaths", cfg.effect_paths_original); free(t);
            t = ini_set(a, "PresetPath", cfg.preset_path_original); free(a);
            gravar_arquivo(ini_path, t, strlen(t)); free(t); printf("EffectSearchPaths e PresetPath restaurados.\n");
        }
        if (cfg.preset[0]) DeleteFileA(cfg.preset);
        if (cfg.pasta_dedicada[0]) RemoveDirectoryA(cfg.pasta_dedicada);
    }
    if (cfg.performance_mode_original && tem_ini) {
        char *t = ler_arquivo(ini_path, NULL); char *a = ini_set(t, "PerformanceMode", "1"); free(t);
        gravar_arquivo(ini_path, a, strlen(a)); free(a); printf("Performance Mode restaurado.\n");
    }
    char c[MAX_PATH]; cfg_caminho(c, sizeof c); DeleteFileA(c);
    if (!silencioso) printf("Desinstalado. (Os ajustes de cutscenes/rastro/recarga ja gravados nos arquivos do jogo continuam como estao.)\n");
    return 0;
}

/* ---------- escolher a pasta do jogo ---------- */
static int achar_pasta(char *out, size_t cap) {
    DWORD pid = achar_pid("speed.exe");
    if (pid) {
        HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (h) {
            char caminho[MAX_PATH]; DWORD n = MAX_PATH;
            if (QueryFullProcessImageNameA(h, 0, caminho, &n)) {
                char *s = strrchr(caminho, '\\'); if (s) *s = 0;
                snprintf(out, cap, "%s", caminho); CloseHandle(h); return 1;
            }
            CloseHandle(h);
        }
    }
    CoInitialize(NULL);
    BROWSEINFOA bi; memset(&bi, 0, sizeof bi);
    bi.lpszTitle = "Escolha a pasta do jogo (a que tem o speed.exe)";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST il = SHBrowseForFolderA(&bi);
    if (il) { char p[MAX_PATH]; if (SHGetPathFromIDListA(il, p)) { snprintf(out, cap, "%s", p); return 1; } }
    printf("Cole o caminho da pasta do jogo (a que tem o speed.exe): ");
    if (fgets(out, (int)cap, stdin)) { trim(out); size_t n = strlen(out); if (n && out[0] == '"') { memmove(out, out + 1, n); out[strlen(out) - 1] = 0; } return out[0] != 0; }
    return 0;
}

int main(int argc, char **argv) {
    int des = 0; char pasta[MAX_PATH] = {0};
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--desinstalar")) des = 1;
        else if (!strcmp(argv[i], "--sem-pausa")) g_pausa = 0;
        else if (!strcmp(argv[i], "--teste")) { g_teste = 1; g_pausa = 0; }
        else if (argv[i][0] != '-') snprintf(pasta, sizeof pasta, "%s", argv[i]);
    }
    int code;
    if (des) code = desinstalar(0);
    else if (pasta[0] || achar_pasta(pasta, sizeof pasta)) code = instalar(pasta);
    else { printf("Nenhuma pasta escolhida.\n"); code = 1; }
    if (g_pausa) { printf("\nEnter para fechar..."); fflush(stdout); getchar(); }
    return code;
}

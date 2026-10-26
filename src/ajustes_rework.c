/* ajustes_rework - aplica/reverte os ajustes de arquivo do NFS Most Wanted Rework (sem precisar do painel do ReShade).
 *
 *   ajustes_rework.exe "C:\pasta\do\jogo"                   aplica os tres ajustes recomendados
 *   ajustes_rework.exe "C:\pasta\do\jogo" --status          mostra o estado atual
 *   ajustes_rework.exe "C:\pasta\do\jogo" --cutscenes=1     (1 = pular cutscenes, 0 = tocar)  [SkipMovies]
 *                                         --rastro=0        (1 = rastro do nitro, 0 = sem)    [NosTrailRepeatCount 8/0]
 *                                         --recarga=0       (1 = como no original, 0 = como no Rework)
 *   ajustes_rework.exe --restaurar "C:\pasta\do\jogo"       volta os arquivos dos backups (*.bak_vibracao)
 *
 * Sem argumentos de ajuste = recomendado: cutscenes tocam, rastro ligado, recarga como no original.
 * Feche o jogo antes. Faz backup (*.bak_vibracao) antes de alterar. Se nao ha pasta, usa a do speed.exe aberto.
 */
#include "comum.h"

static int g_pausa = 1;
static int g_teste = 0;      /* --teste: so para testes automaticos em pastas falsas; ignora o jogo aberto */

static int achar_pasta(char *out, size_t cap) {
    DWORD pid = achar_pid("speed.exe");
    if (pid) {
        HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (h) {
            char c[MAX_PATH]; DWORD n = MAX_PATH;
            if (QueryFullProcessImageNameA(h, 0, c, &n)) { char *s = strrchr(c, '\\'); if (s) *s = 0; snprintf(out, cap, "%s", c); CloseHandle(h); return 1; }
            CloseHandle(h);
        }
    }
    printf("Cole o caminho da pasta do jogo (a que tem o speed.exe): ");
    if (!fgets(out, (int)cap, stdin)) return 0;
    size_t n = strlen(out);
    while (n && (out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == ' ')) out[--n] = 0;
    if (n >= 2 && out[0] == '"' && out[n - 1] == '"') { memmove(out, out + 1, n - 2); out[n - 2] = 0; }
    return out[0] != 0;
}

static void mostrar(const char *ini, const char *attr) {
    char *t = ler_arquivo(ini, NULL);
    if (t) {
        char *s = ini_get(t, "SkipMovies"), *r = ini_get(t, "NosTrailRepeatCount");
        char sk[16] = "?", ra[16] = "?"; if (s) sscanf(s, "%15s", sk); if (r) sscanf(r, "%15s", ra);
        printf("  cutscenes:        SkipMovies = %s  (%s)\n", sk, !strcmp(sk, "1") ? "puladas" : "tocam");
        printf("  rastro do nitro:  NosTrailRepeatCount = %s  (%s)\n", ra, !strcmp(ra, "0") ? "sem rastro" : "com rastro");
        free(s); free(r); free(t);
    } else printf("  (NFSMWExtraOptionsSettings.ini nao encontrado)\n");
    int rec = estado_recarga(attr);
    printf("  recarga do nitro: %s\n", rec == 1 ? "como no original (corrigida)" : rec == 0 ? "como no Rework (sem recarga)" : "indisponivel (attributes.bin ausente ou de outra versao)");
}

static int trocar_ini(const char *ini, const char *chave, const char *valor) {
    char *t = ler_arquivo(ini, NULL); if (!t) { printf("  nao consegui ler %s\n", ini); return 0; }
    char *a = ini_get(t, chave);
    if (!a) { printf("  chave %s nao existe no .ini (versao diferente do ExtraOptions?)\n", chave); free(t); return 0; }
    char tok[32] = {0}; sscanf(a, "%31s", tok); free(a);
    int ok = 1;
    if (strcmp(tok, valor) != 0) {
        char *n = ini_set_token(t, chave, valor);
        ok = gravar_arquivo(ini, n, strlen(n)); free(n);
        printf("  %s: %s -> %s\n", chave, tok, valor);
    } else printf("  %s ja era %s\n", chave, valor);
    free(t);
    return ok;
}

static int restaurar(const char *arq) {
    char b[MAX_PATH + 32]; snprintf(b, sizeof b, "%s.bak_vibracao", arq);
    if (!arquivo_existe(b)) { printf("  sem backup de %s\n", arq); return 0; }
    if (!CopyFileA(b, arq, FALSE)) { printf("  falha ao restaurar %s\n", arq); return 0; }
    printf("  restaurado: %s\n", arq); return 1;
}

int main(int argc, char **argv) {
    char pasta[MAX_PATH] = {0};
    int status = 0, rest = 0, cut = -1, ras = -1, rec = -1;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--status")) status = 1;
        else if (!strcmp(a, "--restaurar")) rest = 1;
        else if (!strcmp(a, "--sem-pausa")) g_pausa = 0;
        else if (!strcmp(a, "--teste")) { g_teste = 1; g_pausa = 0; }
        else if (!strncmp(a, "--cutscenes=", 12)) cut = atoi(a + 12) != 0;
        else if (!strncmp(a, "--rastro=", 9))     ras = atoi(a + 9) != 0;
        else if (!strncmp(a, "--recarga=", 10))   rec = atoi(a + 10) != 0;
        else if (a[0] != '-') snprintf(pasta, sizeof pasta, "%s", a);
    }
    int code = 0;
    if (!pasta[0] && !achar_pasta(pasta, sizeof pasta)) { printf("Nenhuma pasta informada.\n"); code = 1; goto fim; }
    char exe[MAX_PATH], ini[MAX_PATH], attr[MAX_PATH];
    juntar(exe, sizeof exe, pasta, "speed.exe");
    juntar(ini, sizeof ini, pasta, "SCRIPTS\\NFSMWExtraOptionsSettings.ini");
    juntar(attr, sizeof attr, pasta, "GLOBAL\\attributes.bin");
    if (!arquivo_existe(exe)) { printf("Nao achei speed.exe em: %s\n", pasta); code = 1; goto fim; }

    if (status) { printf("Estado em %s:\n", pasta); mostrar(ini, attr); goto fim; }
    if (!g_teste && achar_pid("speed.exe")) { printf("Feche o jogo (speed.exe) e rode de novo.\n"); code = 1; goto fim; }

    if (rest) {
        printf("Restaurando dos backups:\n");
        int a = restaurar(ini), b = restaurar(attr);
        code = (a || b) ? 0 : 1; goto fim;
    }
    if (cut < 0 && ras < 0 && rec < 0) { cut = 0; ras = 1; rec = 1; }      /* recomendado */
    printf("Aplicando em %s:\n", pasta);
    if (cut >= 0 || ras >= 0) {
        if (arquivo_existe(ini)) copiar_backup(ini);
        if (cut >= 0 && !trocar_ini(ini, "SkipMovies", cut ? "1" : "0")) code = 1;
        if (ras >= 0 && !trocar_ini(ini, "NosTrailRepeatCount", ras ? "8" : "0")) code = 1;
    }
    if (rec >= 0) {
        int atual = estado_recarga(attr);
        if (atual < 0) { printf("  recarga: attributes.bin ausente ou de outra versao (so vale para o Rework v2.0, %u bytes) - nada alterado\n", TAM_ATTR_REWORK); code = 1; }
        else if (atual == rec) printf("  recarga do nitro ja estava %s\n", rec ? "como no original" : "como no Rework");
        else {
            copiar_backup(attr);
            if (gravar_recarga(attr, rec)) printf("  recarga do nitro: agora %s\n", rec ? "como no original (100/50)" : "como no Rework (sem recarga)");
            else { printf("  falha ao gravar attributes.bin\n"); code = 1; }
        }
    }
    printf("\nEstado final:\n"); mostrar(ini, attr);
fim:
    if (g_pausa) { printf("\nEnter para fechar..."); fflush(stdout); getchar(); }
    return code;
}

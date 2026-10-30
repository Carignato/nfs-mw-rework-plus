/* Utilidades comuns ao mw_rumble e ao instalar (arquivos, processos, .ini, mw_rumble.cfg). */
#ifndef COMUM_H
#define COMUM_H

#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wformat-truncation"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAM_ATTR_REWORK 688434u
#define NREG_NITRO 4
/* registros do nitro (classe nos) em attributes.bin do Rework v2.0 e valores do Rework / corrigidos */
static const uint32_t REG_NITRO[NREG_NITRO] = {0x8537, 0x8577, 0x8597, 0x85B7};
static const float REG_NITRO_REWORK[NREG_NITRO][2] = {{100, 100}, {100, 100}, {100, 100}, {999, 999}};
static const float NITRO_CORRIGIDO[2] = {100, 50};

/* ---------- arquivos ---------- */
static char *ler_arquivo(const char *path, size_t *tam) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = (char *)malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    size_t lidos = fread(b, 1, (size_t)n, f); fclose(f);
    b[lidos] = 0; if (tam) *tam = lidos;
    return b;
}
static int gravar_arquivo(const char *path, const char *dados, size_t n) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    size_t w = fwrite(dados, 1, n, f); fclose(f);
    return w == n;
}
static int arquivo_existe(const char *p) { DWORD a = GetFileAttributesA(p); return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY); }
static int pasta_existe(const char *p)   { DWORD a = GetFileAttributesA(p); return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY); }

static void juntar(char *dest, size_t cap, const char *a, const char *b) {
    size_t la = strlen(a);
    int sep = la && a[la - 1] != '\\' && a[la - 1] != '/';
    snprintf(dest, cap, "%s%s%s", a, sep ? "\\" : "", b);
}
static void dir_do_programa(char *dest, size_t cap) {
    const char *o = getenv("NFSMW_DIR_PROGRAMA");        /* o instalador unico define isto: cfg/log ficam junto do mw_rumble.exe */
    if (o && *o) { snprintf(dest, cap, "%s", o); return; }
    GetModuleFileNameA(NULL, dest, (DWORD)cap);
    char *p = strrchr(dest, '\\'); if (p) *p = 0;
}
/* cria todas as pastas do caminho */
static void criar_pastas(const char *caminho) {
    char tmp[MAX_PATH]; snprintf(tmp, sizeof tmp, "%s", caminho);
    for (char *p = tmp + 3; *p; p++)
        if (*p == '\\' || *p == '/') { char c = *p; *p = 0; CreateDirectoryA(tmp, NULL); *p = c; }
    CreateDirectoryA(tmp, NULL);
}
/* resolve "base\relativo" (aceita .\x e ..\x); se rel for absoluto, devolve rel */
static void resolver(char *dest, size_t cap, const char *base, const char *rel) {
    char tmp[MAX_PATH * 2];
    if (rel[0] && rel[1] == ':') snprintf(tmp, sizeof tmp, "%s", rel);
    else juntar(tmp, sizeof tmp, base, rel);
    char full[MAX_PATH];
    DWORD n = GetFullPathNameA(tmp, MAX_PATH, full, NULL);
    snprintf(dest, cap, "%s", n ? full : tmp);
}
static int copiar_backup(const char *path) {          /* path -> path.bak_vibracao, sem sobrescrever */
    char b[MAX_PATH + 32]; snprintf(b, sizeof b, "%s.bak_vibracao", path);
    return CopyFileA(path, b, TRUE);
}

/* ---------- processos ---------- */
static DWORD achar_pid(const char *nome) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32 e; e.dwSize = sizeof e; DWORD pid = 0;
    for (BOOL ok = Process32First(s, &e); ok; ok = Process32Next(s, &e))
        if (_stricmp(e.szExeFile, nome) == 0) { pid = e.th32ProcessID; break; }
    CloseHandle(s);
    return pid;
}

/* ---------- .ini ---------- */
/* inicio da linha cuja chave e `chave` (antes de '='), ou NULL. */
static const char *ini_linha(const char *txt, const char *chave) {
    size_t lc = strlen(chave);
    for (const char *p = txt; *p;) {
        if (strncmp(p, chave, lc) == 0) {
            const char *q = p + lc;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == '=') return p;
        }
        const char *nl = strchr(p, '\n');
        if (!nl) break;
        p = nl + 1;
    }
    return NULL;
}
/* valor depois do '=' sem espacos nas pontas (malloc), ou NULL se a chave nao existe */
static char *ini_get(const char *txt, const char *chave) {
    const char *l = ini_linha(txt, chave); if (!l) return NULL;
    const char *q = strchr(l, '='); q++;
    while (*q == ' ' || *q == '\t') q++;
    const char *f = q; while (*f && *f != '\r' && *f != '\n') f++;
    while (f > q && (f[-1] == ' ' || f[-1] == '\t')) f--;
    char *r = (char *)malloc((size_t)(f - q) + 1); memcpy(r, q, (size_t)(f - q)); r[f - q] = 0;
    return r;
}
/* troca TODO o valor da linha (ate o fim dela) */
static char *ini_set(const char *txt, const char *chave, const char *novo) {
    const char *l = ini_linha(txt, chave);
    if (!l) return _strdup(txt);
    const char *q = strchr(l, '=') + 1;
    const char *f = q; while (*f && *f != '\r' && *f != '\n') f++;
    size_t pre = (size_t)(q - txt), ln = strlen(novo), pos = strlen(f);
    char *r = (char *)malloc(pre + ln + pos + 1);
    memcpy(r, txt, pre); memcpy(r + pre, novo, ln); memcpy(r + pre + ln, f, pos + 1);
    return r;
}
/* troca so o primeiro "token" do valor (preserva comentarios depois dele) */
static char *ini_set_token(const char *txt, const char *chave, const char *novo) {
    const char *l = ini_linha(txt, chave);
    if (!l) return _strdup(txt);
    const char *q = strchr(l, '=') + 1;
    while (*q == ' ' || *q == '\t') q++;
    const char *f = q; while (*f && *f != ' ' && *f != '\t' && *f != '\r' && *f != '\n') f++;
    size_t pre = (size_t)(q - txt), ln = strlen(novo), pos = strlen(f);
    char *r = (char *)malloc(pre + ln + pos + 1);
    memcpy(r, txt, pre); memcpy(r + pre, novo, ln); memcpy(r + pre + ln, f, pos + 1);
    return r;
}

/* ---------- mw_rumble.cfg (chave=valor, ao lado dos .exe) ---------- */
typedef struct {
    char jogo[MAX_PATH], preset[MAX_PATH], modo[16], shader[MAX_PATH], pasta_dedicada[MAX_PATH];
    char effect_paths_original[1024], preset_path_original[MAX_PATH];
    int performance_mode_original;     /* 1 se o instalador desligou o Performance Mode */
} Cfg;

static void cfg_caminho(char *dest, size_t cap) {
    char d[MAX_PATH]; dir_do_programa(d, sizeof d); juntar(dest, cap, d, "mw_rumble.cfg");
}
static int cfg_ler(Cfg *c) {
    memset(c, 0, sizeof *c);
    char p[MAX_PATH]; cfg_caminho(p, sizeof p);
    char *t = ler_arquivo(p, NULL); if (!t) return 0;
    char *v;
    if ((v = ini_get(t, "jogo")))                  { snprintf(c->jogo, sizeof c->jogo, "%s", v); free(v); }
    if ((v = ini_get(t, "preset")))                { snprintf(c->preset, sizeof c->preset, "%s", v); free(v); }
    if ((v = ini_get(t, "modo")))                  { snprintf(c->modo, sizeof c->modo, "%s", v); free(v); }
    if ((v = ini_get(t, "shader")))                { snprintf(c->shader, sizeof c->shader, "%s", v); free(v); }
    if ((v = ini_get(t, "pasta_dedicada")))        { snprintf(c->pasta_dedicada, sizeof c->pasta_dedicada, "%s", v); free(v); }
    if ((v = ini_get(t, "effect_paths_original"))) { snprintf(c->effect_paths_original, sizeof c->effect_paths_original, "%s", v); free(v); }
    if ((v = ini_get(t, "preset_path_original")))  { snprintf(c->preset_path_original, sizeof c->preset_path_original, "%s", v); free(v); }
    if ((v = ini_get(t, "performance_mode_original"))) { c->performance_mode_original = atoi(v); free(v); }
    free(t);
    return c->jogo[0] != 0;
}
static int cfg_gravar(const Cfg *c) {
    char p[MAX_PATH]; cfg_caminho(p, sizeof p);
    char b[8192];
    int n = snprintf(b, sizeof b,
        "jogo=%s\r\npreset=%s\r\nmodo=%s\r\nshader=%s\r\npasta_dedicada=%s\r\neffect_paths_original=%s\r\n"
        "preset_path_original=%s\r\nperformance_mode_original=%d\r\n",
        c->jogo, c->preset, c->modo, c->shader, c->pasta_dedicada, c->effect_paths_original,
        c->preset_path_original, c->performance_mode_original);
    return gravar_arquivo(p, b, (size_t)n);
}

/* ---------- attributes.bin do Rework (recarga do nitro) ---------- */
/* 1 = corrigido (100/50), 0 = como no Rework, -1 = arquivo ausente/de outra versao/misto */
static int estado_recarga(const char *attr) {
    size_t n; char *b = ler_arquivo(attr, &n);
    if (!b) return -1;
    int corr = 0, rew = 0;
    if (n == TAM_ATTR_REWORK) {
        for (int i = 0; i < NREG_NITRO; i++) {
            float v[2]; memcpy(v, b + REG_NITRO[i] + 0x18, 8);
            if (v[0] == NITRO_CORRIGIDO[0] && v[1] == NITRO_CORRIGIDO[1]) corr++;
            else if (v[0] == REG_NITRO_REWORK[i][0] && v[1] == REG_NITRO_REWORK[i][1]) rew++;
        }
    }
    free(b);
    if (corr == NREG_NITRO) return 1;
    if (rew == NREG_NITRO) return 0;
    return -1;
}
static int gravar_recarga(const char *attr, int corrigida) {
    size_t n; char *b = ler_arquivo(attr, &n);
    if (!b || n != TAM_ATTR_REWORK) { free(b); return 0; }
    for (int i = 0; i < NREG_NITRO; i++) {
        const float *novo = corrigida ? NITRO_CORRIGIDO : REG_NITRO_REWORK[i];
        memcpy(b + REG_NITRO[i] + 0x18, novo, 8);
    }
    int ok = gravar_arquivo(attr, b, n);
    free(b);
    return ok;
}

#endif

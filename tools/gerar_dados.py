"""Gera src/blobs.h (binarios embutidos no instalador) e src/diftab.h (tabela da dificuldade).

uso: python gerar_dados.py <mw_rumble.exe> <NFSMWRumbleLauncher.asi> <NFSMWDifficulty.asi> <gameplay.bin ORIGINAL> <gameplay.bin com +30%>

A tabela da dificuldade = todos os valores SkillLevel (chave 0x3b798aa2, int) e o float 0xb1ece070 que
diferem entre o gameplay.bin original e o alterado (116 entradas); guarda deslocamento, tipo e valor original.
"""
import os
import struct
import sys

SAIDA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src")
KEYS = {0: struct.pack("<I", 0x3B798AA2), 1: struct.pack("<I", 0xB1ECE070)}


def array_c(nome, dados):
    linhas = []
    for i in range(0, len(dados), 24):
        linhas.append("    " + ",".join("0x%02X" % b for b in dados[i:i + 24]) + ",")
    return "static const unsigned char %s[] = {\n%s\n};\n#define %s_N %d\n" % (nome, "\n".join(linhas), nome, len(dados))


def main():
    rumble, launcher, difasi, orig, mod = sys.argv[1:6]
    out = ["/* gerado por tools/gerar_dados.py - nao editar */\n"]
    for nome, p in (("BLOB_RUMBLE", rumble), ("BLOB_LAUNCHER", launcher), ("BLOB_DIFASI", difasi)):
        out.append(array_c(nome, open(p, "rb").read()))
    open(os.path.join(SAIDA, "blobs.h"), "w", newline="\n").write("\n".join(out))

    o = open(orig, "rb").read()
    m = open(mod, "rb").read()
    assert len(o) == len(m)
    ents = []
    for kind, key in KEYS.items():
        p = 0
        while True:
            p = o.find(key, p)
            if p < 0:
                break
            if o[p + 4:p + 8] != m[p + 4:p + 8]:
                ents.append((p + 4, kind, struct.unpack_from("<I", o, p + 4)[0]))
            p += 1
    ents.sort()
    txt = ["/* gerado por tools/gerar_dados.py - nao editar */",
           "#define DIF_TAM_BIN %du" % len(o),
           "#define DIF_N %d" % len(ents),
           "/* deslocamento, tipo (0 = int SkillLevel, 1 = float), valor original (bits) */",
           "static const uint32_t DIF_TAB[DIF_N][3] = {"]
    for off, kind, v in ents:
        txt.append("    {0x%X, %d, 0x%08X}," % (off, kind, v))
    txt.append("};")
    open(os.path.join(SAIDA, "diftab.h"), "w", newline="\n").write("\n".join(txt) + "\n")
    print("blobs.h e diftab.h gerados;", len(ents), "entradas")


main()

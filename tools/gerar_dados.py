"""Gera src/blobs.h (binarios embutidos no instalador).

uso: python gerar_dados.py <mw_rumble.exe> <NFSMWRumbleLauncher.asi> <NFSMWDifficulty.asi>

Os .asi (src/asi/*.c) sao 32 bits; compile com um clang i686 (ex.: llvm-mingw):
  i686-w64-mingw32-clang -O2 -shared -static -s -nostartfiles -Wl,-e,_DllMain@12 -o NFSMWDifficulty.asi src/asi/difficulty.c
"""
import os
import sys

SAIDA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src")
NL = chr(10)


def array_c(nome, dados):
    linhas = []
    for i in range(0, len(dados), 24):
        linhas.append("    " + ",".join("0x%02X" % b for b in dados[i:i + 24]) + ",")
    return "static const unsigned char %s[] = {%s%s%s};%s#define %s_N %d%s" % (
        nome, NL, NL.join(linhas), NL, NL, nome, len(dados), NL)


def main():
    rumble, launcher, difasi = sys.argv[1:4]
    out = ["/* gerado por tools/gerar_dados.py - nao editar */" + NL]
    for nome, p in (("BLOB_RUMBLE", rumble), ("BLOB_LAUNCHER", launcher), ("BLOB_DIFASI", difasi)):
        out.append(array_c(nome, open(p, "rb").read()))
    open(os.path.join(SAIDA, "blobs.h"), "w", newline=NL).write(NL.join(out))
    print("blobs.h gerado")


main()

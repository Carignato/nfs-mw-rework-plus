# Seletor de dificuldade / Difficulty selector

## O que é

Uma tela nativa **Fácil / Médio / Difícil** que aparece ao criar uma **carreira nova**, logo depois dos avisos de autosave
e antes do pedido do nome do perfil. O jogo original só tem seletor de dificuldade na Corrida Rápida; na carreira ele é adaptativo.

## Como funciona

1. `NFSMWDifficulty.asi` (um plugin ASI de 32 bits, `src/asi/difficulty.c`) intercepta dois pontos do `speed.exe` (versão 1.3, 6.049.792 bytes):
   - a chamada que, depois do 2º aviso de autosave, entra no estado 3 da máquina de estados do cartão de memória (`0x563660`);
   - o despacho das respostas dos diálogos (`0x5640FD` → `0x563380`).
2. Em vez de ir direto para o nome do perfil, abre a tela `MC_ProfileManager.fng` do próprio jogo com **3 botões**
   (`BUTTON1/2/3`; o jogo usa só dois, o terceiro existe na tela e é ligado pelo ASI), com os rótulos do jogo `OPT_SKILL_EASY/MEDIUM/HARD`.
3. A escolha copia `scripts\NFSMWDifficulty\gameplay_<nivel>.bin/.lzc` sobre `GLOBAL\gameplay.bin/.lzc`.
   O jogo lê esses arquivos ao iniciar, então a mudança vale **depois de reiniciar o jogo**. É global (vale para todas as carreiras).

## O que muda nos dados

`GLOBAL\gameplay.bin` (formato VPAK) tem, para cada corrida da carreira (`race_bin_01..15`), as coleções dos oponentes (`character`) e dos
chefes (`boss`). Dois campos escalam com o progresso e são multiplicados:

| Campo | Chave (hash) | Tipo | Escala |
|---|---|---|---|
| `SkillLevel` | `0x3b798aa2` | inteiro (100–700) | × 1,0 / 1,3 / 1,7 |
| (nome desconhecido) | `0xb1ece070` | float 0–1 | × 1,0 / 1,3 / 1,7, teto 1,0 |

São 116 valores (61 inteiros + 55 floats). O instalador gera as três versões a partir do **seu** `gameplay.bin` (que precisa ser o original do Rework;
a tabela de deslocamentos está em `src/diftab.h`, gerada por `tools/gerar_dados.py`). Nenhum arquivo do jogo é distribuído.

> Aviso honesto: o efeito exato desses campos na IA não foi medido de forma objetiva — é a leitura mais coerente dos dados (os chefes e os rivais mais
> avançados têm valores maiores). `SkillLevel` do Razor é 700, o maior do jogo.

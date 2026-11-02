# Seletor de dificuldade / Difficulty selector

## O que é

Uma tela nativa **Fácil / Médio / Difícil** que aparece ao criar uma **carreira nova**, logo depois dos avisos de autosave
e antes do pedido do nome do perfil. O jogo original só tem seletor de dificuldade na Corrida Rápida; na carreira ele é adaptativo
(o jogo ajusta a dificuldade sozinho conforme você vence ou perde).

| Nível | Catch-up (a IA "volta" para perto de você) | Habilidade da IA |
|---|---|---|
| Fácil | desligado (0) | 0,75 (um pouco mais lenta) |
| Médio | pela metade (0,5) | 1,0 |
| Difícil | máximo (1,0) | 1,0 |

O nível fica **gravado dentro da carreira** (no save), então cada carreira tem o seu. Nenhum arquivo do jogo é alterado.
O jogo **deixa de ajustar a dificuldade sozinho**: o nível que você escolheu vale para a carreira inteira.

> Carreiras já existentes não têm escolha: elas ficam com o valor que tinham quando o ASI foi instalado.
> Uma carreira que já subiu ao máximo (o adaptativo empurra para cima quem vence muito) fica no Difícil.

## Como o jogo decide a dificuldade (medido no `speed.exe` 1.3 do Rework)

Na carreira (modo `2` em `GRaceStatus+0x1964`; a Corrida Rápida é o modo `0`):

1. O perfil da carreira guarda um inteiro de 16 bits (`perfil+0xb8`, lido por `0x5dc1a0`). Dividido por 32767 vira `f`, entre −1 e +1.
2. Habilidade da IA: `s = clamp(SkillLevel×0,01) + curva(·)×f`, limitado a 0..1. Como todo oponente tem `SkillLevel` ≥ 100, a primeira parte
   já é 1,0; só um `f` negativo reduz `s` (até 0,75 com `f = −1`). A IA multiplica o acelerador por `0,33 + 0,67×s`.
3. Força do catch-up: `(f + 1) / 2`, de 0 a 1.
4. No fim de **cada corrida** o jogo recalculava `f` com o seu desempenho (`0x5ff7bb` chama o setter `0x5db470`) e gravava no perfil.
   Por isso quem vence muito acaba no máximo (32767). Em uma carreira de teste medimos `0 → 13106 → 17933 → …` depois de cada corrida.

Valores medidos em jogo: carreira nova nasce com `0`; o valor sobrevive a fechar e reabrir o jogo (está no save).

## Como o ASI faz

`NFSMWDifficulty.asi` (plugin ASI de 32 bits, `src/asi/difficulty.c`):

1. **Tela de escolha.** Intercepta dois pontos do `speed.exe` (6.049.792 bytes): a chamada que, depois do 2º aviso de autosave, entra no
   estado 3 da máquina de estados do cartão de memória (`0x563660`) e o despacho das respostas (`0x5640FD` → `0x563380`). Em vez de ir direto
   ao nome, abre a tela `MC_ProfileManager.fng` do jogo com **3 botões** (o terceiro existe na tela e é ligado pelo ASI), com os rótulos
   do jogo `OPT_SKILL_EASY/MEDIUM/HARD`.
2. **Congela a adaptação.** Troca a chamada em `0x5ff7bb` (`call 0x5db470`) por `add esp,4; nop; nop`. O resto da rotina continua igual.
3. **Grava o nível no perfil novo.** Depois da escolha, uma thread espera o perfil da carreira ser recriado (o nome do perfil muda) e grava
   `−32768` (Fácil), `0` (Médio) ou `32767` (Difícil) em `perfil+0xb8`, segurando o valor até o jogo carregar o mundo. Se o perfil novo não
   aparecer em 3 minutos, a escolha é descartada. O que acontece fica em `scripts\NFSMWDifficulty.log`.

Para desfazer: apague `scripts\NFSMWDifficulty.asi` (ou use **Desinstalar**). As carreiras criadas com o seletor continuam com o valor gravado,
e o jogo volta a ajustá-lo sozinho a partir dali.

## Versão 1.1.0: o que estava errado

A v1.1.0 multiplicava por ×1,3 / ×1,7 o `SkillLevel` (`0x3b798aa2`) e um float de nome desconhecido (`0xb1ece070`) em `GLOBAL\gameplay.bin`.
Isso **não tinha efeito**: o jogo limita `SkillLevel × 0,01` a 1,0 (todos os oponentes já passam de 100) e nenhum código do `speed.exe` lê o float.
Quem instalou a v1.1.0 tem o `gameplay.bin` alterado; o instalador novo o devolve ao original.

## Limitações

- Não medi objetivamente quanto a IA fica mais lenta; os números de acelerador vêm da leitura do código. O que foi medido em jogo: o valor do perfil,
  o `f` lido pelo jogo, que ele sobrevive ao save e que não muda mais entre corridas.
- A detecção do "perfil novo" é por mudança do nome do perfil; se você escolher o nível e cancelar a criação, a escolha expira em 3 minutos.

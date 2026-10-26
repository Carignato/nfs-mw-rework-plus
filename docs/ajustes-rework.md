# Ajustes feitos no NFS Most Wanted Rework

Tudo o que foi alterado no jogo (além do mod de vibração), com o valor **antes e depois** e como aplicar na mão.
O `ajustes_rework.exe` faz tudo isso sozinho; o painel do Home (`NFS_MW_Opcoes`) também. Os arquivos do Rework em si
**não** estão neste repositório (são de terceiros): aqui há só as diferenças e as ferramentas.

> Feche o jogo e faça backup antes de editar qualquer arquivo.

`<JOGO>` = pasta do `speed.exe` (ex.: `D:\NFS Most Wanted Rework\Need for Speed - Most Wanted Rework`).

---

## 1) Cutscenes sendo puladas e 2) rastro do nitro ausente

Arquivo: `<JOGO>\SCRIPTS\NFSMWExtraOptionsSettings.ini` (NFSMW ExtraOptions)

| Chave | Antes | Depois | Efeito |
|---|---|---|---|
| `SkipMovies` | `1` | `0` | as cutscenes passam a tocar |
| `NosTrailRepeatCount` | `0` | `8` | o rastro do nitro volta (8 é o padrão do jogo) |

Valores altos de `NosTrailRepeatCount` podem causar bugs gráficos; o padrão (8) é seguro. O texto depois de `//` é comentário.

---

## 3) Nitro que não recarregava

**Causa.** O Rework regravou o `GLOBAL\attributes.bin` (banco de dados VLT do jogo) com a faixa de velocidade de recarga do
nitro em **100/100** (e **999/999** em um tipo). Assim o nitro praticamente nunca recarrega. No jogo original a faixa é
**máxima 100 / mínima 50**.

**Onde.** `<JOGO>\GLOBAL\attributes.bin`, versão do Rework v2.0 com **688.434 bytes**. Cada registro da classe `nos` (nitro)
tem 32 bytes de floats. Neste arquivo os registros estão deslocados 1 byte (começam em endereços ímpares).

| Deslocamento no registro | Campo |
|---|---|
| `+0x04` | `TORQUE_BOOST` |
| `+0x0C` | `RECHARGE_MIN` |
| `+0x10` | `NOS_CAPACITY` |
| `+0x14` | `RECHARGE_MAX` |
| `+0x18` | `RECHARGE_MAX_SPEED` |
| `+0x1C` | `RECHARGE_MIN_SPEED` |

| Registro (endereço no arquivo) | Capacidade | Vel. máx./mín. — Rework | Vel. máx./mín. — corrigido |
|---|---|---|---|
| `0x8537` | 10 | 100 / 100 | **100 / 50** |
| `0x8557` | 0 (sem nitro) | 999 / 999 | não alterado |
| `0x8577` | 13 | 100 / 100 | **100 / 50** |
| `0x8597` | 15 | 100 / 100 | **100 / 50** |
| `0x85B7` | 16 | 999 / 999 | **100 / 50** |

São 4 registros, 2 floats cada (`base+0x18` = `100.0`, `base+0x1C` = `50.0`). O tamanho do arquivo não muda.
Se o arquivo não tiver exatamente 688.434 bytes e esses valores, as ferramentas **não alteram nada**.
Resultado testado em jogo: o nitro volta a recarregar dirigindo acima de ~50 mph.

Ida e volta verificada: aplicar e desfazer devolve o `attributes.bin` **idêntico byte a byte** ao original do Rework
(`tools/testar_ajustes.py`).

---

## Como aplicar

**Com a ferramenta** (`ajustes_rework.exe`, no zip do Release):

```
ajustes_rework.exe "C:\pasta\do\jogo"                  aplica o recomendado (cutscenes tocam, rastro, recarga)
ajustes_rework.exe "C:\pasta\do\jogo" --status         mostra o estado atual
ajustes_rework.exe "C:\pasta\do\jogo" --recarga=0      volta a recarga ao comportamento do Rework
ajustes_rework.exe "C:\pasta\do\jogo" --cutscenes=1 --rastro=0
ajustes_rework.exe "C:\pasta\do\jogo" --restaurar      volta tudo dos backups (*.bak_vibracao)
```

**Pelo painel:** com o `instalar.exe` e o `mw_rumble.exe` do mod, as três opções aparecem em *Ajustes do jogo* no menu Home
e são gravadas com o jogo fechado.

---

## Outras descobertas (úteis para quem usa o Rework)

- **O jogo só usa DirectInput** (`DirectInput8Create` no `speed.exe`, sem XInput). Controle Xbox/PlayStation não vibra sem
  um mod; por isso o mod de vibração existe.
- **O menu do Home é o do ReShade**, não do jogo. Em instalações do Rework o `ReShade.ini` costuma ter `EffectSearchPaths=`
  **vazio**, então o ReShade não carrega nenhum efeito (o menu fica vazio). Se o `Performance Mode` estiver ligado, o menu
  também não deixa editar opções. O instalador do mod contorna isso sem ligar os efeitos antigos.
- **Quando o ReShade não consegue carregar os efeitos do preset, ele pode regravar o preset com a lista de efeitos vazia.**
  Faça backup do preset (`RESHADE FINAL.ini`) antes de mexer.
- **DS4Windows (opcional):** se o DualSense for visto como dois controles, ligue o *Exclusive Mode*
  (`useExclusiveMode` = `True` em `%APPDATA%\DS4Windows\Profiles.xml`, ou na tela de configurações) para o jogo enxergar só o
  controle virtual. Isso **não** faz o jogo vibrar sozinho; a vibração vem do `mw_rumble.exe`.

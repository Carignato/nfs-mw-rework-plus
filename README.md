# NFS Most Wanted Rework Plus

**PT-BR:** melhorias para o *Need for Speed: Most Wanted* (2005) no PC, para o mod **Rework**:
vibração no controle (inicia sozinha com o jogo), painel de opções no menu Home, seletor de dificuldade ao criar a carreira e correções (nitro que não recarrega, cutscenes puladas, rastro do nitro).

**EN:** improvements for *Need for Speed: Most Wanted* (2005) on PC, for the **Rework** mod:
controller rumble (starts automatically with the game), an options panel in the Home menu, a difficulty selector when creating a career, and fixes (nitro not recharging, skipped cutscenes, missing nitro trail).

---

## Instalação (PT-BR)

1. Baixe o `NFSMW_Rework_Plus_Setup.exe` em [**Releases**](../../releases).
2. **Feche o jogo** e execute o instalador. Clique em **Procurar...** e escolha a pasta do jogo (a que tem o `speed.exe`).
3. Marque o que quer instalar e clique em **Instalar**. Pronto: abra o jogo normalmente.

| Opção | O que faz |
|---|---|
| Vibração + painel | instala o `mw_rumble` (vibra em batidas, pousos, nitro, derrapagem...) e o painel do menu Home (ReShade 4.9.1) |
| Iniciar sozinha | a vibração abre escondida com o jogo e fecha junto — sem janela de cmd |
| Correções do Rework | cutscenes tocam, rastro do nitro volta, nitro volta a recarregar |
| Seletor de dificuldade | ao criar uma **carreira nova** aparece a tela **Fácil / Médio / Difícil** (Médio = +30%, Difícil = +70% de habilidade dos oponentes e chefes). Vale depois de reiniciar o jogo |
| Opcional: recarga do nitro 40% mais lenta | deixa a recarga do nitro mais demorada |
| Opcional: limites de infração | multas por velocidade/direção imprudente passam a acontecer em velocidades mais baixas |

O instalador **não distribui arquivos do jogo**: tudo é gerado a partir dos arquivos que você já tem, e cada arquivo alterado ganha um backup (`*.bak_vibracao`). Para desfazer, execute o instalador de novo e clique em **Desinstalar**.

## Install (English)

1. Download `NFSMW_Rework_Plus_Setup.exe` from [**Releases**](../../releases).
2. **Close the game**, run the installer, click **Procurar...** (Browse) and pick the game folder (the one with `speed.exe`).
3. Tick what you want and click **Instalar** (Install). Then just start the game.

The installer is in Portuguese. It does not ship any game files: everything is generated from your own files, each changed file gets a `*.bak_vibracao` backup, and **Desinstalar** (Uninstall) restores them.

---

## Informações / Info

- **PT-BR:** controle Xbox funciona direto; PlayStation via [DS4Windows](https://github.com/Ryochan7/DS4Windows). O painel precisa do ReShade 4.9.1; sem ele a vibração funciona igual. O seletor de dificuldade e as correções valem para o **Rework v2.0** (`speed.exe` 6.049.792 bytes, `attributes.bin` 688.434 bytes); em outras versões o instalador pula o que não reconhece, sem alterar nada. Testado no Windows 11, DualSense. Só jogo offline.
- **EN:** Xbox controllers work directly; PlayStation ones through [DS4Windows](https://github.com/Ryochan7/DS4Windows). The panel needs ReShade 4.9.1; rumble works without it. The difficulty selector and fixes target **Rework v2.0**; on other versions the installer skips what it does not recognise. Tested on Windows 11, DualSense. Offline play only.
- Como funciona a dificuldade / how difficulty works: [`docs/dificuldade.md`](docs/dificuldade.md)
- Detalhes dos ajustes / fix details: [`docs/ajustes-rework.md`](docs/ajustes-rework.md)
- Compilar / build: `construir.bat` (gcc) — Licença / License: [MIT](LICENSE) · Créditos / Credits: [NFSPluginSDK](https://github.com/berkayylmao/NFSPluginSDK), [ReShade](https://reshade.me)

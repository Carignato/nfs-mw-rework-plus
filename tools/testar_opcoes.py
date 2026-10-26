"""Confere se o mw_rumble.exe le certo as opcoes do preset do ReShade (modo --opcoes).
Uso:  python tools/testar_opcoes.py caminho/mw_rumble.exe"""
import os, shutil, subprocess, sys, tempfile

exe_origem = os.path.abspath(sys.argv[1])
base = tempfile.mkdtemp(prefix='opcoes_')
exe = os.path.join(base, 'mw_rumble.exe'); shutil.copy(exe_origem, exe)
jogo = os.path.join(base, 'jogo'); os.makedirs(jogo)
preset = os.path.join(base, 'P.ini')

# como o ReShade grava: secoes por efeito, bool como 0/1, float com 6 casas
open(preset, 'w', newline='').write(
    'Techniques=NFS_MW_Opcoes@NFS_MW_Opcoes.fx\r\n\r\n[Outro.fx]\r\nVibracao_Nitro=1\r\n\r\n'
    '[NFS_MW_Opcoes.fx]\r\nJogo_PularCutscenes=1\r\nJogo_RastroNitro=0\r\nJogo_RecargaNitro=1\r\n'
    'Vibracao_Batidas=0\r\nVibracao_Intensidade=1.250000\r\nVibracao_Ligada=1\r\n\r\n[Depois.fx]\r\nVibracao_Pouso=0\r\n')
open(os.path.join(base, 'mw_rumble.cfg'), 'w', newline='').write(
    f'jogo={jogo}\r\npreset={preset}\r\nmodo=dedicado\r\n')

saida = subprocess.run([exe, '--opcoes'], capture_output=True, text=True).stdout
print(saida)
esperado = {
    'Vibracao_Ligada': (1, True), 'Vibracao_Intensidade': (1.25, True), 'Vibracao_Batidas': (0, True),
    'Vibracao_Nitro': (1, False),            # so existe em OUTRO efeito: deve ficar no padrao
    'Vibracao_Pouso': (1, False),            # so existe na secao seguinte: deve ficar no padrao
    'Jogo_PularCutscenes': (1, True), 'Jogo_RastroNitro': (0, True), 'Jogo_RecargaNitro': (1, True),
}
ok = True
for linha in saida.splitlines():
    partes = linha.split()
    if len(partes) >= 4 and partes[0] in esperado:
        val, do_menu = esperado[partes[0]]
        bom = abs(float(partes[2]) - val) < 1e-6 and (('(do menu)' in linha) == do_menu)
        ok &= bom
        if not bom: print('FALHOU:', linha)
print('RESULTADO:', 'OK' if ok else 'FALHOU')
shutil.rmtree(base, ignore_errors=True)
sys.exit(0 if ok else 1)

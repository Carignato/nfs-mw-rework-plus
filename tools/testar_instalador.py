"""Teste do instalar.exe em pastas de jogo falsas (modo dedicado e integrado), com ida e volta.
Uso:  python tools/testar_instalador.py caminho\\instalar.exe"""
import os, shutil, subprocess, sys, tempfile

BS = chr(92)
exe_origem = os.path.abspath(sys.argv[1])
base = tempfile.mkdtemp(prefix='fakegame_')
pasta_exe = os.path.join(base, 'bin'); os.makedirs(pasta_exe)
exe = os.path.join(pasta_exe, 'instalar.exe'); shutil.copy(exe_origem, exe)

def novo(nome, efeitos):
    d = os.path.join(base, nome)
    os.makedirs(os.path.join(d, 'reshade-shaders', 'Shaders'))
    open(os.path.join(d, 'speed.exe'), 'w').write('x')
    open(os.path.join(d, 'reshade-shaders', 'Shaders', 'V.fx'), 'w').write('x')
    open(os.path.join(d, 'ReShade.ini'), 'w', newline='').write(
        '[GENERAL]\r\nEffectSearchPaths=' + efeitos + '\r\nPerformanceMode=1\r\nPresetPath=.' + BS + 'P.ini\r\n')
    open(os.path.join(d, 'P.ini'), 'w', newline='').write(
        'Techniques=V@V.fx\r\nTechniqueSorting=V@V.fx\r\n\r\n[V.fx]\r\nX=1\r\n')
    return d

def ler(p):
    return open(p, newline='').read()

ok = True
for nome, ef, modo_esperado in (('vazio', '', 'dedicado'), ('valido', '.' + BS + 'reshade-shaders' + BS + 'Shaders', 'integrado')):
    d = novo(nome, ef)
    antes = (ler(os.path.join(d, 'ReShade.ini')), ler(os.path.join(d, 'P.ini')))
    r1 = subprocess.run([exe, d, '--teste'], capture_output=True, text=True)
    cfg = ler(os.path.join(pasta_exe, 'mw_rumble.cfg'))
    modo = [l.split('=', 1)[1] for l in cfg.split('\r\n') if l.startswith('modo=')][0]
    shader = [l.split('=', 1)[1] for l in cfg.split('\r\n') if l.startswith('shader=')][0]
    tem_shader = os.path.isfile(shader)
    texto = ler(shader) if tem_shader else ''
    r2 = subprocess.run([exe, '--desinstalar', '--teste'], capture_output=True, text=True)
    depois = (ler(os.path.join(d, 'ReShade.ini')), ler(os.path.join(d, 'P.ini')))
    sobras = [x for x in ('NFS_MW_Opcoes.ini', os.path.join('reshade-shaders', 'NFSMW_Opcoes')) if os.path.exists(os.path.join(d, x))]
    res = (r1.returncode == 0 and r2.returncode == 0 and modo == modo_esperado and tem_shader
           and 'technique NFS_MW_Opcoes' in texto and 'Vibracao_Pouso' in texto and antes == depois and not sobras
           and not os.path.exists(os.path.join(pasta_exe, 'mw_rumble.cfg')))
    ok &= res
    print(f'{nome:7s} modo={modo:9s} rc={r1.returncode},{r2.returncode} shader={tem_shader} restaurado_identico={antes == depois} sobras={sobras} -> {"OK" if res else "FALHOU"}')
    if not res:
        print(r1.stdout, r1.stderr, r2.stdout, r2.stderr)
shutil.rmtree(base, ignore_errors=True)
sys.exit(0 if ok else 1)

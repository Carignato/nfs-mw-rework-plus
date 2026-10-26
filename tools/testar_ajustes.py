"""Testa o ajustes_rework.exe numa COPIA dos arquivos do Rework (nunca no jogo de verdade).

Uso:  python tools/testar_ajustes.py ajustes_rework.exe <pasta do jogo Rework> <attributes.bin original do Rework>

Confere: recarga 1 -> 0 devolve o arquivo ao original do Rework byte a byte; 0 -> 1 volta ao corrigido;
cutscenes/rastro trocam so o valor (comentarios do .ini intactos) e voltam identicos; --restaurar volta ao backup."""
import hashlib, os, shutil, subprocess, sys, tempfile

exe = os.path.abspath(sys.argv[1]); jogo = sys.argv[2]; attr_rework = sys.argv[3]
base = tempfile.mkdtemp(prefix='ajustes_'); g = os.path.join(base, 'jogo')
os.makedirs(os.path.join(g, 'SCRIPTS')); os.makedirs(os.path.join(g, 'GLOBAL'))
open(os.path.join(g, 'speed.exe'), 'w').write('x')
ini = os.path.join(g, 'SCRIPTS', 'NFSMWExtraOptionsSettings.ini'); attr = os.path.join(g, 'GLOBAL', 'attributes.bin')
shutil.copy(os.path.join(jogo, 'SCRIPTS', 'NFSMWExtraOptionsSettings.ini'), ini)
shutil.copy(os.path.join(jogo, 'GLOBAL', 'attributes.bin'), attr)

def h(p): return hashlib.sha256(open(p, 'rb').read()).hexdigest()
def run(*a): return subprocess.run([exe, g, '--teste', *a], capture_output=True, text=True)
ok = True
def checa(nome, cond):
    global ok
    ok &= bool(cond); print(('OK     ' if cond else 'FALHOU ') + nome)

inicial_ini, inicial_attr = h(ini), h(attr)
estado_inicial = run('--status').stdout
corrigido = 'como no original' in estado_inicial
print(estado_inicial)

if corrigido:
    run('--recarga=0')
    checa('recarga=0 devolve o attributes.bin identico ao original do Rework', h(attr) == h(attr_rework))
    run('--recarga=1')
    checa('recarga=1 volta ao estado corrigido de antes', h(attr) == inicial_attr)
else:
    run('--recarga=1'); run('--recarga=0')
    checa('recarga 1 depois 0 volta ao original do Rework', h(attr) == h(attr_rework))

run('--cutscenes=1'); run('--rastro=0')
t = open(ini, newline='').read()
checa('cutscenes=1 e rastro=0 gravados', 'SkipMovies = 1' in t and 'NosTrailRepeatCount = 0' in t)
run('--cutscenes=0'); run('--rastro=1')
t2 = open(ini, newline='').read()
checa('rastro=1 grava 8', 'NosTrailRepeatCount = 8' in t2)
checa('ini volta identico ao inicial (comentarios preservados)', h(ini) == inicial_ini)
r = run('--restaurar')
checa('--restaurar volta ao backup', r.returncode == 0)
shutil.rmtree(base, ignore_errors=True)
print('RESULTADO:', 'OK' if ok else 'FALHOU')
sys.exit(0 if ok else 1)

"""Gera src/shader_data.h a partir de src/shader/*.fx (o instalar.c embute o shader no .exe).
Uso (so para desenvolvedores, antes de compilar):  python tools/gerar_shader_h.py"""
import os

BARRA = chr(92)
ASPAS = chr(34)
aqui = os.path.dirname(os.path.abspath(__file__))
src = os.path.join(aqui, '..', 'src')
partes = [('SHADER_BASE', '01_base.fx'), ('SHADER_AJUSTES', '02_ajustes.fx'),
          ('SHADER_RECARGA', '03_recarga.fx'), ('SHADER_FIM', '04_fim.fx')]

saida = ['/* Gerado por tools/gerar_shader_h.py a partir dos .fx em src/shader. Nao edite a mao. */\n']
for nome, arq in partes:
    texto = open(os.path.join(src, 'shader', arq), encoding='ascii', newline='').read()
    linhas = texto.split('\n')
    saida.append(f'static const char {nome}[] =\n')
    for i, linha in enumerate(linhas):
        fim = BARRA + 'n' if i < len(linhas) - 1 else ''
        esc = linha.replace(BARRA, BARRA * 2).replace(ASPAS, BARRA + ASPAS).replace('\t', BARRA + 't')
        saida.append(f'    {ASPAS}{esc}{fim}{ASPAS}\n')
    saida.append('    ;\n\n')
open(os.path.join(src, 'shader_data.h'), 'w', encoding='ascii', newline='\n').write(''.join(saida))
print('src/shader_data.h gerado')

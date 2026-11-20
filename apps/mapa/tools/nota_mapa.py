"""MAPA 0.3 — compara o relatorio.txt do teste_v2 com o gabarito do gera_mapa.py, compasso a compasso.
uso: python nota_mapa.py <relatorio.txt> <gabarito.json>
Por elemento: acerto (compassos certos / compassos do gabarito), sobra (marcados a mais) e F1.
"""
import json
import re
import sys

rel = open(sys.argv[1], encoding="utf-8", errors="replace").read().splitlines()
gab = json.load(open(sys.argv[2]))["elementos"]
# projeto com anacruse (audioInicio > 0): o compasso 1 do gabarito é o compasso 2 do projeto
m0 = re.search(r"audioInicio ([\d.]+)", rel[0])
desloc = 1 if m0 and float(m0.group(1)) > 0 else 0
achado = {}
for ln in rel:
    m = re.match(r"\s+faixa (.+?)\s{2,}", ln)
    if not m:
        continue
    nome = m.group(1).strip()
    bars = set()
    for a, b in re.findall(r"\[([\d.]+)-([\d.]+)\]", ln):
        a, b = float(a), float(b)          # compasso 1-based, fim inclusivo
        bars |= set(range(int(round(a)) - 1 - desloc, int(round(b)) - desloc))
    achado[nome] = bars
tot = []
print(f"{'elemento':12s} {'gab':>4s} {'achou':>5s} {'certo':>5s} {'falta':>5s} {'sobra':>5s}   F1")
for el, tr in gab.items():
    g = set()
    for a, z in tr:
        g |= set(range(a, z))
    a = achado.get(el, set())
    certo = len(g & a)
    p = certo / len(a) if a else (1.0 if not g else 0.0)
    r = certo / len(g) if g else 1.0
    f1 = 2 * p * r / (p + r) if p + r else 0
    tot.append(f1)
    print(f"{el:12s} {len(g):4d} {len(a):5d} {certo:5d} {len(g - a):5d} {len(a - g):5d}  {f1:.2f}")
print(f"MÉDIA F1 {sum(tot) / len(tot):.3f}")

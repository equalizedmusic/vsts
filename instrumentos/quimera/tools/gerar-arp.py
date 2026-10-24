#!/usr/bin/env python3
"""Gera Source/presets/arp.inc: os 40 presets da categoria ARP do QUIMERA (banco FACTORY 1).

Uso:  python gerar-arp.py            (reescreve ../Source/presets/arp.inc)

8 famílias de som x 5 presets. Cada preset = som da família + ajustes próprios + arpejo próprio (modo,
divisão, oitavas, gate, swing, LENGTH, passos em pausa e acentos). Os valores seguem o mesmo contrato do
Presets.cpp (valores REAIS; listas = índice); o que não aparece volta ao padrão do parâmetro.
Com SCALE (5 deles), uma tecla só já toca a escala. Passos: string de 16 com 'x' = toca, '.' = pausa; acentos: 'a' = acento, '.' = sem.
"""
from pathlib import Path

# ---------------- famílias de som (chave C++ -> valor C++) ----------------
FAMILIAS = {
    "pluck": dict(
        base=[('O (0, "pos")', "cl::serra"), ('O (0, "uni")', 5), ('O (0, "det")', 22), ('O (0, "wid")', 85),
              ('F (0, "cut")', 700), ('F (0, "res")', 25), ('F (0, "env")', 55), ('E (1, "d")', 220), ('E (1, "s")', 0),
              ('E (0, "a")', "0.5f"), ('E (0, "d")', 380), ('E (0, "s")', 0), ('E (0, "r")', 250),
              ("id::dlyOn", 1), ("id::dlyDiv", "dv::d316"), ("id::dlyFb", 40), ("id::dlyPing", 1), ("id::dlyMix", 22),
              ("id::revOn", 1), ("id::revType", "rv::placa"), ("id::revMix", 15), ("id::volume", -11)],
        mods=[("srcVel", "dstFACut", 30), ("srcMacro1", "dstFACut", 30), ("srcMacro2", "dstFARes", 25),
              ("srcWheel", "dstFACut", 15)]),
    "acido": dict(
        base=[('O (0, "pos")', "cl::serra"), ('F (0, "cut")', 260), ('F (0, "res")', 80), ('F (0, "drv")', 30),
              ('F (0, "env")', 70), ('E (1, "d")', 180), ('E (0, "a")', 1), ('E (0, "r")', 60),
              ("id::polyMode", "pm::legato"), ("id::glide", 60),
              ("id::dstOn", 1), ("id::dstType", "ds::valvula"), ("id::dstDrive", 25), ("id::dstMix", 50), ("id::volume", -11)],
        mods=[("srcVel", "dstFACut", 35), ("srcMacro1", "dstFACut", 35), ("srcMacro2", "dstFARes", 20),
              ("srcMacro3", "dstFADrive", 40)]),
    "sino": dict(
        base=[('O (0, "tab")', "tb::sinos"), ('O (0, "pos")', 40),
              ('O (1, "on")', 1), ('O (1, "tab")', "tb::cristal"), ('O (1, "pos")', 30), ('O (1, "oct")', 1), ('O (1, "lvl")', 35),
              ('F (0, "cut")', 9000), ('F (0, "res")', 10),
              ('E (0, "a")', "0.5f"), ('E (0, "d")', 900), ('E (0, "s")', 0), ('E (0, "r")', 700), ('E (2, "d")', 600), ('E (2, "s")', 0),
              ("id::revOn", 1), ("id::revType", "rv::hall"), ("id::revDecay", "3.5f"), ("id::revMix", 30),
              ("id::dlyOn", 1), ("id::dlyDiv", "dv::d8"), ("id::dlyFb", 35), ("id::dlyMix", 18), ("id::volume", -12)],
        mods=[("srcEnv3", "dstO1Pos", 40), ("srcVel", "dstO1Pos", 30), ("srcMacro1", "dstO1Pos", 40),
              ("srcMacro2", "dstO2Level", 50)]),
    "acorde": dict(
        base=[('O (0, "pos")', "cl::serra"), ('O (0, "uni")', 7), ('O (0, "det")', 30), ('O (0, "wid")', 95),
              ('O (1, "on")', 1), ('O (1, "tab")', "tb::classica"), ('O (1, "pos")', "cl::quadrada"), ('O (1, "oct")', -1),
              ('O (1, "lvl")', 45),
              ('F (0, "cut")', 900), ('F (0, "res")', 30), ('F (0, "env")', 50), ('E (1, "d")', 260),
              ('E (0, "a")', 1), ('E (0, "d")', 300), ('E (0, "s")', 30), ('E (0, "r")', 200),
              ("id::choOn", 1), ("id::choMix", 35), ("id::revOn", 1), ("id::revType", "rv::hall"), ("id::revMix", 20),
              ("id::volume", -14)],
        mods=[("srcVel", "dstFACut", 25), ("srcMacro1", "dstFACut", 35), ("srcMacro2", "dstDetune", 30)]),
    "cristal": dict(
        base=[('O (0, "tab")', "tb::cristal"), ('O (0, "pos")', 50), ('O (0, "uni")', 3), ('O (0, "det")', 15),
              ('F (0, "cut")', 12000),
              ('E (0, "a")', "0.5f"), ('E (0, "d")', 500), ('E (0, "s")', 10), ('E (0, "r")', 900),
              ('L (0, "div")', "dv::d2"), ('L (0, "mode")', "lm::livre"),
              ("id::revOn", 1), ("id::revType", "rv::hall"), ("id::revMix", 35),
              ("id::dlyOn", 1), ("id::dlyDiv", "dv::d316"), ("id::dlyPing", 1), ("id::dlyFb", 50), ("id::dlyMix", 25),
              ("id::volume", -12)],
        mods=[("srcLfo1", "dstO1Pos", 30), ("srcVel", "dstO1Pos", 25), ("srcMacro1", "dstO1Pos", 40)]),
    "orgao": dict(
        base=[('O (0, "tab")', "tb::orgao"), ('O (0, "pos")', 40), ('F (0, "cut")', 6000),
              ('E (0, "a")', 2), ('E (0, "d")', 200), ('E (0, "s")', 70), ('E (0, "r")', 120),
              ("id::choOn", 1), ("id::choMix", 30), ("id::volume", -11)],
        mods=[("srcMacro1", "dstO1Pos", 50), ("srcVel", "dstFACut", 15)]),
    "voz": dict(
        base=[('O (0, "tab")', "tb::vocal"), ('O (0, "pos")', 30), ('O (0, "uni")', 3), ('O (0, "det")', 18),
              ('F (0, "type")', "ft::ladder12"), ('F (0, "cut")', 3000),
              ('E (0, "a")', 2), ('E (0, "d")', 350), ('E (0, "s")', 20), ('E (0, "r")', 300),
              ('L (1, "div")', "dv::c2"), ('L (1, "mode")', "lm::livre"),
              ("id::revOn", 1), ("id::revType", "rv::placa"), ("id::revMix", 25), ("id::volume", -11)],
        mods=[("srcLfo2", "dstO1Pos", 35), ("srcVel", "dstO1Pos", 20), ("srcMacro1", "dstO1Pos", 40)]),
    "glitch": dict(
        base=[('O (0, "tab")', "tb::digital"), ('O (0, "pos")', 60), ('O (0, "warp")', "wp::quantizar"), ('O (0, "wamt")', 40),
              ('F (0, "cut")', 4000), ('F (0, "res")', 30),
              ('E (0, "a")', "0.5f"), ('E (0, "d")', 160), ('E (0, "s")', 0), ('E (0, "r")', 80),
              ("id::dstOn", 1), ("id::dstType", "ds::bitcrush"), ("id::dstDrive", 30), ("id::dstMix", 40),
              ("id::dlyOn", 1), ("id::dlyDiv", "dv::d16"), ("id::dlyFb", 30), ("id::dlyMix", 15), ("id::volume", -12)],
        mods=[("srcRandom", "dstO1Pos", 30), ("srcVel", "dstFACut", 25), ("srcMacro1", "dstO1Warp", 40)]),
}

TODOS = "x" * 16
TEMPO = "a...a...a...a..."


# nome, família, ajustes do som, arpejo (modo, divisão, oitavas, gate, swing, length, acento, passos, acentos)
# scale: índice em scaleNames do Scales.h (0 = CHROMATIC, 1 MAJOR, 2 MINOR, 3 DORIAN, 5 LYDIAN, 10 PENTA MAJ, 13 PHRYG DOM…)
# root: 0 = C … 9 = A
def arp(mode, rate, oct=1, gate=60, swing=0, length=16, acc=50, steps=TODOS, accents="." * 16, scale=0, root=0):
    return dict(mode=mode, rate=rate, oct=oct, gate=gate, swing=swing, length=length, acc=acc, steps=steps, accents=accents,
                scale=scale, root=root)


PRESETS = [
    # ---- pluck de serra ----
    ("Escada de Neon", "pluck", [], arp("arpUp", "dv::d16", 2, 45, acc=50, accents=TEMPO, scale=2, root=9)),
    ("Trem das Seis", "pluck", [('F (0, "cut")', 900)], arp("arpUpDown", "dv::d16", 3, 55, 20, steps="x.xxx.xxx.xxx.xx")),
    ("Varal de Luz", "pluck", [('O (0, "uni")', 7), ("id::dlyDiv", "dv::d8t")], arp("arpAsPlayed", "dv::d8t", 1, 60, length=12)),
    ("Catraca Trance", "pluck", [('F (0, "res")', 45)], arp("arpConverge", "dv::d16", 2, 40, acc=60, accents="..a...a...a...a.")),
    ("Ponte Aérea", "pluck", [('O (0, "oct")', 1), ("id::revMix", 25)], arp("arpDown", "dv::d32", 2, 35, length=8)),
    # ---- ácido mono (GATE 100% = slide) ----
    ("Ácido em Fila", "acido", [], arp("arpUp", "dv::d16", 2, 70, acc=70, accents="a..a..a...a..a..")),
    ("Cobra Coral", "acido", [('F (0, "res")', 86)], arp("arpRandom", "dv::d16", 2, 100, steps="x.xxx.xxx.x.xxx.", accents="a...a.....a.....")),
    ("Gotejamento Ácido", "acido", [('O (0, "pos")', "cl::quadrada")], arp("arpAsPlayed", "dv::d16", 1, 60, 25, acc=65, accents=".a..a..a.a..a..a")),
    ("Rua Augusta 4h", "acido", [("id::dlyOn", 1), ("id::dlyDiv", "dv::d316"), ("id::dlyFb", 30), ("id::dlyMix", 15)],
     arp("arpDownUp", "dv::d16", 2, 80, length=7, accents="a......" + "." * 9)),
    ("Limalha", "acido", [('F (0, "drv")', 55), ("id::dstDrive", 40)], arp("arpDiverge", "dv::d16t", 2, 60, acc=60, accents=TEMPO)),
    # ---- sinos FM ----
    ("Carrilhão de Bolso", "sino", [], arp("arpUp", "dv::d8", 2, 80)),
    ("Sino na Escada", "sino", [('O (1, "lvl")', 50)], arp("arpUpDown", "dv::d16", 3, 60, 30)),
    ("Caixinha de Música", "sino", [('O (0, "oct")', 1), ("id::dlyMix", 10)], arp("arpRandom", "dv::d8t", 2, 70, scale=10, root=0)),
    ("Relógio de Torre", "sino", [('O (0, "oct")', -1), ("id::revMix", 40)], arp("arpDiverge", "dv::d16", 2, 65, steps="x..x..x.x..x..x.")),
    ("Orvalho de Metal", "sino", [('O (0, "tab")', "tb::fmMetalico"), ('O (0, "pos")', 25)], arp("arpDown", "dv::d4", 3, 90)),
    # ---- acordes (modo CHORD) ----
    ("Acorde em Rajada", "acorde", [], arp("arpChord", "dv::d16", 1, 30, steps="x.xxx.xxx.xxx.xx", acc=60, accents=TEMPO)),
    ("Stab de Galpão", "acorde", [('F (0, "cut")', 1300), ("id::dlyOn", 1), ("id::dlyDiv", "dv::d316"), ("id::dlyMix", 20)],
     arp("arpChord", "dv::d8", 2, 40, steps=".x.x.x.x.x.x.x.x")),
    ("Bate-Estaca", "acorde", [("id::cmpOn", 1), ("id::cmpMode", "cp::multibanda"), ("id::cmpAmt", 35)],
     arp("arpChord", "dv::d16", 1, 25, steps="x..x..x.x..x..x.", acc=70, accents="a.......a.......")),
    ("Pulso de Acordes", "acorde", [('O (0, "uni")', 5), ("id::hypOn", 1), ("id::hypMix", 35)],
     arp("arpChord", "dv::d16t", 1, 35, steps="x.x.x.x.x.x.x.x.")),
    ("Coro de Martelos", "acorde", [('O (1, "on")', 0), ('F (0, "env")', 70)], arp("arpChord", "dv::d8t", 2, 50, length=6)),
    # ---- cristal ----
    ("Cristal Giratório", "cristal", [], arp("arpUp", "dv::d32", 3, 30, scale=5, root=5)),
    ("Gota em Espiral", "cristal", [('O (0, "pos")', 70)], arp("arpUpDown", "dv::d16", 4, 50)),
    ("Céu de Lantejoula", "cristal", [("id::revMix", 50)], arp("arpRandom", "dv::d32", 2, 40, steps="xx.xxx.xx.xxx.xx")),
    ("Prisma Lento", "cristal", [('E (0, "r")', 1500)], arp("arpConverge", "dv::d16", 3, 55, 15)),
    ("Chuva de Agulhas", "cristal", [('O (0, "tab")', "tb::harmonicos"), ('O (0, "pos")', 60)], arp("arpAsPlayed", "dv::d8", 2, 70)),
    # ---- órgão ----
    ("Órgão Saltitante", "orgao", [], arp("arpUp", "dv::d8", 1, 70, 35)),
    ("Quermesse", "orgao", [('O (0, "pos")', 65)], arp("arpUpDown", "dv::d8t", 2, 60)),
    ("Realejo", "orgao", [("id::choMix", 50)], arp("arpAsPlayed", "dv::d16", 1, 55, 40, acc=40, accents=TEMPO)),
    ("Igrejinha em Fuga", "orgao", [("id::revOn", 1), ("id::revType", "rv::hall"), ("id::revMix", 35)], arp("arpDown", "dv::d16", 2, 65)),
    ("Tecla de Feira", "orgao", [('O (0, "pos")', 20)], arp("arpChord", "dv::d8", 1, 40, steps=".x.x.x.x.x.x.x.x")),
    # ---- vozes ----
    ("Coral Picado", "voz", [], arp("arpUp", "dv::d8", 2, 90)),
    ("Sílabas no Vento", "voz", [('O (0, "pos")', 55)], arp("arpRandom", "dv::d16", 2, 70, steps="x.xx.xx.x.xx.xx.")),
    ("Vozes em Roda", "voz", [("id::choOn", 1), ("id::choMix", 30)], arp("arpDownUp", "dv::d8t", 2, 75)),
    ("Mantra em Degraus", "voz", [('E (0, "a")', 40), ("id::revMix", 40)], arp("arpUpDown", "dv::d4", 1, 100, scale=3, root=2)),
    ("Eco de Catedral", "voz", [("id::dlyOn", 1), ("id::dlyDiv", "dv::d38"), ("id::dlyFb", 45), ("id::dlyMix", 25)],
     arp("arpDiverge", "dv::d16", 3, 60, length=10)),
    # ---- glitch digital ----
    ("Bit Saltador", "glitch", [], arp("arpRandom", "dv::d32", 3, 30, steps="xx.x.xxx.xx.x.xx", accents="a..a...a..a....a", scale=13, root=4)),
    ("Curto-Circuito", "glitch", [('O (0, "wamt")', 70)], arp("arpUp", "dv::d16t", 4, 25)),
    ("Dado Viciado", "glitch", [("id::dstType", "ds::downsample")], arp("arpConverge", "dv::d32", 2, 40, acc=70, accents=TEMPO)),
    ("Morse Quebrado", "glitch", [('F (0, "res")', 55)], arp("arpAsPlayed", "dv::d16", 1, 45, length=5)),
    ("Fliperama", "glitch", [('O (0, "tab")', "tb::eletrico"), ('O (0, "pos")', 30)], arp("arpDown", "dv::d16", 3, 50, 50)),
]


def num(v):
    if isinstance(v, (int, float)):
        return str(v)
    return v


def gerar():
    linhas = ["// ARP — 40 presets com o arpejador ligado (gerado por tools/gerar-arp.py; mexa lá e gere de novo)"]
    nomes = set()
    for nome, fam, ajustes, a in PRESETS:
        assert nome not in nomes, nome
        nomes.add(nome)
        assert len(a["steps"]) == 16 and len(a["accents"]) == 16, nome
        kv = dict(FAMILIAS[fam]["base"])
        for k, v in ajustes:
            kv[k] = v
        itens = [f"{{ {k}, {num(v)} }}" for k, v in kv.items()]
        itens += [f"{{ id::arpOn, 1 }}", f"{{ id::arpMode, (float) {a['mode']} }}", f"{{ id::arpRate, {a['rate']} }}",
                  f"{{ id::arpOct, {a['oct']} }}", f"{{ id::arpGate, {a['gate']} }}", f"{{ id::arpSwing, {a['swing']} }}",
                  f"{{ id::arpLen, {a['length']} }}", f"{{ id::arpAcc, {a['acc']} }}"]
        if a["scale"]:
            itens += [f"{{ id::arpScale, {a['scale']} }}", f"{{ id::arpRoot, {a['root']} }}"]
        itens += [f"{{ arpStepId ({i}), 0 }}" for i, c in enumerate(a["steps"]) if c == "."]
        itens += [f"{{ arpAccId ({i}), 1 }}" for i, c in enumerate(a["accents"]) if c == "a"]
        mods = ", ".join(f"{{ {s}, {d}, {v} }}" for s, d, v in FAMILIAS[fam]["mods"])
        corpo = []
        linha = "        { "
        for i, it in enumerate(itens):
            pedaco = it + (", " if i < len(itens) - 1 else " },")
            if len(linha) + len(pedaco) > 124:
                corpo.append(linha.rstrip())
                linha = "          "
            linha += pedaco
        corpo.append(linha)
        linhas.append(f'    p.push_back (mk (cArp, u8 ("{nome}"),')
        linhas += corpo
        linhas.append(f"        {{ {mods} }}));")
        linhas.append("")
    saida = Path(__file__).resolve().parent.parent / "Source" / "presets" / "arp.inc"
    saida.write_text("\n".join(linhas).rstrip() + "\n", encoding="utf-8", newline="\n")
    print(f"{len(PRESETS)} presets -> {saida}")


if __name__ == "__main__":
    gerar()

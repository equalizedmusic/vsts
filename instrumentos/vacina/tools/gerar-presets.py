#!/usr/bin/env python3
"""Gera o banco FACTORY 1 do VACINA (FM de 4 operadores): Source/presets/fabrica.inc (9 categorias × 40 presets).

Uso:
    python gerar-presets.py                 # escreve fabrica.inc (com o volume nivelado em niveis.txt, se existir)
    python gerar-presets.py --lista X.txt   # escreve também a lista completa (todos os parâmetros) para o harness medir

Cada categoria tem 8 ARQUÉTIPOS (famílias de som FM: sino tubular, e-piano, baixo de borracha…) e cada arquétipo tem
5 nomes escritos à mão; cada nome é uma variação do arquétipo (semente fixa pelo nome — rodar de novo dá o mesmo arquivo).
O volume de cada preset é acertado pela medida do harness (niveis.txt: nome;dB a somar), para todos soarem parelhos.
Valores REAIS (iguais ao estado salvo pelo plugin); o que o preset não traz volta ao padrão do parâmetro.

Lembrete de FM: o OP 1 é sempre portador; quem modula tem número maior (ver algos em Params.h).
  ALGO 1 4>3>2>1 · 2 (3+4)>2>1 · 3 3>2>1 + 4>1 · 4 4>3>1 + 2>1 · 5 2>1 + 4>3 · 6 4>1,2,3 · 7 4>3 + 1, 2 · 8 os 4 soltos
  (no script os algoritmos são 0..7). razão = COARSE (0 = ×0,5) × (1 + FINE %); inteira = harmônico, quebrada = metal.
"""
import random
import sys
import zlib
from pathlib import Path

AQUI = Path(__file__).resolve().parent
CATEGORIAS = ["BASS", "LEAD", "PAD", "PLUCK", "BELLS", "KEYS", "ARP", "TEXTURA", "FX"]

# padrões e faixas (iguais ao createLayout de Params.h)
PADRAO = {
    "volume": -6, "keymode": 0, "porta": 0, "bendrange": 2, "drift": 0, "unison": 1, "unidetune": 25, "unispread": 60,
    "algo": 0, "feedback": 0, "fold": 0, "ring": 0, "noisevol": 0, "oscvol": 50,
    "cutoff": 20000, "cutoff2": 0, "filtbal": 0, "routing": 0, "f1mode": 0, "f2mode": 0, "reso1": 0, "reso2": 20,
    "env1": 0, "env2": 0, "key1": 0, "key2": 0, "sattype": 0, "satamt": 30,
    "fa": 2, "fd": 450, "fs": 30, "fr": 350, "fvel": 30, "fslope": 0,
    "aa": 2, "ad": 600, "as": 100, "ar": 300, "avel": 40, "aslope": 0,
    "l1rate": 5, "l1shape": 0, "l1sync": 0, "l1pitch": 0, "l1cut": 0, "l1ratio": 0, "l1amp": 0,
    "l2rate": 0.4, "l2shape": 1, "l2sync": 0, "l2index": 0, "l2fb": 0, "l2cut": 0, "l2pan": 0,
    "l3rate": 1.5, "l3shape": 0, "l3sync": 0, "l3dst": 0, "l3amt": 0,
    "m1src": 1, "m1dst": 5, "m1amt": 30, "m2src": 0, "m2dst": 0, "m2amt": 0, "m3src": 0, "m3dst": 0, "m3amt": 0,
    "m4src": 0, "m4dst": 0, "m4amt": 0, "m5src": 0, "m5dst": 0, "m5amt": 0, "m6src": 0, "m6dst": 0, "m6amt": 0,
    "arpon": 0, "arpmode": 0, "arpoct": 1, "arprate": 2, "arpgate": 55, "arphold": 0,
    "dlysend": 0, "dlytime": 3, "dlyfb": 40, "rvbsend": 0, "rvbsize": 55, "rvbdamp": 45,
    "eqlow": 0, "eqmid": 0, "eqhigh": 0, "disttype": 0, "distamt": 0, "boostamt": 0, "boosttune": 100,
    "chomix": 0, "choint": 45, "phamix": 0, "phaint": 50,
}
for _k in range(1, 5):
    PADRAO.update({f"op{_k}wave": 0, f"op{_k}coarse": 1, f"op{_k}fine": 0, f"op{_k}det": 0, f"op{_k}lvl": 100 if _k == 1 else 0,
                   f"op{_k}vel": 0, f"op{_k}a": 1, f"op{_k}d": 800, f"op{_k}s": 100, f"op{_k}r": 300})

FAIXAS = {
    "volume": (-36, 12), "keymode": (0, 2), "porta": (0, 2000), "bendrange": (1, 24), "unison": (1, 4),
    "algo": (0, 7), "cutoff": (20, 20000), "cutoff2": (-48, 48), "routing": (0, 1), "f1mode": (0, 6), "f2mode": (0, 5),
    "env1": (-100, 100), "env2": (-100, 100), "sattype": (0, 6),
    "fa": (0.5, 10000), "fd": (1, 10000), "fr": (1, 10000), "aa": (0.5, 10000), "ad": (1, 10000), "ar": (1, 10000),
    "fslope": (-100, 100), "aslope": (-100, 100),
    "l1rate": (0.02, 30), "l2rate": (0.02, 30), "l3rate": (0.02, 30), "l1shape": (0, 5), "l2shape": (0, 5), "l3shape": (0, 5),
    "l1sync": (0, 8), "l2sync": (0, 8), "l3sync": (0, 8), "l3dst": (0, 20),
    "l1pitch": (-100, 100), "l1cut": (-100, 100), "l1ratio": (-100, 100), "l2index": (-100, 100), "l2fb": (-100, 100),
    "l2cut": (-100, 100), "l2pan": (-100, 100), "l3amt": (-100, 100),
    "arpon": (0, 1), "arpmode": (0, 5), "arpoct": (1, 4), "arprate": (0, 5), "arpgate": (5, 100), "arphold": (0, 1),
    "dlytime": (0, 6), "dlyfb": (0, 95), "eqlow": (-12, 12), "eqmid": (-12, 12), "eqhigh": (-12, 12), "disttype": (0, 3),
    "boosttune": (40, 400),
}
for _k in range(1, 5):
    FAIXAS.update({f"op{_k}wave": (0, 7), f"op{_k}coarse": (0, 16), f"op{_k}det": (-50, 50),
                   f"op{_k}a": (0.5, 10000), f"op{_k}d": (1, 20000), f"op{_k}r": (1, 10000)})
for _s in range(1, 7):
    FAIXAS[f"m{_s}src"] = (0, 10); FAIXAS[f"m{_s}dst"] = (0, 20); FAIXAS[f"m{_s}amt"] = (-100, 100)
INTEIROS = {"keymode", "bendrange", "unison", "algo", "cutoff2", "routing", "f1mode", "f2mode", "sattype",
            "l1shape", "l1sync", "l2shape", "l2sync", "l3shape", "l3sync", "l3dst",
            "arpon", "arpmode", "arpoct", "arprate", "arphold", "dlytime", "disttype"} \
           | {f"m{s}{k}" for s in range(1, 7) for k in ("src", "dst")} \
           | {f"op{k}{p}" for k in range(1, 5) for p in ("wave", "coarse")}

# listas (índices iguais a Params.h)
W = {n: i for i, n in enumerate(["SINE", "HALF", "ABS", "PULSE", "ALT", "CAMEL", "SQUARE", "SAW"])}
SRC = {n: i for i, n in enumerate(["OFF", "WHEEL", "AFTER", "VEL", "KEY", "LFO1", "LFO2", "LFO3", "FENV", "AENV", "RANDOM"])}
DST = {n: i for i, n in enumerate(["OFF", "CUTOFF", "CUTOFF2", "RESO", "PITCH", "INDEX", "OP1", "OP2", "OP3", "OP4",
                                    "RATIO2", "RATIO3", "RATIO4", "FEEDBACK", "FOLD", "RING", "NOISE", "SAT", "FILTBAL",
                                    "PAN", "VOLUME"])}
F1 = {"LP24": 0, "LP12": 1, "HP": 2, "BP": 3, "BS": 4, "COMB": 5, "VOWEL": 6}
SAT = {"OFF": 0, "LIGHT": 1, "TUBE": 2, "HARD": 3, "FOLD": 4, "BITS": 5, "RATE": 6}
LFO = {"SINE": 0, "TRI": 1, "SAW": 2, "SQUARE": 3, "SH": 4, "SG": 5}
SYNC = {"OFF": 0, "1/16": 1, "1/8T": 2, "1/8": 3, "1/4": 4, "1/2": 5, "1BAR": 6, "2BARS": 7, "4BARS": 8}
ARP = {"UP": 0, "DOWN": 1, "UPDOWN": 2, "RANDOM": 3, "PLAYED": 4, "CHORD": 5}
RATE = {"1/32": 0, "1/16T": 1, "1/16": 2, "1/8T": 3, "1/8": 4, "1/4": 5}


class V:
    """Ajudante de variação: faixas e escolhas com a semente do nome."""
    def __init__(self, nome):
        self.r = random.Random(zlib.crc32(nome.encode("utf-8")))

    def __call__(self, a, b):
        return a + (b - a) * self.r.random()

    def c(self, *op):
        return self.r.choice(op)


def P(**kw):
    """dict com 'as' (sustain do amp) escrito como as_ (as é palavra reservada)."""
    if "as_" in kw:
        kw["as"] = kw.pop("as_")
    return kw


def op(v, k, w=None, c=None, f=None, det=None, lvl=None, vel=None, a=None, d=None, s=None, r=None):
    """Acerta o operador k (1..4). c = COARSE (0 = ×0,5), f = FINE %, lvl = nível %, a/d/r em ms, s = sustain %."""
    for nome, x in (("wave", w), ("coarse", c), ("fine", f), ("det", det), ("lvl", lvl), ("vel", vel),
                    ("a", a), ("d", d), ("s", s), ("r", r)):
        if x is not None:
            v[f"op{k}{nome}"] = W[x] if nome == "wave" and isinstance(x, str) else x
    return v


def mat(v, *slots):
    """Liga a matriz: (fonte, destino, quantidade), a partir do slot 2 (o 1 é a roda → FM INDEX)."""
    for i, (s, d, a) in enumerate(slots):
        v[f"m{i + 2}src"], v[f"m{i + 2}dst"], v[f"m{i + 2}amt"] = SRC[s], DST[d], a
    return v


def arp(v, x, modo=None, rate=None, octs=None, gate=None):
    v.update(arpon=1, arpmode=ARP[modo] if modo else x.c(0, 2, 3), arprate=RATE[rate] if rate else x.c(2, 2, 1, 4),
             arpoct=octs if octs else x.c(1, 2, 2, 3), arpgate=gate if gate is not None else x(30, 70))
    return v


def quebrada(x):
    """FINE de uma razão inarmônica (metal): longe dos valores que caem num harmônico."""
    return x.c(41, 73, 27, 59, 14, 87, 33, 66) * x(0.92, 1.08) if x.r.random() < 0.7 else x(15, 85)


# =====================================================================================================
# ARQUÉTIPOS. A receita recebe o ajudante x e devolve o que muda do padrão.
# =====================================================================================================

# ---------------- BASS ----------------
def bass_dx(x):
    """baixo FM clássico: o modulador 1:1 cai rápido (o 'tapa'), a realimentação dá o rosnado."""
    v = P(algo=x.c(0, 4), feedback=x(20, 55), keymode=x.c(0, 1), aa=0.5, ar=x(60, 160), avel=60)
    op(v, 1, c=x.c(0, 1), lvl=100, d=x(900, 2500), s=x(40, 80), r=120)
    op(v, 2, c=x.c(1, 1, 2), lvl=x(60, 82), vel=x(30, 60), d=x(140, 380), s=x(5, 25), r=100)
    op(v, 3, c=1, lvl=x(30, 60), d=x(200, 600), s=x(10, 40))
    op(v, 4, c=x.c(1, 3), lvl=x(25, 55), d=x(100, 300), s=0)
    return v


def bass_rubber(x):
    """baixo de borracha: meia-senoide, a altura despenca no ataque (FILT ENV → PITCH)."""
    v = P(algo=x.c(4, 0), keymode=1, porta=x(0, 30), fd=x(40, 110), fs=0, fa=0.5, aa=0.5, ar=x(70, 150),
          feedback=x(0, 25))
    op(v, 1, w=x.c("HALF", "ABS", "SINE"), c=x.c(0, 1), lvl=100, d=x(500, 1400), s=x(30, 70))
    op(v, 2, w=x.c("SINE", "HALF"), c=1, lvl=x(45, 70), d=x(150, 400), s=x(10, 30))
    return mat(v, ("FENV", "PITCH", x(8, 16)), ("VEL", "INDEX", x(10, 25)))


def bass_wobble(x):
    """wobble FM: o LFO 2 no tempo abre e fecha o índice."""
    v = P(algo=0, keymode=x.c(1, 2), porta=x(20, 60), feedback=x(30, 70), l2sync=x.c(3, 4, 2), l2shape=x.c(0, 1, 2),
          l2index=x(35, 60), aa=1, ar=x(60, 120), sattype=SAT["TUBE"], satamt=x(30, 55), cutoff=x(1800, 5000),
          f1mode=F1["LP24"], reso1=x(15, 35), key1=50)
    op(v, 1, c=x.c(0, 1), lvl=100)
    op(v, 2, c=x.c(1, 2), f=x.c(0, 0, 50), lvl=x(25, 45))
    op(v, 3, c=x.c(1, 3), lvl=x(30, 55))
    op(v, 4, c=x.c(1, 2), lvl=x(20, 45))
    return v


def bass_metal(x):
    """baixo de metal: modulador com razão quebrada, dobra de onda por cima."""
    v = P(algo=x.c(0, 2), keymode=1, feedback=x(10, 40), fold=x(15, 40), aa=0.5, ar=x(50, 120), cutoff=x(1200, 3500),
          f1mode=F1["LP12"], reso1=x(10, 30), env1=x(20, 40), fd=x(150, 400), fs=x(10, 30), key1=60)
    op(v, 1, c=0, lvl=100, d=x(800, 2000), s=60)
    op(v, 2, c=x.c(1, 2), f=quebrada(x), lvl=x(40, 65), d=x(200, 600), s=x(20, 45))
    op(v, 3, c=x.c(3, 5), f=x.c(0, 41), lvl=x(20, 45), d=x(100, 300), s=0)
    return mat(v, ("VEL", "FOLD", x(10, 30)))


def bass_acid(x):
    """ácido FM: mono com glide, filtro ressonante e o índice caindo com o envelope do filtro."""
    v = P(algo=0, keymode=2, porta=x(40, 90), cutoff=x(220, 500), reso1=x(70, 88), env1=x(45, 75), fa=1, fd=x(150, 320),
          fs=0, fr=60, fvel=x(40, 70), aa=0.5, ar=60, sattype=x.c(2, 3), satamt=x(40, 70), key1=40,
          dlysend=x(0, 16), dlytime=3, feedback=x(15, 40))
    op(v, 1, w=x.c("SINE", "SAW"), c=1, lvl=100)
    op(v, 2, c=1, lvl=x(40, 60))
    op(v, 3, c=x.c(1, 2), lvl=x(25, 45))
    op(v, 4, c=1, lvl=x(30, 50))
    return mat(v, ("FENV", "INDEX", x(15, 30)))


def bass_sub(x):
    """sub de seno com um fio de FM 2:1 só no ataque."""
    v = P(algo=4, keymode=1, porta=x(0, 40), aa=0.5, ar=x(80, 220), avel=30)
    op(v, 1, c=x.c(0, 1), lvl=100, d=x(2000, 6000), s=x(70, 95))
    op(v, 2, c=2, lvl=x(30, 50), d=x(60, 160), s=0)
    op(v, 3, c=x.c(0, 1), lvl=x(0, 30))
    return mat(v, ("FENV", "PITCH", x(2, 5)))


def bass_square(x):
    """baixo quadrado de videogame: ondas SQUARE e PULSE SINE, um pouco de dobra."""
    v = P(algo=x.c(4, 6), keymode=x.c(1, 2), porta=x(0, 50), aa=0.5, ar=x(50, 120), fold=x(0, 25),
          sattype=SAT["BITS"], satamt=x(30, 60))
    op(v, 1, w="SQUARE", c=x.c(0, 1), lvl=100, d=x(800, 2000), s=x(50, 85))
    op(v, 2, w=x.c("PULSE", "SINE"), c=x.c(1, 2), lvl=x(25, 45), d=x(100, 300), s=x(0, 20))
    op(v, 3, w="SINE", c=0, lvl=x(40, 70))
    return v


def bass_ring(x):
    """baixo com ring: os portadores vezes a onda do OP 2 (que nem modula) — grave sujo e metálico."""
    v = P(algo=x.c(6, 7), ring=x(35, 70), keymode=1, aa=0.5, ar=x(70, 160), cutoff=x(900, 2500), f1mode=F1["LP24"],
          reso1=x(20, 45), env1=x(30, 55), fd=x(120, 300), fs=x(10, 30), key1=50)
    op(v, 1, c=x.c(0, 1), lvl=100)
    op(v, 2, c=x.c(2, 3, 1), f=x.c(0, 0, 41, 73), lvl=x(0, 20))
    return mat(v, ("LFO1", "RING", x(10, 25)))


# ---------------- LEAD ----------------
def lead_sweep(x):
    """a razão do OP 2 varre no ataque (FILT ENV → OP2 RATIO): soa como um sync, mas de FM."""
    v = P(algo=0, keymode=x.c(1, 2), porta=x(20, 80), feedback=x(20, 50), fa=1, fd=x(200, 600), fs=0, aa=1, ar=x(150, 350),
          l1rate=x(4.5, 6), l1pitch=x(6, 10), dlysend=x(10, 25), dlytime=x.c(3, 4), rvbsend=x(5, 15))
    op(v, 1, c=1, lvl=100)
    op(v, 2, c=x.c(1, 2), lvl=x(55, 75))
    op(v, 3, c=x.c(2, 3), lvl=x(20, 40))
    return mat(v, ("FENV", "RATIO2", x(25, 45)), ("AFTER", "INDEX", 25))


def lead_brass(x):
    """metal de sopro FM: o modulador 1:1 abre mais devagar que o portador (o 'blém' do metal)."""
    v = P(algo=x.c(0, 1), keymode=x.c(0, 2), porta=x(0, 40), aa=x(15, 40), ar=x(180, 400), feedback=x(10, 35),
          l1rate=x(4.5, 6), l1pitch=x(4, 8), chomix=x(0, 20), rvbsend=x(8, 18))
    op(v, 1, c=1, lvl=100, a=x(10, 40))
    op(v, 2, c=1, lvl=x(55, 72), a=x(60, 160), d=x(400, 900), s=x(55, 80), vel=x(30, 60))
    op(v, 3, c=1, lvl=x(25, 45), a=x(80, 200))
    op(v, 4, c=x.c(1, 2), lvl=x(10, 30))
    return mat(v, ("VEL", "INDEX", x(10, 20)))


def lead_saw(x):
    """serra de realimentação: a corrente 4>3>2>1 com o OP 4 se realimentando vira uma serra cheia."""
    v = P(algo=0, feedback=x(55, 85), keymode=x.c(1, 2), porta=x(20, 70), unison=x.c(2, 2, 3), unidetune=x(15, 35),
          aa=1, ar=x(150, 300), cutoff=x(3000, 8000), f1mode=F1["LP24"], reso1=x(10, 30), key1=50,
          dlysend=x(10, 25), rvbsend=x(5, 15))
    op(v, 1, c=1, lvl=100)
    op(v, 2, c=1, lvl=x(50, 65))
    op(v, 3, c=1, lvl=x(45, 60))
    op(v, 4, c=1, lvl=x(55, 75))
    return v


def lead_psy(x):
    """lead de psytrance: razão 1:3,5 que treme com o LFO 1 (RATIO), eco pontuado."""
    v = P(algo=x.c(0, 2), keymode=2, porta=x(30, 80), feedback=x(10, 40), l1rate=x(5, 8), l1ratio=x(8, 18),
          aa=1, ar=x(150, 300), dlysend=x(25, 40), dlytime=x.c(3, 5), dlyfb=x(45, 65), phamix=x(0, 30))
    op(v, 1, c=1, lvl=100)
    op(v, 2, c=3, f=50, lvl=x(40, 60))
    op(v, 3, c=x.c(1, 7), f=x.c(0, 41), lvl=x(20, 40))
    return mat(v, ("LFO2", "PAN", x(20, 40)))


def lead_whistle(x):
    """apito: seno quase puro com vibrato e um fio de 2:1."""
    v = P(algo=4, keymode=2, porta=x(60, 160), aa=x(10, 40), ar=x(200, 450), l1rate=x(5, 6.5), l1pitch=x(8, 13),
          noisevol=x(2, 6), rvbsend=x(15, 30), dlysend=x(10, 22))
    op(v, 1, c=x.c(1, 2), lvl=100)
    op(v, 2, c=x.c(2, 3), lvl=x(10, 25))
    return mat(v, ("WHEEL", "INDEX", 40))


def lead_metal(x):
    """lead de metal: razões quebradas no garfo 4>3>1 + 2>1, dobra e um tanto de distorção."""
    v = P(algo=3, keymode=x.c(1, 2), porta=x(10, 50), feedback=x(10, 30), fold=x(5, 20), aa=1, ar=x(120, 300),
          distamt=x(0, 15), disttype=x.c(0, 1), dlysend=x(10, 25), cutoff=x(4000, 9000), f1mode=F1["LP12"], key1=40)
    op(v, 1, c=1, lvl=100)
    op(v, 2, c=x.c(1, 2), f=quebrada(x), lvl=x(25, 40))
    op(v, 3, c=x.c(2, 3), f=quebrada(x), lvl=x(20, 35))
    op(v, 4, c=x.c(1, 2), lvl=x(20, 35))
    return mat(v, ("AFTER", "FOLD", 35))


def lead_vocal(x):
    """lead que fala: FM brilhante no filtro VOWEL, com o LFO andando nas vogais."""
    v = P(algo=0, keymode=x.c(1, 2), porta=x(30, 90), feedback=x(30, 60), f1mode=F1["VOWEL"], cutoff=x(400, 1200),
          reso1=x(40, 70), l1rate=x(0.3, 1.2), l1cut=x(25, 45), l1shape=x.c(0, 1, 5), aa=x(5, 30), ar=x(200, 400),
          rvbsend=x(10, 20), chomix=x(0, 25))
    op(v, 1, c=1, lvl=100)
    op(v, 2, c=1, lvl=x(55, 70))
    op(v, 3, c=x.c(1, 2), lvl=x(40, 55))
    op(v, 4, c=1, lvl=x(30, 50))
    return v


def lead_ring(x):
    """lead com ring e razão inarmônica: robô cantando."""
    v = P(algo=x.c(4, 6), ring=x(30, 60), keymode=2, porta=x(40, 100), aa=x(5, 20), ar=x(150, 350),
          l1rate=x(4, 6), l1pitch=x(5, 9), dlysend=x(15, 30), dlytime=x.c(2, 3, 4))
    op(v, 1, c=1, lvl=100)
    op(v, 2, c=x.c(2, 3, 5), f=quebrada(x), lvl=x(25, 45))
    op(v, 3, c=x.c(1, 2), lvl=x(0, 35))
    op(v, 4, c=x.c(1, 3), lvl=x(20, 40))
    return mat(v, ("LFO3", "RING", 20))


# ---------------- PAD ----------------
def pad_glass(x):
    """vidro: duas pilhas, uma 1:1 macia e outra com razão alta (o brilho de vidro), ataque lento."""
    v = P(algo=4, aa=x(300, 900), ar=x(1200, 2500), chomix=x(20, 40), rvbsend=x(30, 50), rvbsize=x(65, 85))
    op(v, 1, c=1, lvl=100, a=x(200, 600))
    op(v, 2, c=1, lvl=x(35, 50), a=x(400, 900))
    op(v, 3, c=x.c(2, 4), lvl=x(45, 65), a=x(600, 1500), d=x(1500, 4000), s=x(30, 60))
    op(v, 4, c=x.c(13, 14, 11), f=x(0, 6), lvl=x(25, 40), a=x(500, 1200), d=x(2000, 5000), s=x(20, 50))
    return v


def pad_organ_space(x):
    """órgão espacial: os 4 soltos em parciais (0,5 · 1 · 2 · 3) levemente desafinados, chorus e reverb."""
    v = P(algo=7, aa=x(200, 700), ar=x(1000, 2200), chomix=x(30, 55), choint=x(40, 70), rvbsend=x(30, 50),
          l1rate=x(0.2, 0.6), l1amp=x(0, 15))
    op(v, 1, c=1, lvl=100)
    op(v, 2, c=0, det=x(-8, -3), lvl=x(55, 75))
    op(v, 3, c=2, det=x(3, 8), lvl=x(40, 60))
    op(v, 4, c=x.c(3, 4), det=x(-6, 6), lvl=x(25, 45))
    return v


def pad_evolve(x):
    """pad que respira: LFO 3 lento no índice, LFO 1 lento na razão — muda o tempo todo."""
    v = P(algo=x.c(0, 1), feedback=x(10, 35), aa=x(500, 1500), ar=x(1500, 3000), l3rate=x(0.05, 0.2), l3dst=DST["INDEX"],
          l3amt=x(25, 45), l1rate=x(0.08, 0.3), l1ratio=x(4, 10), rvbsend=x(35, 55), rvbsize=x(70, 90), chomix=x(10, 30))
    op(v, 1, c=1, lvl=100, a=x(400, 900))
    op(v, 2, c=x.c(1, 2), lvl=x(30, 45))
    op(v, 3, c=x.c(1, 3), f=x.c(0, 0, 41), lvl=x(25, 40))
    op(v, 4, c=x.c(2, 5), lvl=x(20, 35))
    return v


def pad_choir(x):
    """coro de metal: FM 1:1 no VOWEL, vogais andando devagar."""
    v = P(algo=1, feedback=x(15, 40), f1mode=F1["VOWEL"], cutoff=x(500, 1500), reso1=x(35, 60), l2rate=x(0.1, 0.3),
          l2cut=x(20, 40), aa=x(400, 1000), ar=x(1500, 2500), unison=2, unidetune=x(15, 30), rvbsend=x(35, 55),
          chomix=x(15, 35))
    op(v, 1, c=1, lvl=100)
    op(v, 2, c=1, lvl=x(45, 60))
    op(v, 3, c=x.c(1, 2), lvl=x(30, 45))
    op(v, 4, c=x.c(1, 3), lvl=x(20, 40))
    return v


def pad_plastic(x):
    """plástico: meias-senoides e senoides em módulo, dobra que vai e volta."""
    v = P(algo=x.c(4, 5), fold=x(10, 30), l3dst=DST["FOLD"], l3amt=x(20, 40), l3rate=x(0.1, 0.4), aa=x(300, 900),
          ar=x(1200, 2400), phamix=x(20, 45), rvbsend=x(25, 45))
    op(v, 1, w=x.c("HALF", "ABS"), c=1, lvl=100)
    op(v, 2, w=x.c("CAMEL", "ALT"), c=x.c(1, 2), lvl=x(25, 40))
    op(v, 3, w=x.c("HALF", "SINE"), c=x.c(2, 1), det=x(4, 10), lvl=x(55, 75))
    op(v, 4, w="SINE", c=x.c(1, 3), lvl=x(20, 40))
    return v


def pad_fan(x):
    """leque: um modulador para três portadores desafinados — largo e trêmulo."""
    v = P(algo=5, feedback=x(10, 35), unison=x.c(1, 2), unidetune=x(10, 25), aa=x(300, 1000), ar=x(1200, 2500),
          chomix=x(25, 45), rvbsend=x(30, 50), l2rate=x(0.1, 0.3), l2pan=x(15, 35))
    op(v, 1, c=1, det=x(-10, -4), lvl=100)
    op(v, 2, c=1, det=x(4, 10), lvl=x(80, 95))
    op(v, 3, c=2, det=x(-3, 3), lvl=x(40, 60))
    op(v, 4, c=x.c(1, 2), lvl=x(20, 35))
    return v


def pad_dark(x):
    """escuro: modulador 0,5 (sub-harmônicos), filtro baixo que abre com a roda."""
    v = P(algo=x.c(0, 2), feedback=x(10, 30), cutoff=x(500, 1200), f1mode=F1["LP24"], reso1=x(10, 30), aa=x(500, 1500),
          ar=x(1500, 3000), rvbsend=x(30, 50), rvbdamp=x(60, 80))
    op(v, 1, c=1, lvl=100)
    op(v, 2, c=0, lvl=x(40, 60))
    op(v, 3, c=x.c(0, 1), f=x.c(0, 50), lvl=x(25, 40))
    return mat(v, ("WHEEL", "CUTOFF", 45), ("LFO2", "CUTOFF", x(5, 12)))


def pad_swirl(x):
    """redemoinho: phaser, pan andando e a razão do OP 3 escorregando com o LFO 3."""
    v = P(algo=x.c(4, 0), aa=x(400, 1200), ar=x(1500, 2800), phamix=x(40, 70), phaint=x(40, 70), l2rate=x(0.1, 0.4),
          l2pan=x(25, 50), l3rate=x(0.05, 0.2), l3dst=DST["RATIO3"], l3amt=x(6, 14), rvbsend=x(30, 50), dlysend=x(10, 25),
          dlytime=5)
    op(v, 1, c=1, lvl=100)
    op(v, 2, c=x.c(1, 2), lvl=x(30, 45))
    op(v, 3, c=x.c(1, 2), lvl=x(50, 70))
    op(v, 4, c=x.c(3, 4), f=x.c(0, 41), lvl=x(25, 40))
    return v


# ---------------- PLUCK ----------------
def pluck_dx(x):
    """pluck FM: o índice despenca em 100 ms — o 'plin' clássico."""
    v = P(algo=x.c(0, 4), feedback=x(10, 40), aa=0.5, ad=x(400, 900), as_=0, ar=x(200, 400), avel=60,
          dlysend=x(15, 30), dlytime=x.c(2, 3), rvbsend=x(10, 25))
    op(v, 1, c=1, lvl=100, d=x(500, 1200), s=0)
    op(v, 2, c=x.c(1, 2, 3), lvl=x(55, 75), vel=x(30, 60), d=x(80, 220), s=0)
    op(v, 3, c=1, lvl=x(30, 60), d=x(300, 700), s=0)
    op(v, 4, c=x.c(1, 4), lvl=x(20, 45), d=x(60, 150), s=0)
    return v


def pluck_marimba(x):
    """marimba: 1:4 com o modulador bem curto, corpo de madeira."""
    v = P(algo=4, aa=0.5, ad=x(500, 900), as_=0, ar=x(250, 500), avel=50, rvbsend=x(15, 30))
    op(v, 1, c=1, lvl=100, d=x(500, 900), s=0)
    op(v, 2, c=4, lvl=x(40, 60), d=x(30, 80), s=0)
    op(v, 3, c=x.c(3, 4), lvl=x(15, 30), d=x(80, 200), s=0)
    op(v, 4, c=x.c(7, 10), lvl=x(20, 40), d=x(20, 60), s=0)
    return v


def pluck_kalimba(x):
    """kalimba de lata: razões quebradas curtas por cima de um seno."""
    v = P(algo=x.c(2, 3), aa=0.5, ad=x(600, 1200), as_=0, ar=x(300, 600), avel=50, rvbsend=x(15, 30),
          dlysend=x(10, 20), dlytime=2)
    op(v, 1, c=1, lvl=100, d=x(700, 1400), s=0)
    op(v, 2, c=x.c(5, 7), f=quebrada(x), lvl=x(25, 40), d=x(40, 120), s=0)
    op(v, 3, c=x.c(2, 3), f=x.c(0, 50), lvl=x(20, 35), d=x(100, 250), s=0)
    op(v, 4, c=x.c(1, 2), lvl=x(15, 30), d=x(60, 150), s=0)
    return v


def pluck_glass(x):
    """caco de vidro: 1:3,5 com muito eco."""
    v = P(algo=0, aa=0.5, ad=x(300, 700), as_=0, ar=x(300, 600), dlysend=x(25, 40), dlytime=x.c(1, 3, 5), dlyfb=x(45, 65),
          rvbsend=x(20, 35))
    op(v, 1, c=1, lvl=100, d=x(300, 700), s=0)
    op(v, 2, c=3, f=50, lvl=x(40, 55), d=x(60, 160), s=0)
    op(v, 3, c=x.c(1, 7), lvl=x(20, 35), d=x(50, 120), s=0)
    return v


def pluck_rubber(x):
    """plim de borracha: a altura cai no ataque, meia-senoide."""
    v = P(algo=4, aa=0.5, ad=x(200, 500), as_=0, ar=x(120, 250), fa=0.5, fd=x(30, 80), fs=0, dlysend=x(15, 30),
          dlytime=x.c(2, 3))
    op(v, 1, w=x.c("HALF", "ABS"), c=1, lvl=100, d=x(250, 500), s=0)
    op(v, 2, c=x.c(1, 2), lvl=x(40, 60), d=x(60, 150), s=0)
    return mat(v, ("FENV", "PITCH", x(10, 22)))


def pluck_koto(x):
    """corda dedilhada: realimentação forte no ataque, decai para quase seno."""
    v = P(algo=x.c(0, 1), feedback=x(50, 80), aa=0.5, ad=x(800, 1600), as_=0, ar=x(300, 600), avel=50, rvbsend=x(15, 30),
          fa=0.5, fd=x(150, 400), fs=0)
    op(v, 1, c=1, lvl=100, d=x(800, 1600), s=0)
    op(v, 2, c=x.c(1, 3), lvl=x(40, 55), d=x(150, 350), s=0)
    op(v, 3, c=1, lvl=x(30, 45), d=x(100, 250), s=0)
    op(v, 4, c=x.c(1, 2), lvl=x(40, 60), d=x(60, 160), s=0)
    return mat(v, ("FENV", "FEEDBACK", 25))


def pluck_comb(x):
    """plim metálico no COMB: o filtro pente afinado na nota deixa tudo ressoando como mola."""
    v = P(algo=x.c(0, 4), f1mode=F1["COMB"], cutoff=261.6, key1=100, reso1=x(70, 88), aa=0.5, ad=x(500, 1000), as_=0,
          ar=x(250, 500), noisevol=x(10, 25), rvbsend=x(10, 25))
    op(v, 1, c=1, lvl=100, d=x(150, 400), s=0)
    op(v, 2, c=x.c(2, 3), f=x.c(0, 41), lvl=x(40, 60), d=x(40, 100), s=0)
    return mat(v, ("AENV", "NOISE", 30))


def pluck_chip(x):
    """plim de videogame: SQUARE e PULSE SINE, eco curto."""
    v = P(algo=x.c(4, 7), aa=0.5, ad=x(150, 350), as_=0, ar=x(80, 200), dlysend=x(15, 30), dlytime=x.c(1, 2),
          sattype=SAT["BITS"], satamt=x(20, 45))
    op(v, 1, w="SQUARE", c=1, lvl=100, d=x(150, 400), s=0)
    op(v, 2, w="PULSE", c=x.c(1, 2), lvl=x(20, 40), d=x(50, 120), s=0)
    op(v, 3, w=x.c("SQUARE", "SAW"), c=2, lvl=x(30, 55), d=x(80, 200), s=0)
    return v


# ---------------- BELLS ----------------
def bell_tubular(x):
    """sino tubular: 1:3,5, cauda longa."""
    v = P(algo=4, aa=0.5, ad=x(3000, 6000), as_=0, ar=x(2000, 4000), avel=40, rvbsend=x(25, 40), rvbsize=x(60, 80))
    op(v, 1, c=1, lvl=100, d=x(3000, 6000), s=0, r=2500)
    op(v, 2, c=3, f=50, lvl=x(40, 55), d=x(1500, 3000), s=0, r=2000)
    op(v, 3, c=x.c(2, 1), lvl=x(30, 50), d=x(1500, 3000), s=0, r=2000)
    op(v, 4, c=x.c(5, 7), f=x.c(0, 41), lvl=x(20, 35), d=x(500, 1200), s=0)
    return v


def bell_glock(x):
    """glockenspiel: modulador alto e curto, portador limpo."""
    v = P(algo=x.c(4, 0), aa=0.5, ad=x(1500, 3000), as_=0, ar=x(800, 1600), avel=50, rvbsend=x(20, 35))
    op(v, 1, c=x.c(2, 1), lvl=100, d=x(1500, 3000), s=0, r=1200)
    op(v, 2, c=x.c(7, 9), f=x(0, 4), lvl=x(30, 45), d=x(100, 300), s=0)
    op(v, 3, c=x.c(4, 6), lvl=x(25, 40), d=x(400, 900), s=0)
    op(v, 4, c=x.c(11, 13), lvl=x(20, 35), d=x(60, 150), s=0)
    return v


def bell_gamelan(x):
    """gamelão: razões inarmônicas de verdade (2,76 e 5,40), batimento entre dois portadores."""
    v = P(algo=4, aa=0.5, ad=x(2000, 4000), as_=0, ar=x(1500, 3000), rvbsend=x(20, 35), chomix=x(0, 20))
    op(v, 1, c=1, lvl=100, d=x(2000, 4000), s=0, r=2000)
    op(v, 2, c=2, f=x(36, 40), lvl=x(35, 50), d=x(800, 1800), s=0)
    op(v, 3, c=1, det=x(4, 9), lvl=x(70, 90), d=x(1500, 3000), s=0, r=2000)
    op(v, 4, c=5, f=x(6, 10), lvl=x(25, 40), d=x(400, 1000), s=0)
    return v


def bell_crystal(x):
    """cristal: tudo alto e fino, eco largo."""
    v = P(algo=x.c(4, 5), aa=0.5, ad=x(1500, 3500), as_=0, ar=x(1500, 3000), dlysend=x(20, 35), dlytime=x.c(3, 5),
          rvbsend=x(30, 50), rvbsize=x(70, 90))
    op(v, 1, c=x.c(2, 4), lvl=100, d=x(1500, 3500), s=0, r=2000)
    op(v, 2, c=x.c(5, 7), f=x(0, 3), lvl=x(25, 40), d=x(500, 1200), s=0)
    op(v, 3, c=x.c(3, 6), lvl=x(55, 75), d=x(1000, 2500), s=0, r=1500)
    op(v, 4, c=x.c(9, 14), f=quebrada(x), lvl=x(20, 35), d=x(200, 600), s=0)
    return v


def bell_church(x):
    """sino de igreja: 0,5 de corpo, parcial quebrada no meio, cauda enorme."""
    v = P(algo=x.c(2, 3), feedback=x(5, 20), aa=0.5, ad=x(5000, 9000), as_=0, ar=x(3000, 6000), rvbsend=x(30, 45),
          rvbsize=x(80, 95), rvbdamp=x(50, 70))
    op(v, 1, c=0, lvl=100, d=x(5000, 9000), s=0, r=4000)
    op(v, 2, c=x.c(1, 2), f=x(18, 22), lvl=x(35, 50), d=x(2000, 4000), s=0, r=3000)
    op(v, 3, c=x.c(3, 4), f=x.c(0, 41), lvl=x(25, 40), d=x(1000, 2500), s=0)
    op(v, 4, c=x.c(1, 2), f=x(40, 45), lvl=x(25, 40), d=x(800, 2000), s=0)
    return v


def bell_gong(x):
    """gongo: realimentação + razões quebradas no garfo, e o ring sujando a cauda."""
    v = P(algo=3, feedback=x(30, 60), ring=x(10, 30), aa=x(1, 20), ad=x(4000, 8000), as_=0, ar=x(3000, 5000),
          rvbsend=x(25, 40), rvbsize=x(70, 90))
    op(v, 1, c=x.c(0, 1), lvl=100, d=x(4000, 8000), s=0, r=4000)
    op(v, 2, c=x.c(1, 2), f=quebrada(x), lvl=x(35, 55), d=x(2000, 5000), s=0, r=3000)
    op(v, 3, c=x.c(2, 3), f=quebrada(x), lvl=x(30, 50), d=x(1500, 4000), s=0)
    op(v, 4, c=x.c(1, 5), f=x.c(0, 41), lvl=x(30, 50), d=x(800, 2000), s=0)
    return v


def bell_temple(x):
    """sino de templo: dois portadores em batimento lento, ring leve, trêmulo."""
    v = P(algo=6, ring=x(10, 25), aa=x(1, 10), ad=x(3000, 6000), as_=0, ar=x(2000, 4000), l1rate=x(3, 6), l1amp=x(10, 25),
          rvbsend=x(25, 40))
    op(v, 1, c=1, lvl=100, d=x(3000, 6000), s=0, r=3000)
    op(v, 2, c=x.c(2, 3), f=x.c(41, 73), lvl=x(10, 30), d=x(2000, 4000), s=0)
    op(v, 3, c=1, det=x(3, 7), lvl=x(60, 80), d=x(2000, 5000), s=0, r=2500)
    op(v, 4, c=x.c(3, 4), f=50, lvl=x(30, 45), d=x(1000, 2500), s=0)
    return v


def bell_music_box(x):
    """caixinha de música: pequenininha, aguda, com o cliquezinho do pente."""
    v = P(algo=4, aa=0.5, ad=x(800, 1600), as_=0, ar=x(500, 1000), noisevol=x(5, 12), rvbsend=x(15, 30),
          eqlow=x(-6, -3), eqhigh=x(1, 4))
    op(v, 1, c=x.c(2, 4), lvl=100, d=x(800, 1600), s=0)
    op(v, 2, c=x.c(5, 6), lvl=x(25, 40), d=x(50, 150), s=0)
    op(v, 3, c=x.c(4, 8), lvl=x(30, 50), d=x(400, 900), s=0)
    op(v, 4, c=x.c(9, 12), lvl=x(15, 30), d=x(30, 80), s=0)
    return mat(v, ("AENV", "NOISE", -12))


# ---------------- KEYS ----------------
def keys_ep(x):
    """o e-piano FM: duas pilhas — uma 1:1 (corpo) e outra 1:14 bem curta (o 'tine')."""
    v = P(algo=4, aa=0.5, ad=x(1500, 3000), as_=x(0, 20), ar=x(300, 600), avel=55, chomix=x(15, 35), rvbsend=x(10, 22),
          l1rate=x(3, 5), l1amp=x(0, 15))
    op(v, 1, c=1, lvl=100, d=x(1500, 3000), s=x(10, 30))
    op(v, 2, c=1, lvl=x(35, 50), vel=x(50, 80), d=x(500, 1200), s=x(10, 30))
    op(v, 3, c=1, lvl=x(55, 75), d=x(800, 1600), s=0)
    op(v, 4, c=14, lvl=x(30, 45), vel=x(60, 90), d=x(40, 120), s=0)
    return v


def keys_ep_dark(x):
    """e-piano escuro e macio: menos tine, filtro baixo, trêmulo."""
    v = P(algo=4, aa=0.5, ad=x(2000, 4000), as_=x(10, 30), ar=x(300, 600), avel=50, cutoff=x(1500, 3500), f1mode=F1["LP12"],
          key1=50, l1rate=x(3.5, 5.5), l1amp=x(15, 35), phamix=x(0, 25), rvbsend=x(10, 20))
    op(v, 1, c=1, lvl=100, d=x(2000, 4000), s=x(10, 30))
    op(v, 2, c=1, lvl=x(25, 40), vel=x(40, 70), d=x(600, 1200), s=x(10, 25))
    op(v, 3, c=1, lvl=x(50, 65), d=x(1000, 2000), s=0)
    op(v, 4, c=x.c(7, 14), lvl=x(15, 30), vel=60, d=x(40, 100), s=0)
    return v


def keys_organ(x):
    """órgão FM: os 4 soltos nas razões das barras (0,5 · 1 · 2 · 3/4), um pouco de realimentação no 4."""
    v = P(algo=7, feedback=x(0, 25), aa=x(2, 8), ar=x(40, 100), avel=0, l1rate=x(5, 7), l1amp=x(0, 15),
          chomix=x(20, 45), choint=x(55, 85), distamt=x(0, 15))
    op(v, 1, c=1, lvl=100)
    op(v, 2, c=0, lvl=x(55, 80))
    op(v, 3, c=2, lvl=x(45, 70))
    op(v, 4, c=x.c(3, 4), lvl=x(30, 55))
    return v


def keys_clav(x):
    """clave FM: quadrada filtrada com o filtro estalando no ataque."""
    v = P(algo=x.c(0, 4), feedback=x(30, 60), aa=0.5, ad=x(300, 700), as_=x(20, 40), ar=x(60, 150), avel=60,
          cutoff=x(600, 1500), f1mode=F1["LP12"], reso1=x(30, 50), env1=x(45, 70), fa=0.5, fd=x(80, 200), fs=0, key1=60,
          phamix=x(0, 30))
    op(v, 1, w=x.c("SQUARE", "SAW"), c=1, lvl=100)
    op(v, 2, c=x.c(1, 3), lvl=x(30, 50), d=x(100, 250), s=x(10, 30))
    return v


def keys_harpsi(x):
    """cravo FM: modulador 1:1 + 4:1 curtos, sem sustain."""
    v = P(algo=1, feedback=x(20, 50), aa=0.5, ad=x(1000, 2000), as_=0, ar=x(200, 400), avel=30, rvbsend=x(10, 25),
          eqhigh=x(1, 4))
    op(v, 1, c=1, lvl=100, d=x(1000, 2000), s=0)
    op(v, 2, c=1, lvl=x(45, 60), d=x(300, 700), s=0)
    op(v, 3, c=x.c(4, 5), lvl=x(30, 45), d=x(100, 250), s=0)
    op(v, 4, c=x.c(3, 7), lvl=x(25, 40), d=x(80, 200), s=0)
    return v


def keys_vibe(x):
    """vibrafone: 1:4 macio com trêmulo de motor."""
    v = P(algo=4, aa=0.5, ad=x(2500, 4500), as_=0, ar=x(800, 1500), l1rate=x(4, 6.5), l1amp=x(30, 50), rvbsend=x(20, 35))
    op(v, 1, c=1, lvl=100, d=x(2500, 4500), s=0, r=1200)
    op(v, 2, c=4, lvl=x(25, 35), d=x(200, 500), s=0)
    op(v, 3, c=x.c(3, 4), lvl=x(15, 30), d=x(600, 1200), s=0)
    op(v, 4, c=x.c(10, 11), lvl=x(10, 20), d=x(50, 120), s=0)
    return v


def keys_toy(x):
    """piano de brinquedo: barras de metal desafinadas, PULSE SINE no tine."""
    v = P(algo=4, aa=0.5, ad=x(800, 1500), as_=0, ar=x(300, 600), avel=40, chomix=x(10, 25), rvbsend=x(10, 25))
    op(v, 1, c=1, lvl=100, d=x(800, 1500), s=0)
    op(v, 2, w=x.c("PULSE", "ABS"), c=x.c(3, 5), f=x(0, 8), lvl=x(25, 40), d=x(80, 200), s=0)
    op(v, 3, c=2, det=x(6, 14), lvl=x(40, 60), d=x(500, 1000), s=0)
    op(v, 4, c=x.c(7, 9), f=quebrada(x), lvl=x(15, 30), d=x(40, 100), s=0)
    return v


def keys_lofi(x):
    """e-piano gasto: BITS, wow do LFO na altura, chorus."""
    v = P(algo=4, aa=0.5, ad=x(1500, 3000), as_=x(10, 25), ar=x(300, 600), avel=50, sattype=SAT["BITS"], satamt=x(35, 60),
          l1rate=x(0.4, 1.0), l1pitch=x(3, 6), chomix=x(25, 45), cutoff=x(2500, 5000), f1mode=F1["LP12"], drift=x(30, 60),
          eqhigh=x(-4, -1))
    op(v, 1, c=1, lvl=100, d=x(1500, 3000), s=x(10, 30))
    op(v, 2, c=1, lvl=x(35, 50), vel=60, d=x(500, 1000), s=x(10, 25))
    op(v, 3, c=1, lvl=x(55, 70), d=x(800, 1500), s=0)
    op(v, 4, c=14, lvl=x(25, 40), vel=70, d=x(40, 100), s=0)
    return v


# ---------------- ARP (o ARP já vem ligado) ----------------
def arp_bells(x):
    v = P(algo=4, aa=0.5, ad=x(400, 900), as_=0, ar=x(300, 700), dlysend=x(20, 35), dlytime=x.c(1, 3), rvbsend=x(15, 30))
    op(v, 1, c=x.c(1, 2), lvl=100, d=x(500, 900), s=0)
    op(v, 2, c=3, f=50, lvl=x(35, 50), d=x(100, 250), s=0)
    op(v, 3, c=x.c(1, 4), lvl=x(40, 60), d=x(300, 600), s=0)
    op(v, 4, c=x.c(7, 9), lvl=x(20, 35), d=x(40, 100), s=0)
    return arp(v, x, rate=x.c("1/16", "1/16T"))


def arp_acid(x):
    v = P(algo=0, feedback=x(30, 60), aa=0.5, ad=x(200, 400), as_=0, ar=60, cutoff=x(300, 700), reso1=x(65, 85),
          env1=x(40, 70), fa=0.5, fd=x(100, 220), fs=0, key1=40, sattype=SAT["TUBE"], satamt=x(40, 60))
    op(v, 1, c=1, lvl=100)
    op(v, 2, c=1, lvl=x(40, 55), d=x(100, 200), s=x(10, 30))
    op(v, 4, c=x.c(1, 2), lvl=x(30, 45))
    v = mat(v, ("FENV", "INDEX", x(15, 30)))
    return arp(v, x, rate="1/16", octs=x.c(1, 2), gate=x(40, 65))


def arp_chip(x):
    v = P(algo=x.c(6, 7), aa=0.5, ad=x(150, 300), as_=x(0, 30), ar=x(50, 120), sattype=SAT["BITS"], satamt=x(30, 55))
    op(v, 1, w="SQUARE", c=1, lvl=100)
    op(v, 2, w="PULSE", c=2, lvl=x(30, 50))
    op(v, 3, w="SQUARE", c=x.c(1, 2), det=x(5, 12), lvl=x(30, 50))
    return arp(v, x, rate=x.c("1/16", "1/32"), octs=x.c(2, 3), gate=x(30, 60))


def arp_psy(x):
    v = P(algo=0, feedback=x(20, 45), aa=0.5, ad=x(150, 300), as_=0, ar=x(80, 160), l1rate=x(0.3, 1.0), l1ratio=x(6, 14),
          dlysend=x(20, 35), dlytime=3, dlyfb=x(40, 60), phamix=x(15, 35), l3sync=SYNC["1BAR"], l3shape=LFO["TRI"])
    op(v, 1, c=1, lvl=100, d=x(200, 400), s=0)
    op(v, 2, c=3, f=50, lvl=x(40, 55), d=x(80, 200), s=x(0, 20))
    op(v, 3, c=x.c(1, 2), lvl=x(30, 45))
    v = mat(v, ("LFO3", "INDEX", x(15, 30)))
    return arp(v, x, modo=x.c("UP", "UPDOWN"), rate="1/16", octs=x.c(1, 2))


def arp_glass(x):
    v = P(algo=x.c(4, 5), aa=0.5, ad=x(300, 600), as_=0, ar=x(300, 600), dlysend=x(25, 40), dlytime=x.c(2, 3, 5),
          rvbsend=x(25, 40))
    op(v, 1, c=x.c(2, 1), lvl=100, d=x(300, 600), s=0)
    op(v, 2, c=x.c(5, 7), f=x.c(0, 41), lvl=x(25, 40), d=x(60, 150), s=0)
    op(v, 3, c=x.c(3, 4), lvl=x(55, 75), d=x(250, 500), s=0)
    op(v, 4, c=x.c(1, 2), lvl=x(20, 35), d=x(100, 250), s=0)
    return arp(v, x, rate=x.c("1/16", "1/8T"), octs=x.c(2, 3))


def arp_random_metal(x):
    v = P(algo=3, feedback=x(10, 30), aa=0.5, ad=x(150, 350), as_=0, ar=x(100, 250), fold=x(0, 15),
          dlysend=x(15, 30), dlytime=x.c(1, 2))
    op(v, 1, c=1, lvl=100, d=x(200, 400), s=0)
    op(v, 2, c=x.c(2, 3), f=quebrada(x), lvl=x(25, 40), d=x(60, 150), s=0)
    op(v, 3, c=x.c(1, 3), f=quebrada(x), lvl=x(20, 35), d=x(100, 200), s=0)
    v = mat(v, ("RANDOM", "RATIO2", x(8, 18)), ("RANDOM", "INDEX", x(10, 25)))
    return arp(v, x, modo="RANDOM", rate=x.c("1/16", "1/32"), octs=x.c(2, 3))


def arp_chord(x):
    v = P(algo=x.c(4, 1), feedback=x(10, 40), aa=0.5, ad=x(200, 400), as_=0, ar=x(120, 250), chomix=x(15, 35),
          dlysend=x(20, 35), dlytime=x.c(3, 4), rvbsend=x(15, 30))
    op(v, 1, c=1, lvl=100, d=x(250, 450), s=0)
    op(v, 2, c=1, lvl=x(45, 60), d=x(100, 250), s=0)
    op(v, 3, c=x.c(1, 2), lvl=x(50, 70), d=x(200, 400), s=0)
    op(v, 4, c=x.c(3, 4), lvl=x(25, 40), d=x(60, 150), s=0)
    return arp(v, x, modo="CHORD", rate=x.c("1/8", "1/16", "1/8T"), octs=1, gate=x(25, 45))


def arp_rubber_octaves(x):
    v = P(algo=4, aa=0.5, ad=x(150, 300), as_=0, ar=x(60, 150), fa=0.5, fd=x(30, 70), fs=0, dlysend=x(10, 25))
    op(v, 1, w=x.c("HALF", "ABS"), c=x.c(0, 1), lvl=100, d=x(150, 300), s=0)
    op(v, 2, c=x.c(1, 2), lvl=x(40, 60), d=x(60, 150), s=0)
    v = mat(v, ("FENV", "PITCH", x(8, 16)))
    return arp(v, x, modo="UP", rate="1/16", octs=x.c(2, 3), gate=x(35, 60))


# ---------------- TEXTURA ----------------
def tex_drone(x):
    """drone que nunca é igual: LFO 3 lentíssimo no índice, LFO 2 na realimentação."""
    v = P(algo=x.c(0, 1), feedback=x(20, 45), aa=x(1500, 4000), ar=x(3000, 6000), l3rate=x(0.03, 0.1), l3dst=DST["INDEX"],
          l3amt=x(30, 50), l2rate=x(0.05, 0.15), l2fb=x(20, 40), rvbsend=x(40, 60), rvbsize=x(80, 95), dlysend=x(10, 25),
          dlytime=6, dlyfb=x(55, 75))
    op(v, 1, c=x.c(0, 1), lvl=100)
    op(v, 2, c=x.c(1, 2), f=x.c(0, 0, 41), lvl=x(30, 45))
    op(v, 3, c=x.c(1, 3), lvl=x(25, 40))
    op(v, 4, c=x.c(1, 2), f=x.c(0, 50), lvl=x(20, 35))
    return v


def tex_wind(x):
    """vento de metal: ruído e FM passando pelo COMB que anda devagar."""
    v = P(algo=x.c(4, 6), noisevol=x(35, 60), f1mode=F1["COMB"], cutoff=x(150, 400), reso1=x(70, 88), key1=x(0, 60),
          l2rate=x(0.05, 0.2), l2cut=x(20, 40), aa=x(800, 2000), ar=x(2000, 4000), rvbsend=x(40, 60), rvbsize=x(80, 95))
    op(v, 1, c=1, lvl=x(40, 60))
    op(v, 2, c=x.c(3, 5), f=quebrada(x), lvl=x(20, 35))
    return v


def tex_space(x):
    """sinos no espaço: 4 soltos com razões quebradas e pan andando, eco longo."""
    v = P(algo=7, aa=x(600, 1500), ar=x(3000, 5000), l2rate=x(0.1, 0.3), l2pan=x(40, 70), dlysend=x(25, 40), dlytime=x.c(5, 6),
          dlyfb=x(55, 75), rvbsend=x(45, 65), rvbsize=x(85, 98))
    op(v, 1, c=1, lvl=100, d=x(3000, 8000), s=x(20, 50))
    op(v, 2, c=2, f=quebrada(x), lvl=x(30, 50), a=x(1000, 3000), d=x(2000, 5000), s=x(10, 40))
    op(v, 3, c=x.c(3, 5), f=quebrada(x), lvl=x(20, 40), a=x(2000, 5000), d=x(2000, 5000), s=x(10, 40))
    op(v, 4, c=x.c(7, 9), f=quebrada(x), lvl=x(15, 30), a=x(3000, 6000), d=x(3000, 8000), s=x(10, 40))
    return v


def tex_swirl(x):
    """caleidoscópio: a razão dos moduladores andando com o LFO 1 lento, phaser e pan."""
    v = P(algo=0, feedback=x(10, 30), aa=x(800, 2000), ar=x(2500, 4500), l1rate=x(0.05, 0.2), l1ratio=x(20, 40),
          l1shape=x.c(0, 5), phamix=x(40, 70), phaint=x(50, 80), l2rate=x(0.1, 0.25), l2pan=x(30, 60), rvbsend=x(35, 55))
    op(v, 1, c=1, lvl=100)
    op(v, 2, c=x.c(1, 2), lvl=x(35, 50))
    op(v, 3, c=x.c(1, 3), lvl=x(25, 40))
    return v


def tex_alien_choir(x):
    """coro alienígena: VOWEL andando no S&G, unison, ring de leve."""
    v = P(algo=1, feedback=x(20, 45), ring=x(5, 20), f1mode=F1["VOWEL"], cutoff=x(600, 1500), reso1=x(45, 70),
          l1rate=x(0.2, 0.6), l1shape=LFO["SG"], l1cut=x(30, 50), unison=x.c(2, 3), unidetune=x(20, 40), aa=x(800, 2000),
          ar=x(2500, 4000), rvbsend=x(40, 60), chomix=x(20, 40))
    op(v, 1, c=1, lvl=100)
    op(v, 2, c=x.c(1, 2), f=x.c(0, 0, 50), lvl=x(45, 60))
    op(v, 3, c=1, lvl=x(30, 45))
    op(v, 4, c=x.c(2, 3), lvl=x(25, 40))
    return v


def tex_computer(x):
    """computador pensando: S&H na razão e no índice, rápido, com eco."""
    v = P(algo=x.c(0, 3), feedback=x(10, 40), l3rate=x(4, 10), l3shape=LFO["SH"], l3dst=DST["RATIO2"], l3amt=x(20, 45),
          l2rate=x(6, 12), l2shape=LFO["SH"], l2index=x(20, 40), aa=x(50, 300), ar=x(1500, 3000),
          dlysend=x(25, 40), dlytime=x.c(1, 2), dlyfb=x(45, 65), rvbsend=x(20, 40))
    op(v, 1, c=1, lvl=100)
    op(v, 2, c=x.c(2, 3), lvl=x(30, 50))
    op(v, 3, c=x.c(1, 5), f=x.c(0, 41), lvl=x(20, 40))
    return v


def tex_deep(x):
    """profundeza: 0,5 + 1 com o filtro fechado andando, cauda escura."""
    v = P(algo=x.c(0, 4), feedback=x(10, 30), cutoff=x(200, 600), f1mode=F1["LP24"], reso1=x(20, 45), l2rate=x(0.03, 0.1),
          l2cut=x(25, 45), aa=x(1500, 3000), ar=x(3000, 5000), rvbsend=x(40, 60), rvbdamp=x(70, 90), boostamt=x(10, 30))
    op(v, 1, c=0, lvl=100)
    op(v, 2, c=x.c(0, 1), lvl=x(40, 60))
    op(v, 3, c=0, det=x(5, 12), lvl=x(50, 70))
    op(v, 4, c=x.c(1, 2), lvl=x(25, 40))
    return v


def tex_ring_cloud(x):
    """nuvem de ring: ring forte com a razão do OP 2 escorregando, reverb grande."""
    v = P(algo=x.c(6, 7), ring=x(50, 85), aa=x(800, 2000), ar=x(2500, 4500), l3rate=x(0.05, 0.15), l3dst=DST["RATIO2"],
          l3amt=x(8, 20), rvbsend=x(45, 65), rvbsize=x(85, 98), chomix=x(20, 40))
    op(v, 1, c=1, lvl=100)
    op(v, 2, c=x.c(2, 3, 5), f=quebrada(x), lvl=x(0, 20))
    op(v, 3, c=x.c(1, 2), det=x(5, 10), lvl=x(40, 60))
    return v


# ---------------- FX ----------------
def fx_riser(x):
    v = P(algo=0, feedback=x(30, 60), noisevol=x(10, 30), aa=x(2000, 4000), ar=x(800, 1500), fa=x(3000, 6000), fd=10000,
          fs=100, rvbsend=x(30, 50), dlysend=x(20, 35), phamix=x(20, 40))
    op(v, 1, c=1, lvl=100)
    op(v, 2, c=x.c(1, 2), lvl=x(30, 50))
    op(v, 3, c=x.c(1, 3), lvl=x(30, 45))
    return mat(v, ("FENV", "PITCH", x(30, 50)), ("FENV", "INDEX", x(30, 50)))


def fx_laser(x):
    v = P(algo=0, feedback=x(40, 80), aa=0.5, ad=x(200, 500), as_=0, ar=x(100, 300), fa=0.5, fd=x(80, 250), fs=0,
          dlysend=x(20, 40), dlytime=x.c(1, 2))
    op(v, 1, c=x.c(1, 2), lvl=100)
    op(v, 2, c=x.c(1, 3), lvl=x(50, 70))
    return mat(v, ("FENV", "PITCH", x(40, 70)), ("FENV", "INDEX", 30))


def fx_drop(x):
    v = P(algo=x.c(0, 4), feedback=x(20, 50), aa=x(1, 20), ad=3000, ar=x(500, 1200), fa=0.5, fd=x(1500, 3000),
          fs=0, rvbsend=x(25, 45), boostamt=x(20, 40))
    op(v, 1, c=1, lvl=100)
    op(v, 2, c=x.c(1, 2), lvl=x(40, 60))
    return mat(v, ("FENV", "PITCH", x(30, 50)), ("FENV", "INDEX", -20))


def fx_robot(x):
    v = P(algo=x.c(3, 5), ring=x(40, 70), l3rate=x(6, 14), l3shape=LFO["SH"], l3dst=DST["PITCH"], l3amt=x(10, 25), aa=0.5,
          ar=x(200, 500), sattype=SAT["RATE"], satamt=x(30, 55))
    op(v, 1, w=x.c("SQUARE", "SINE"), c=1, lvl=100)
    op(v, 2, c=x.c(2, 3), f=quebrada(x), lvl=x(20, 40))
    op(v, 4, c=x.c(1, 5), lvl=x(25, 45))
    return v


def fx_impact(x):
    v = P(algo=3, feedback=x(60, 95), noisevol=x(20, 40), aa=0.5, ad=x(1500, 3500), as_=0, ar=x(1500, 3000), fold=x(20, 50),
          fa=0.5, fd=x(100, 300), fs=0, rvbsend=x(35, 55), rvbsize=x(80, 95), boostamt=x(30, 60), boosttune=x(50, 80))
    op(v, 1, c=0, lvl=100, d=x(1500, 3500), s=0, r=2000)
    op(v, 2, c=x.c(1, 2), f=quebrada(x), lvl=x(50, 70), d=x(200, 600), s=0)
    op(v, 3, c=x.c(2, 3), f=quebrada(x), lvl=x(40, 60), d=x(500, 1500), s=0)
    op(v, 4, c=1, lvl=x(50, 70), d=x(150, 400), s=0)
    return mat(v, ("FENV", "PITCH", x(-25, -12)), ("AENV", "NOISE", -20))


def fx_glitch(x):
    v = P(algo=x.c(0, 3), feedback=x(30, 70), sattype=SAT["RATE"], satamt=x(40, 70), l3rate=x(8, 16), l3shape=LFO["SH"],
          l3dst=x.c(DST["RATIO2"], DST["RATIO3"], DST["FEEDBACK"]), l3amt=x(25, 50), l2rate=x(4, 10), l2shape=LFO["SH"],
          l2index=x(30, 50), aa=0.5, ar=x(100, 300), dlysend=x(15, 30), dlytime=x.c(0, 1))
    op(v, 1, c=1, lvl=100)
    op(v, 2, c=x.c(2, 5), lvl=x(30, 50))
    op(v, 3, c=x.c(3, 7), f=x.c(0, 41), lvl=x(25, 45))
    return v


def fx_siren(x):
    v = P(algo=x.c(0, 4), feedback=x(10, 40), l1rate=x(0.4, 1.5), l1pitch=x(30, 50), l1shape=x.c(0, 1), aa=x(20, 80),
          ar=x(300, 700), dlysend=x(15, 30), rvbsend=x(20, 35))
    op(v, 1, c=1, lvl=100)
    op(v, 2, c=x.c(1, 2), lvl=x(30, 50))
    return mat(v, ("LFO1", "INDEX", x(10, 25)))


def fx_metal_scrape(x):
    v = P(algo=3, feedback=x(40, 70), fold=x(20, 45), noisevol=x(10, 25), f1mode=F1["COMB"], cutoff=x(200, 800),
          reso1=x(60, 85), l2rate=x(0.2, 0.6), l2cut=x(25, 45), aa=x(50, 300), ar=x(800, 1800), rvbsend=x(25, 45))
    op(v, 1, c=x.c(0, 1), lvl=100)
    op(v, 2, c=x.c(2, 3), f=quebrada(x), lvl=x(40, 60))
    op(v, 3, c=x.c(3, 5), f=quebrada(x), lvl=x(30, 50))
    op(v, 4, c=x.c(1, 2), lvl=x(40, 60))
    return mat(v, ("LFO2", "RATIO3", x(10, 20)))


# =====================================================================================================
# os 8 arquétipos de cada categoria, com os 5 nomes (o 1º nome de cada um abre a fila no menu)
# =====================================================================================================
ARQUETIPOS = {
    "BASS": [
        (["Tapa Cromado", "Elétron Solto", "Baixo de Alumínio", "Chicote Azul", "Pulso Magnético"], bass_dx),
        (["Borracha Quente", "Pneu Derretido", "Bola Pula-Pula", "Chiclete Grave", "Elástico Cósmico"], bass_rubber),
        (["Onda Líquida", "Lava Elétrica", "Ressaca de Neon", "Maré Mecânica", "Pêndulo Gordo"], bass_wobble),
        (["Bigorna Lunar", "Trilho Torto", "Cano de Cobre", "Engrenagem Funda", "Lata Grave"], bass_metal),
        (["Fluoresceína", "Ácido Prateado", "Bolha de Mercúrio", "Gosma Ácida", "Sapo Elétrico"], bass_acid),
        (["Abismo Polido", "Subterrâneo", "Pressão Baixa", "Fundo do Poço", "Grave Liso"], bass_sub),
        (["Fliperama Grave", "Cartucho Quente", "Bit Gordo", "Pixel Pesado", "Joystick"], bass_square),
        (["Anel de Saturno", "Ferro em Brasa", "Rádio Quebrado", "Satélite Grave", "Ímã"], bass_ring),
    ],
    "LEAD": [
        (["Raio Prateado", "Cometa", "Fio de Luz", "Sinal de Fumaça Elétrica", "Antena Viva"], lead_sweep),
        (["Trombeta Cromada", "Fanfarra Sintética", "Clarim de Lata", "Metal Brilhante", "Corneta de Plástico"], lead_brass),
        (["Serra de Feedback", "Lâmina Cintilante", "Navalha de Neon", "Cortador de Nuvem", "Faca Quente"], lead_saw),
        (["Terceiro Olho", "Cogumelo Dourado", "Espiral Infinita", "Lótus Elétrica", "Festival na Floresta"], lead_psy),
        (["Assobio Cósmico", "Flauta de Vidro", "Pássaro Robô", "Sopro Fino", "Teremim de Bolso"], lead_whistle),
        (["Ferrugem Cantante", "Liga Metálica", "Solda Fria", "Cromo Gritante", "Titânio"], lead_metal),
        (["Boca de Néon", "Uau Sintético", "Garganta de Lata", "Vogal Perdida", "Sereia Elétrica"], lead_vocal),
        (["Robô Cantor", "Transmissão", "Frequência Proibida", "Controle Remoto", "Telepatia"], lead_ring),
    ],
    "PAD": [
        (["Vitral", "Catedral de Gelo", "Vidro Soprado", "Aquário Celeste", "Janela para o Sol"], pad_glass),
        (["Órgão Espacial", "Estação Orbital", "Nave Mãe", "Câmara de Eco", "Cosmódromo"], pad_organ_space),
        (["Respiração Lenta", "Maré de Cores", "Pulmão de Estrela", "Aurora Boreal", "Floresta Respirando"], pad_evolve),
        (["Coro de Cromo", "Anjos de Lata", "Monges do Futuro", "Mantra Metálico", "Canto de Satélite"], pad_choir),
        (["Plástico Derretido", "Lava Lâmpada", "Brinquedo Antigo", "Bolha de Sabão", "Embalagem a Vácuo"], pad_plastic),
        (["Leque de Luz", "Prisma Aberto", "Arco-Íris Elétrico", "Leque Holográfico", "Dobradura"], pad_fan),
        (["Eclipse", "Lado Escuro", "Cripta", "Neblina Grossa", "Noite Polar"], pad_dark),
        (["Redemoinho", "Hipnose", "Mandala", "Vertigem", "Caleidoscópio"], pad_swirl),
    ],
    "PLUCK": [
        (["Gota de Mercúrio", "Pingo Elétrico", "Faísca Doce", "Plim Cromado", "Estalinho"], pluck_dx),
        (["Marimba de Neon", "Madeira Sintética", "Xilofone Fluorescente", "Tecla de Bambu", "Batuque Digital"], pluck_marimba),
        (["Kalimba de Lata", "Polegar de Prata", "Pente de Aço", "Mbira Orbital", "Caixa de Fósforo"], pluck_kalimba),
        (["Caco de Vidro", "Estilhaço", "Lasca de Gelo", "Cristal Partido", "Espelho Trincado"], pluck_glass),
        (["Plim de Borracha", "Bola de Gude", "Mola de Brinquedo", "Pulo do Gato", "Bolota"], pluck_rubber),
        (["Koto Cósmico", "Corda Bamba", "Cítara de Marte", "Harpa de Arame", "Dedilhado Torto"], pluck_koto),
        (["Mola de Aço", "Ressonância Presa", "Tubo de Ensaio", "Ressonador", "Diapasão"], pluck_comb),
        (["Moeda Extra", "Vida Extra", "Bônus", "Fase Secreta", "Ponto Final"], pluck_chip),
    ],
    "BELLS": [
        (["Sino Tubular", "Torre do Relógio", "Badalada", "Carrilhão", "Meio-Dia"], bell_tubular),
        (["Glockenspiel Lunar", "Estrelinha", "Faísca de Natal", "Pó de Fada", "Sininho"], bell_glock),
        (["Gamelão de Bali", "Bronze Batido", "Templo de Java", "Gongo Pequeno", "Metalofone"], bell_gamelan),
        (["Cristaleira", "Taça de Cristal", "Lustre", "Diamante", "Quartzo"], bell_crystal),
        (["Sino de Igreja", "Campanário", "Ave-Maria", "Vigília", "Domingo de Manhã"], bell_church),
        (["Gongo Psicodélico", "Tam-Tam", "Disco Voador", "Prato Chinês", "Planeta de Bronze"], bell_gong),
        (["Templo Flutuante", "Incenso", "Meditação", "Pagode", "Monastério"], bell_temple),
        (["Caixinha de Música", "Bailarina", "Relicário", "Ninar", "Sótão"], bell_music_box),
    ],
    "KEYS": [
        (["Piano Elétrico 84", "Rhodes de Plástico", "Balada de Cromo", "Tecla de Vidro", "Sala de Espera Dourada"], keys_ep),
        (["Veludo Elétrico", "Madrugada Macia", "Lounge Cósmico", "Fumaça Lenta", "Abajur"], keys_ep_dark),
        (["Órgão Lisérgico", "Igrejinha Psicodélica", "Barras de Cobre", "Quermesse Galáctica", "Rodinha"], keys_organ),
        (["Clave Elástica", "Funk de Lata", "Dedo Mole", "Groove Cromado", "Wah Sintético"], keys_clav),
        (["Cravo Cósmico", "Barroco Digital", "Corte do Futuro", "Minueto", "Peruca Empoada"], keys_harpsi),
        (["Vibrafone Líquido", "Motor Lento", "Jazz de Vidro", "Piscina", "Coquetel"], keys_vibe),
        (["Piano de Brinquedo", "Pianinho", "Quarto de Criança", "Desafinado", "Teclado de Feira"], keys_toy),
        (["Fita Gasta", "Disco Arranhado", "Vitrola Torta", "Lembrança", "Polaroide"], keys_lofi),
    ],
    "ARP": [
        (["Escada de Sinos", "Carrossel", "Ciranda", "Roda Gigante", "Gira-Gira"], arp_bells),
        (["Ácido em Fila", "Gosma em Série", "Fábrica Ácida", "Linha de Montagem", "Esteira"], arp_acid),
        (["Arcade", "Ficha", "Tela Inicial", "Recorde", "Continue"], arp_chip),
        (["Trance Lisérgico", "Goa", "Ritual", "Fogueira", "Nascer do Sol"], arp_psy),
        (["Chuva de Vidro", "Garoa Prateada", "Granizo", "Orvalho", "Gotejamento"], arp_glass),
        (["Metal Aleatório", "Dado Viciado", "Roleta", "Sorteio", "Caos Controlado"], arp_random_metal),
        (["Acorde Pulsante", "Batida de Coração", "Estalo de Dedos", "Pulsar", "Sinal de Vida"], arp_chord),
        (["Oitavas de Borracha", "Pula-Pula", "Elevador", "Escadaria", "Trampolim"], arp_rubber_octaves),
    ],
    "TEXTURA": [
        (["Drone Infinito", "Zumbido Sagrado", "Om Elétrico", "Transe Lento", "Mantra"], tex_drone),
        (["Vento de Metal", "Tempestade de Areia", "Deserto Cromado", "Duna", "Sopro do Túnel"], tex_wind),
        (["Poeira de Estrela", "Constelação", "Via Láctea", "Nebulosa", "Gravidade Zero"], tex_space),
        (["Lisergia", "Miragem Líquida", "Viagem", "Psicodelia", "Dimensão Paralela"], tex_swirl),
        (["Coro Alienígena", "Contato Imediato", "Abdução", "Visitantes", "Sinal de Outro Mundo"], tex_alien_choir),
        (["Computador Sonhando", "Cérebro Eletrônico", "Algoritmo", "Matriz de Pontos", "Memória RAM"], tex_computer),
        (["Fossa das Marianas", "Baleia de Aço", "Pressão do Mar", "Escuridão Abissal", "Sonar"], tex_deep),
        (["Nuvem de Ring", "Anel de Fumaça", "Halo", "Eco do Vácuo", "Interferência"], tex_ring_cloud),
    ],
    "FX": [
        (["Decolagem", "Subida ao Céu", "Lançamento", "Contagem Regressiva", "Turbina Ligando"], fx_riser),
        (["Laser", "Pistola de Raios", "Zap", "Disparo Estelar", "Feixe"], fx_laser),
        (["Queda Livre", "Pouso Forçado", "Desligando", "Apagão", "Mergulho"], fx_drop),
        (["Robô", "Androide", "Voz de Lata", "Sonda Espacial", "Ciborgue"], fx_robot),
        (["Impacto", "Meteoro", "Trovão de Metal", "Estrondo", "Cratera"], fx_impact),
        (["Defeito", "Tela Azul", "Curto-Circuito", "Pane Geral", "Vírus"], fx_glitch),
        (["Sirene Psicodélica", "Alarme", "Alerta Vermelho", "Ambulância de Marte", "Plantão"], fx_siren),
        (["Sucata Viva", "Ferro-Velho", "Chapa Arranhada", "Metal Retorcido", "Serralheria"], fx_metal_scrape),
    ],
}


def limpa(valores):
    """Limita às faixas, arredonda e tira o que é igual ao padrão."""
    out = {}
    for k, x in valores.items():
        if k not in PADRAO:
            raise KeyError(f"parâmetro desconhecido: {k}")
        lo, hi = FAIXAS.get(k, (0, 100))
        x = max(lo, min(hi, x))
        if k in INTEIROS:
            x = int(round(x))
        elif k in ("l1rate", "l2rate", "l3rate") or abs(x) < 10:
            x = round(x, 2)
        elif abs(x) >= 100:
            x = round(x)
        else:
            x = round(x, 1)
        if x != PADRAO[k]:
            out[k] = x
    return out


def monta():
    """[(categoria, nome, valores)] na ordem do banco; os arquétipos se intercalam (o menu não tem 5 parecidos em fila)."""
    lista, vistos = [], set()
    for cat in CATEGORIAS:
        arqs = ARQUETIPOS[cat]
        assert len(arqs) == 8 and all(len(n) == 5 for n, _ in arqs), cat
        for v in range(5):
            for nomes, receita in arqs:
                nome = nomes[v]
                if nome.lower() in vistos:
                    raise ValueError(f"nome repetido: {nome}")
                vistos.add(nome.lower())
                lista.append((cat, nome, receita(V(nome))))
    return lista


def fmt(x):
    if isinstance(x, int):
        return f"{x}.0f"
    s = f"{x:.2f}".rstrip("0").rstrip(".")
    return (s if "." in s else s + ".0") + "f"


def main():
    lista = monta()
    niveis = {}
    arq_niveis = AQUI / "niveis.txt"
    if arq_niveis.exists():
        for linha in arq_niveis.read_text(encoding="utf-8").splitlines():
            if ";" in linha:
                n, d = linha.rsplit(";", 1)
                niveis[n] = float(d)

    final = []
    for cat, nome, vals in lista:
        vals = dict(vals)
        vals["volume"] = vals.get("volume", PADRAO["volume"]) + niveis.get(nome, 0.0)
        final.append((cat, nome, limpa(vals)))

    if "--lista" in sys.argv:
        # todos os parâmetros, para o harness renderizar e medir (categoria;nome;id=valor,…)
        destino = Path(sys.argv[sys.argv.index("--lista") + 1])
        linhas = []
        for cat, nome, vals in final:
            cheio = dict(PADRAO)
            cheio.update(vals)
            linhas.append(f"{CATEGORIAS.index(cat)};{nome};" + ",".join(f"{k}={v}" for k, v in cheio.items()))
        destino.write_text("\n".join(linhas) + "\n", encoding="utf-8")

    linhas = [
        "// GERADO por tools/gerar-presets.py — não editar à mão (mude o script e rode de novo).",
        "// VACINA — banco FACTORY 1: 40 presets por categoria (8 arquétipos × 5), na ordem de categoryNames.",
        "// { nome, categoria, { valores REAIS } }",
    ]
    atual = None
    for cat, nome, vals in final:
        if cat != atual:
            linhas += ["", f"// ---- {cat} ----"]
            atual = cat
        corpo = ", ".join(f'{{ "{k}", {fmt(x)} }}' for k, x in vals.items())
        linhas.append(f'{{ "{nome}", {CATEGORIAS.index(cat)}, {{ {corpo} }} }},')
    destino = AQUI.parent / "Source" / "presets" / "fabrica.inc"
    destino.write_text("\n".join(linhas) + "\n", encoding="utf-8", newline="\n")
    print(f"{len(final)} presets em {destino}")


if __name__ == "__main__":
    main()

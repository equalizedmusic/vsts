#!/usr/bin/env python3
"""Gera o banco FACTORY 1 do VACINA: Source/presets/fabrica.inc (9 categorias × 40 presets).

Uso:
    python gerar-presets.py                 # escreve fabrica.inc (com o volume nivelado em niveis.txt, se existir)
    python gerar-presets.py --lista X.txt   # escreve também a lista completa (todos os parâmetros) para o harness medir

Cada categoria tem 8 ARQUÉTIPOS (sons de família diferente: reese, ácido, sub, FM…) e cada arquétipo tem 5 nomes
escritos à mão; cada nome é uma variação do arquétipo (semente fixa pelo nome — rodar de novo dá o mesmo arquivo).
O volume de cada preset é acertado pela medida do harness (niveis.txt: nome;dB a somar), para todos soarem parelhos.
Valores REAIS (iguais ao estado salvo pelo plugin); o que o preset não traz volta ao padrão do parâmetro.
"""
import random
import sys
import zlib
from pathlib import Path

AQUI = Path(__file__).resolve().parent
CATEGORIAS = ["BASS", "LEAD", "PAD", "PLUCK", "ARP", "KEYS", "SUPERSAW", "ATMOSFERA", "FX"]

# padrões e faixas (iguais ao createLayout de Params.h)
PADRAO = {
    "volume": -6, "keymode": 0, "porta": 0, "bendrange": 2, "drift": 20, "unison": 1, "unidetune": 25, "unispread": 60,
    "o1model": 0, "o1table": 0, "o1shape": 66, "o1pw": 50, "o1semi": 0, "o1spread": 40, "o1vol": 80,
    "o2model": 0, "o2table": 0, "o2shape": 66, "o2pw": 50, "o2semi": 0, "o2detune": 7, "o2sync": 0, "o2vol": 0, "fm": 0,
    "o3shape": 66, "o3semi": -12, "o3detune": 0, "o3vol": 0,
    "subvol": 0, "noisevol": 0, "noisecolor": 0, "ringvol": 0, "oscvol": 50,
    "cutoff": 2400, "cutoff2": 0, "filtbal": 0, "routing": 0, "f1mode": 0, "f2mode": 0, "reso1": 20, "reso2": 20,
    "env1": 30, "env2": 30, "key1": 30, "key2": 30, "sattype": 0, "satamt": 30,
    "fa": 2, "fd": 450, "fs": 30, "fr": 350, "fvel": 30, "fslope": 0,
    "aa": 2, "ad": 600, "as": 80, "ar": 300, "avel": 40, "aslope": 0,
    "l1rate": 5, "l1shape": 0, "l1sync": 0, "l1pitch": 0, "l1cut": 0, "l1pw": 0, "l1amp": 0,
    "l2rate": 0.4, "l2shape": 1, "l2sync": 0, "l2shp": 0, "l2fm": 0, "l2cut": 0, "l2pan": 0,
    "l3rate": 1.5, "l3shape": 0, "l3sync": 0, "l3dst": 0, "l3amt": 0,
    "m1src": 1, "m1dst": 1, "m1amt": 30, "m2src": 0, "m2dst": 0, "m2amt": 0, "m3src": 0, "m3dst": 0, "m3amt": 0,
    "m4src": 0, "m4dst": 0, "m4amt": 0, "m5src": 0, "m5dst": 0, "m5amt": 0, "m6src": 0, "m6dst": 0, "m6amt": 0,
    "arpon": 0, "arpmode": 0, "arpoct": 1, "arprate": 2, "arpgate": 55, "arphold": 0,
    "dlysend": 0, "dlytime": 3, "dlyfb": 40, "rvbsend": 0, "rvbsize": 55, "rvbdamp": 45,
    "eqlow": 0, "eqmid": 0, "eqhigh": 0, "disttype": 0, "distamt": 0, "boostamt": 0, "boosttune": 100,
    "chomix": 0, "choint": 45, "phamix": 0, "phaint": 50,
}
FAIXAS = {
    "volume": (-36, 12), "keymode": (0, 2), "porta": (0, 2000), "bendrange": (1, 24), "unison": (1, 4),
    "o1model": (0, 2), "o1table": (0, 23), "o1pw": (5, 95), "o1semi": (-24, 24), "o2model": (0, 1), "o2table": (0, 23),
    "o2pw": (5, 95), "o2semi": (-24, 24), "o2detune": (-50, 50), "o2sync": (0, 1), "o3semi": (-24, 24), "o3detune": (-50, 50),
    "noisecolor": (-100, 100), "cutoff": (20, 20000), "cutoff2": (-48, 48), "routing": (0, 1), "f1mode": (0, 6), "f2mode": (0, 5),
    "env1": (-100, 100), "env2": (-100, 100), "sattype": (0, 6),
    "fa": (0.5, 10000), "fd": (1, 10000), "fr": (1, 10000), "aa": (0.5, 10000), "ad": (1, 10000), "ar": (1, 10000),
    "fslope": (-100, 100), "aslope": (-100, 100),
    "l1rate": (0.02, 30), "l2rate": (0.02, 30), "l3rate": (0.02, 30), "l1shape": (0, 5), "l2shape": (0, 5), "l3shape": (0, 5),
    "l1sync": (0, 8), "l2sync": (0, 8), "l3sync": (0, 8), "l3dst": (0, 21),
    "l1pitch": (-100, 100), "l1cut": (-100, 100), "l1pw": (-100, 100), "l2shp": (-100, 100), "l2fm": (-100, 100),
    "l2cut": (-100, 100), "l2pan": (-100, 100), "l3amt": (-100, 100),
    "arpon": (0, 1), "arpmode": (0, 5), "arpoct": (1, 4), "arprate": (0, 5), "arpgate": (5, 100), "arphold": (0, 1),
    "dlytime": (0, 6), "dlyfb": (0, 95), "eqlow": (-12, 12), "eqmid": (-12, 12), "eqhigh": (-12, 12), "disttype": (0, 3),
    "boosttune": (40, 400),
}
for _s in range(1, 7):
    FAIXAS[f"m{_s}src"] = (0, 10); FAIXAS[f"m{_s}dst"] = (0, 21); FAIXAS[f"m{_s}amt"] = (-100, 100)
INTEIROS = {"keymode", "bendrange", "unison", "o1model", "o1table", "o1semi", "o2model", "o2table", "o2semi", "o2sync", "o3semi",
            "cutoff2", "routing", "f1mode", "f2mode", "sattype", "l1shape", "l1sync", "l2shape", "l2sync", "l3shape", "l3sync",
            "l3dst", "arpon", "arpmode", "arpoct", "arprate", "arphold", "dlytime", "disttype"} \
           | {f"m{s}{k}" for s in range(1, 7) for k in ("src", "dst")}

# listas (índices iguais a Params.h)
T = {n: i for i, n in enumerate(["CLASSIC", "PULSE", "DOUBLESAW", "FOLDED", "METAL", "FORMANT", "SYNC", "BENT", "ELECTRIC",
                                  "SQUARESAW", "SATURATED", "BRUSH", "CRYSTAL", "DIGITAL", "VOCAL", "HARMONICS", "GRANITE",
                                  "BELLS", "FMLINEAR", "FMMETAL", "SPECTRAL", "ORGAN", "STRING", "VACINA"])}
SRC = {n: i for i, n in enumerate(["OFF", "WHEEL", "AFTER", "VEL", "KEY", "LFO1", "LFO2", "LFO3", "FENV", "AENV", "RANDOM"])}
DST = {n: i for i, n in enumerate(["OFF", "CUTOFF", "CUTOFF2", "RESO", "PITCH", "PITCH2", "SHAPE1", "SHAPE2", "SHAPE3", "PW",
                                    "FM", "VOL1", "VOL2", "VOL3", "SUB", "NOISE", "RING", "SPREAD", "SAT", "FILTBAL", "PAN",
                                    "VOLUME"])}
F1 = {"LP24": 0, "LP12": 1, "HP": 2, "BP": 3, "BS": 4, "COMB": 5, "VOWEL": 6}
F2 = {"LP": 0, "HP": 1, "BP": 2, "BS": 3, "COMB": 4, "VOWEL": 5}
SAT = {"OFF": 0, "LIGHT": 1, "TUBE": 2, "HARD": 3, "FOLD": 4, "BITS": 5, "RATE": 6}
SINE, TRI, SAW, PULSE = 0, 33, 66, 100
SYNC = {"OFF": 0, "1/16": 1, "1/8T": 2, "1/8": 3, "1/4": 4, "1/2": 5, "1BAR": 6, "2BARS": 7, "4BARS": 8}


class V:
    """Ajudante de variação: faixas e escolhas com a semente do nome."""
    def __init__(self, nome):
        self.r = random.Random(zlib.crc32(nome.encode("utf-8")))

    def __call__(self, a, b):
        return a + (b - a) * self.r.random()

    def c(self, *op):
        return self.r.choice(op)


def mat(v, *slots):
    """Liga a matriz: (fonte, destino, quantidade), a partir do slot 2 (o 1 é a roda → corte)."""
    for i, (s, d, a) in enumerate(slots):
        v[f"m{i + 2}src"], v[f"m{i + 2}dst"], v[f"m{i + 2}amt"] = SRC[s], DST[d], a
    return v


def S(**kw):
    """dict com 'as' (sustain do amp) escrito como as_ (as é palavra reservada)."""
    if "as_" in kw:
        kw["as"] = kw.pop("as_")
    return kw


# =====================================================================================================
# ARQUÉTIPOS. A receita recebe o ajudante x e devolve o que muda do padrão.
# =====================================================================================================
def bass_reese(x):
    v = S(keymode=x.c(1, 2), porta=x(0, 40), o1shape=SAW, o2shape=SAW, o2vol=80, o2detune=x(-28, -14),
          subvol=x(25, 50), cutoff=x(380, 900), reso1=x(10, 30), env1=x(10, 30), fd=x(400, 900), fs=x(30, 60),
          aa=1, ad=900, as_=100, ar=x(80, 200), drift=x(40, 80), oscvol=x(55, 70),
          chomix=x(0, 20), satamt=x(20, 45), sattype=x.c(0, 1, 2), l2rate=x(0.2, 0.6), l1rate=x(0.1, 0.3))
    return mat(v, ("LFO2", "PITCH2", x(1.5, 3)), ("LFO1", "CUTOFF", x(3, 8)))


def bass_acid(x):
    return S(keymode=2, porta=x(40, 90), o1shape=x.c(SAW, PULSE), o1pw=x(35, 50), cutoff=x(180, 420), reso1=x(70, 88),
             env1=x(45, 75), fa=1, fd=x(150, 320), fs=0, fr=60, fvel=x(40, 70), aa=0.5, ad=800, as_=100, ar=60,
             sattype=x.c(2, 3, 1), satamt=x(40, 75), oscvol=60, drift=10, dlysend=x(0, 18), dlytime=3)


def bass_sub(x):
    v = S(keymode=1, o1shape=x.c(SINE, 16, TRI), subvol=x(0, 35), cutoff=x(300, 900), reso1=5, env1=x(5, 20),
          fa=0.5, fd=x(40, 90), fs=0, aa=0.5, ad=x(500, 1400), as_=x(0, 60), ar=x(80, 300), oscvol=x(60, 75), drift=5,
          sattype=SAT["LIGHT"], satamt=x(15, 40), o1vol=100)
    return mat(v, ("FENV", "PITCH", x(6, 14)))


def bass_fm(x):
    return S(keymode=1, o1shape=SINE, o1semi=x.c(0, 12), o2model=1, o2table=T["FMLINEAR"], o2shape=x(40, 85), o2vol=90,
             o1vol=x(0, 20), fm=x(25, 55), cutoff=x(1200, 3000), reso1=10, env1=x(10, 30), fd=x(150, 350), fs=20,
             aa=0.5, ad=x(400, 900), as_=x(40, 80), ar=x(60, 150), subvol=x(20, 45), oscvol=55, drift=10,
             m2src=SRC["FENV"], m2dst=DST["FM"], m2amt=x(20, 45), m3src=SRC["FENV"], m3dst=DST["SHAPE2"], m3amt=x(10, 30))


def bass_funk(x):
    return S(keymode=x.c(0, 1), o1shape=x(70, 95), o1pw=x(25, 45), o2shape=SAW, o2vol=x(30, 60), o2semi=12,
             o2detune=x(-6, 6), f1mode=F1["LP12"], cutoff=x(250, 700), reso1=x(25, 50), env1=x(50, 80), fa=0.5,
             fd=x(90, 220), fs=x(0, 15), fvel=x(50, 80), aa=0.5, ad=x(250, 600), as_=x(0, 25), ar=x(50, 120),
             avel=x(40, 70), boostamt=x(15, 40), boosttune=x(70, 110), drift=15)


def bass_growl(x):
    v = S(keymode=1, o1model=2, o1table=x.c(T["METAL"], T["BENT"], T["VACINA"], T["FOLDED"]), o1shape=x(20, 60),
          o2model=1, o2table=x.c(T["SYNC"], T["ELECTRIC"]), o2vol=x(40, 70), o2semi=x.c(0, 12), subvol=x(40, 70),
          cutoff=x(500, 1400), reso1=x(30, 55), env1=x(10, 30), fd=400, fs=60, aa=1, as_=100, ar=100,
          sattype=SAT["FOLD"], satamt=x(25, 55), oscvol=60, drift=20, l1sync=x.c(SYNC["1/8"], SYNC["1/4"], SYNC["1/8T"]),
          l1shape=x.c(0, 1, 3), l1cut=x(25, 45), distamt=x(10, 30))
    return mat(v, ("LFO1", "SHAPE1", x(25, 45)), ("LFO1", "SAT", x(10, 30)))


def bass_sync(x):
    v = S(keymode=x.c(1, 2), porta=x(0, 50), o1shape=SAW, o1vol=x(20, 50), o2shape=SAW, o2sync=1, o2semi=x.c(7, 12, 19),
          o2vol=90, cutoff=x(900, 2400), reso1=x(10, 30), env1=x(15, 35), fd=x(150, 400), fs=30, aa=0.5,
          as_=100, ar=90, subvol=x(30, 60), oscvol=x(55, 70), drift=15)
    return mat(v, ("FENV", "PITCH2", x(15, 30)), ("WHEEL", "PITCH2", 25))


def bass_vowel(x):
    return S(keymode=1, o1shape=SAW, o2shape=PULSE, o2vol=50, o2detune=x(-10, 10), f1mode=F1["VOWEL"], cutoff=x(250, 600),
             reso1=x(55, 80), env1=x(25, 50), fa=x(1, 60), fd=x(200, 600), fs=x(20, 50), aa=1, as_=100, ar=120,
             subvol=x(40, 70), oscvol=60, drift=20, l1rate=x(0.5, 3), l1cut=x(5, 15), m1amt=40)


def lead_sync(x):
    v = S(keymode=2, porta=x(50, 140), o1shape=SAW, o1vol=x(30, 60), o2shape=x(SAW, 90), o2sync=1, o2semi=x.c(12, 7, 19),
          o2vol=85, cutoff=x(2500, 6000), reso1=x(10, 30), env1=x(10, 30), fd=600, fs=60, aa=x(1, 20), as_=100,
          ar=x(150, 400), l1rate=x(5, 6.5), l1pitch=x(8, 14), dlysend=x(15, 30), dlytime=x.c(3, 5), dlyfb=x(30, 45),
          rvbsend=x(10, 20), drift=25)
    return mat(v, ("WHEEL", "PITCH2", x(20, 40)), ("FENV", "PITCH2", x(8, 20)))


def lead_super(x):
    return S(keymode=x.c(1, 2), porta=x(20, 80), o1model=1, o1shape=x(75, 100), o1spread=x(35, 60), o1vol=90,
             o2shape=SAW, o2semi=12, o2vol=x(20, 40), cutoff=x(3000, 8000), reso1=x(10, 25), env1=x(10, 25), fd=700,
             fs=70, aa=x(1, 10), as_=100, ar=x(200, 500), l1rate=5.5, l1pitch=x(5, 11),
             dlysend=x(18, 30), dlytime=x.c(3, 5), rvbsend=x(15, 28), chomix=x(0, 20), drift=20)


def lead_chip(x):
    v = S(keymode=1, porta=x(0, 25), o1shape=PULSE, o1pw=x.c(50, 25, 12.5), o2shape=PULSE, o2pw=50, o2semi=12, o2vol=x(0, 35),
          cutoff=12000, reso1=0, env1=0, fs=100, aa=0.5, ad=x(150, 400), as_=x(55, 80), ar=x(40, 120),
          l1rate=x(6, 8), l1pitch=x(0, 8), drift=0, sattype=SAT["BITS"], satamt=x(35, 60), dlysend=x(10, 25), dlytime=2,
          l3rate=x(0.5, 2))
    return mat(v, ("LFO3", "PW", x(20, 35)))


def lead_vocal(x):
    v = S(keymode=2, porta=x(40, 120), o1model=2, o1table=x.c(T["VOCAL"], T["FORMANT"]), o1shape=x(10, 60), o1vol=90,
          o2model=1, o2table=T["VOCAL"], o2shape=x(40, 90), o2detune=x(5, 12), o2vol=x(30, 60), f1mode=x.c(F1["LP12"], F1["VOWEL"]),
          cutoff=x(1200, 3200), reso1=x(20, 45), env1=x(5, 20), fs=80, aa=x(15, 60), as_=100, ar=x(300, 700),
          l1rate=x(4.5, 5.5), l1pitch=x(6, 12), rvbsend=x(20, 35), dlysend=x(10, 25), dlytime=5, drift=30, l2rate=x(0.2, 0.8))
    return mat(v, ("LFO2", "SHAPE1", x(15, 30)), ("WHEEL", "SHAPE1", 40))


def lead_bell(x):
    return S(keymode=x.c(0, 1), o1model=2, o1table=x.c(T["BELLS"], T["FMMETAL"], T["CRYSTAL"]), o1shape=x(20, 70), o1vol=90,
             o2shape=SINE, o2semi=x.c(12, 24, 19), o2vol=x(20, 45), cutoff=x(4000, 10000), reso1=5, env1=x(10, 25),
             fd=x(500, 1200), fs=40, aa=0.5, ad=x(900, 2200), as_=x(30, 60), ar=x(500, 1200), avel=60,
             m2src=SRC["FENV"], m2dst=DST["SHAPE1"], m2amt=x(15, 35), dlysend=x(20, 35), dlytime=x.c(3, 5),
             rvbsend=x(20, 35), drift=10)


def lead_scream(x):
    return S(keymode=2, porta=x(20, 60), o1shape=SAW, o2shape=x(SAW, PULSE), o2vol=70, o2detune=x(8, 18), cutoff=x(1500, 4000),
             reso1=x(35, 60), env1=x(20, 40), fd=400, fs=55, aa=1, as_=100, ar=x(150, 300), sattype=SAT["HARD"],
             satamt=x(50, 80), distamt=x(25, 50), disttype=x.c(0, 2), l1rate=5.5, l1pitch=x(6, 12), eqmid=x(2, 5),
             dlysend=x(10, 22), rvbsend=x(8, 15), drift=25, oscvol=65, m2src=SRC["AFTER"], m2dst=DST["CUTOFF"], m2amt=30)


def lead_flute(x):
    v = S(keymode=2, porta=x(30, 90), o1shape=x(SINE, 20), o1vol=90, o2shape=TRI, o2semi=12, o2vol=x(0, 20),
          noisevol=x(8, 18), noisecolor=x(-20, 30), f1mode=F1["LP12"], cutoff=x(1800, 4000), reso1=5, env1=x(10, 25),
          fa=x(30, 80), fd=400, fs=60, aa=x(25, 80), as_=100, ar=x(150, 350), l1rate=x(4.8, 5.6), l1pitch=x(4, 8),
          rvbsend=x(20, 35), drift=35)
    return mat(v, ("AFTER", "NOISE", x(10, 25)), ("LFO1", "VOLUME", x(3, 7)))


def lead_ring(x):
    return S(keymode=1, porta=x(0, 40), o1shape=x(SAW, PULSE), o1vol=x(30, 60), o2shape=x(TRI, SAW), o2semi=x.c(7, 19, 5, -5),
             o2detune=x(0, 30), o2vol=x(0, 30), ringvol=x(60, 90), cutoff=x(2500, 7000), reso1=x(15, 35), env1=x(10, 30),
             fd=500, fs=50, as_=100, ar=x(100, 300), sattype=x.c(SAT["RATE"], SAT["BITS"]), satamt=x(20, 50),
             dlysend=x(15, 30), dlytime=x.c(1, 2, 3), drift=15, m2src=SRC["LFO2"], m2dst=DST["PITCH2"], m2amt=x(2, 6))


def pad_warm(x):
    return S(o1shape=SAW, o2shape=x(SAW, 80), o2detune=x(8, 16), o2vol=80, o3shape=TRI, o3semi=-12, o3vol=x(0, 35),
             unison=x.c(1, 2), unidetune=x(15, 30), f1mode=F1["LP12"], cutoff=x(700, 1600), reso1=x(10, 25), env1=x(15, 30),
             fa=x(400, 1200), fd=x(1500, 3000), fs=x(40, 60), fr=x(1500, 3000), aa=x(300, 900), as_=100,
             ar=x(1200, 2500), chomix=x(25, 45), choint=x(30, 55), rvbsend=x(30, 45), rvbsize=x(65, 85), drift=x(40, 70))


def pad_evolve(x):
    v = S(o1model=2, o1table=x.c(T["GRANITE"], T["SPECTRAL"], T["BRUSH"], T["HARMONICS"], T["DOUBLESAW"]), o1shape=x(20, 60),
          o2model=1, o2table=x.c(T["GRANITE"], T["ELECTRIC"], T["CRYSTAL"]), o2shape=x(30, 80), o2detune=x(6, 14), o2vol=70,
          cutoff=x(1200, 3500), reso1=x(10, 30), env1=x(5, 20), fa=x(800, 2500), fs=60, fr=3000, aa=x(600, 1800),
          as_=100, ar=x(2000, 4000), aslope=x(-15, 0), fslope=x(10, 40), rvbsend=x(35, 55), rvbsize=x(75, 95),
          chomix=x(10, 25), drift=40, l2rate=x(0.05, 0.2), l3rate=x(0.1, 0.4), l3dst=DST["SHAPE2"], l3amt=x(20, 40),
          l3shape=x.c(0, 5))
    return mat(v, ("LFO2", "SHAPE1", x(25, 45)), ("LFO2", "PAN", x(15, 30)))


def pad_choir(x):
    v = S(o1model=2, o1table=T["VOCAL"], o1shape=x(0, 50), o2model=1, o2table=x.c(T["VOCAL"], T["FORMANT"]),
          o2shape=x(20, 70), o2detune=x(8, 15), o2vol=70, unison=2, unidetune=x(15, 25), unispread=90,
          f1mode=x.c(F1["VOWEL"], F1["LP12"]), cutoff=x(500, 1500), reso1=x(30, 55), env1=x(5, 15), fa=1500, fs=70,
          fr=2500, aa=x(500, 1200), as_=100, ar=x(1500, 3000), rvbsend=x(40, 60), rvbsize=x(80, 95),
          chomix=x(20, 40), drift=50, l1rate=x(4.5, 5.2), l1pitch=x(2, 5), l2rate=x(0.1, 0.3))
    return mat(v, ("LFO2", "SHAPE1", x(15, 30)), ("LFO2", "CUTOFF", x(5, 12)))


def pad_bright(x):
    return S(o1model=1, o1shape=x(80, 100), o1spread=x(45, 70), o1vol=90, o2shape=SAW, o2semi=x.c(12, 7), o2vol=x(20, 40),
             unison=x.c(1, 2), unidetune=20, cutoff=x(4000, 9000), reso1=x(5, 15), env1=x(5, 15), fa=x(200, 600), fs=80,
             aa=x(150, 500), as_=100, ar=x(1200, 2400), chomix=x(10, 25), rvbsend=x(35, 50), rvbsize=x(75, 90),
             dlysend=x(10, 20), dlytime=5, eqhigh=x(1, 4), drift=30)


def pad_strings(x):
    v = S(o1model=2, o1table=T["STRING"], o1shape=x(20, 70), o2shape=SAW, o2detune=x(8, 14), o2vol=60, o3shape=SAW,
          o3semi=12, o3detune=x(-8, -3), o3vol=x(15, 35), unison=2, unidetune=x(15, 25), f1mode=F1["LP12"],
          cutoff=x(900, 2200), reso1=10, env1=x(10, 25), fa=x(300, 900), fs=60, fr=1800, aa=x(250, 800), as_=100,
          ar=x(900, 1800), chomix=x(30, 50), choint=60, rvbsend=x(30, 45), rvbsize=80, drift=45, l1rate=5, l1pitch=x(2, 4))
    return mat(v, ("AFTER", "CUTOFF", 25))


def pad_glass(x):
    return S(o1model=2, o1table=x.c(T["CRYSTAL"], T["HARMONICS"], T["BELLS"]), o1shape=x(30, 80), o2model=1,
             o2table=T["CRYSTAL"], o2shape=x(10, 60), o2semi=12, o2detune=x(4, 9), o2vol=x(40, 60), cutoff=x(3000, 9000),
             reso1=x(5, 20), env1=x(5, 15), fs=70, aa=x(200, 700), ad=3000, as_=x(60, 90), aslope=x(-30, -10),
             ar=x(2000, 4000), rvbsend=x(45, 60), rvbsize=x(85, 98), dlysend=x(15, 30), dlytime=x.c(4, 5, 6), dlyfb=50,
             drift=25, l3rate=x(0.1, 0.3), l3dst=DST["SHAPE1"], l3amt=x(15, 30))


def pad_pwm(x):
    return S(o1shape=PULSE, o1pw=x(40, 50), o2shape=PULSE, o2pw=x(30, 45), o2detune=x(6, 12), o2vol=80, f1mode=F1["LP12"],
             cutoff=x(1200, 2800), reso1=x(10, 25), env1=x(10, 25), fa=x(300, 800), fs=60, aa=x(200, 600), as_=100,
             ar=x(900, 2000), l1rate=x(0.3, 0.9), l1pw=x(35, 55), chomix=x(30, 55), choint=x(40, 70), rvbsend=x(25, 40),
             drift=50)


def pad_sweep(x):
    return S(o1model=x.c(0, 1), o1shape=SAW, o1spread=x(30, 50), o2shape=SAW, o2detune=x(10, 18), o2vol=80,
             cutoff=x(200, 500), reso1=x(35, 60), env1=x(40, 70), fa=x(2500, 6000), fd=x(3000, 6000), fs=x(50, 80),
             fslope=x(20, 50), fr=3000, aa=x(400, 1500), as_=100, ar=x(1500, 3000), phamix=x(25, 50),
             phaint=x(30, 60), rvbsend=x(30, 50), rvbsize=85, drift=40)


def pluck_comb(x):
    return S(o1vol=x(0, 15), noisevol=x(70, 95), noisecolor=x(-30, 20), f1mode=F1["COMB"], key1=100,
             cutoff=261.6, reso1=x(86, 96), env1=0, fvel=0, aa=0.5, ad=x(700, 1800), as_=0, ar=x(400, 900),
             avel=60, f2mode=F2["LP"], filtbal=100, cutoff2=x(12, 30), reso2=5, env2=x(10, 30), key2=50,
             rvbsend=x(10, 25), dlysend=x(0, 15), drift=10)


def pluck_saw(x):
    return S(o1shape=x(SAW, 85), o2shape=SAW, o2detune=x(6, 12), o2vol=x(40, 70), o2semi=x.c(0, 12), cutoff=x(300, 700),
             reso1=x(25, 45), env1=x(55, 80), fa=0.5, fd=x(120, 260), fs=0, fr=200, fvel=50, aa=0.5, ad=x(250, 500),
             as_=0, ar=x(200, 400), dlysend=x(20, 35), dlytime=x.c(2, 3, 5), dlyfb=x(35, 55), rvbsend=x(10, 20), drift=20)


def pluck_fm(x):
    return S(o1model=2, o1table=x.c(T["FMLINEAR"], T["BELLS"], T["FMMETAL"]), o1shape=x(30, 75), o2shape=SINE,
             o2semi=x.c(12, 24, 19), o2vol=x(10, 30), cutoff=x(5000, 12000), reso1=5, env1=10, aa=0.5,
             ad=x(300, 900), as_=0, ar=x(300, 700), avel=60,
             m2src=SRC["AENV"], m2dst=DST["SHAPE1"], m2amt=x(30, 60), rvbsend=x(15, 30), dlysend=x(0, 20), drift=10)


def pluck_chip(x):
    v = S(keymode=x.c(0, 1), o1shape=PULSE, o1pw=x.c(50, 25, 12.5), cutoff=12000, reso1=0, env1=0, aa=0.5,
          ad=x(80, 220), as_=0, ar=x(60, 150), sattype=SAT["BITS"], satamt=x(30, 60), dlysend=x(20, 35),
          dlytime=x.c(1, 2), dlyfb=x(30, 50), drift=0, fa=0.5, fd=40, fs=0)
    return mat(v, ("FENV", "PITCH", x(1, 4)))


def pluck_wave(x):
    return S(o1model=2, o1table=x.c(T["METAL"], T["DIGITAL"], T["ELECTRIC"], T["SYNC"]), o1shape=x(60, 100),
             o2model=1, o2table=T["DIGITAL"], o2shape=x(40, 90), o2vol=x(0, 40), cutoff=x(900, 2500), reso1=x(20, 40),
             env1=x(40, 70), fd=x(150, 300), fs=0, aa=0.5, ad=x(250, 600), as_=0, ar=300,
             m2src=SRC["FENV"], m2dst=DST["SHAPE1"], m2amt=x(-60, -30), dlysend=x(15, 30), dlytime=3, rvbsend=15, drift=15)


def pluck_super(x):
    return S(o1model=1, o1shape=x(85, 100), o1spread=x(40, 60), o1vol=95, cutoff=x(500, 1100), reso1=x(15, 30),
             env1=x(50, 70), fd=x(180, 320), fs=0, aa=0.5, ad=x(300, 600), as_=0, ar=x(250, 450),
             dlysend=x(25, 40), dlytime=x.c(3, 5), dlyfb=x(35, 55), rvbsend=x(20, 30), rvbsize=70, drift=20)


def pluck_vowel(x):
    return S(o1shape=x.c(SAW, PULSE, 80), o1pw=x(20, 50), o2shape=x.c(PULSE, SAW), o2semi=x.c(0, 12, -12), o2vol=x(20, 70),
             f1mode=F1["VOWEL"], cutoff=x(180, 700), reso1=x(45, 85), env1=x(30, 80), fd=x(100, 450), fs=0, aa=0.5,
             ad=x(200, 700), as_=0, ar=x(150, 400), dlysend=x(5, 35), dlytime=x.c(2, 3, 5), rvbsend=x(5, 25), drift=20,
             keymode=x.c(0, 0, 2), porta=x(0, 60))


def pluck_perc(x):
    v = S(o1vol=x(20, 50), o1shape=x(SINE, TRI), noisevol=x(50, 80), noisecolor=x(-10, 60), f1mode=x.c(F1["BP"], F1["HP"], F1["LP12"]),
          cutoff=x(800, 4000), reso1=x(30, 60), env1=x(20, 50), fa=0.5, fd=x(30, 90), fs=0, aa=0.5, ad=x(60, 200), as_=0,
          ar=x(60, 200), avel=70, rvbsend=x(10, 25), drift=10)
    return mat(v, ("FENV", "PITCH", x(5, 12)))


def arp(**kw):
    return S(arpon=1, **kw)


def arp_saw(x):
    return arp(arpmode=0, arpoct=x.c(1, 2), arprate=2, arpgate=x(40, 65), o1shape=SAW, o2shape=SAW, o2detune=8, o2vol=60,
               cutoff=x(600, 1400), reso1=x(25, 40), env1=x(40, 60), fd=x(120, 250), fs=10, ad=400, as_=30, ar=150,
               dlysend=x(15, 30), dlytime=3, rvbsend=12, l1sync=SYNC["2BARS"], l1shape=1, l1cut=x(10, 25))


def arp_chip(x):
    return arp(arpmode=x.c(0, 1, 2, 3), arpoct=x.c(1, 2, 3), arprate=x.c(0, 1, 2), arpgate=x(20, 70), o1shape=x.c(PULSE, TRI, 85),
               o1pw=x.c(50, 25, 12.5), o2shape=PULSE, o2semi=x.c(12, 7, 19), o2vol=x(0, 50), cutoff=x(3000, 12000), reso1=0,
               env1=0, aa=0.5, ad=x(80, 260), as_=x(0, 50), ar=x(40, 120), sattype=x.c(SAT["BITS"], SAT["RATE"]),
               satamt=x(25, 60), dlysend=x(5, 30), dlytime=x.c(1, 2), drift=0)


def arp_bell(x):
    return arp(arpmode=x.c(0, 2, 4), arpoct=2, arprate=x.c(2, 4), arpgate=x(60, 90), o1model=2,
               o1table=x.c(T["BELLS"], T["CRYSTAL"]), o1shape=x(30, 70), cutoff=9000, reso1=5, env1=10, aa=0.5,
               ad=x(600, 1400), as_=0, ar=x(500, 900), dlysend=x(25, 40), dlytime=x.c(3, 5), dlyfb=50,
               rvbsend=x(25, 40), drift=10)


def arp_acid(x):
    return arp(arpmode=x.c(0, 3, 2), arpoct=x.c(1, 2), arprate=2, arpgate=x(45, 70), keymode=2, porta=x(20, 50),
               o1shape=x.c(SAW, PULSE), cutoff=x(250, 500), reso1=x(70, 85), env1=x(45, 65), fd=x(150, 260), fs=0,
               sattype=SAT["TUBE"], satamt=x(40, 70), dlysend=x(15, 30), dlytime=3, l1sync=SYNC["4BARS"], l1shape=0,
               l1cut=x(20, 35))


def arp_pingpong(x):
    return arp(arpmode=x.c(2, 4), arpoct=1, arprate=x.c(4, 3), arpgate=x(25, 45), o1shape=x(SAW, PULSE), o2shape=TRI,
               o2semi=12, o2vol=40, cutoff=x(900, 2000), reso1=20, env1=x(40, 60), fd=150, fs=0, ad=300, as_=0,
               ar=200, dlysend=x(40, 55), dlytime=x.c(3, 5), dlyfb=x(50, 65), rvbsend=15)


def arp_random(x):
    v = arp(arpmode=3, arpoct=x.c(2, 3), arprate=x.c(2, 1), arpgate=x(30, 60), o1model=2, o1table=x.c(T["DIGITAL"], T["SYNC"]),
            o1shape=x(30, 70), cutoff=x(800, 1600), reso1=x(40, 60), env1=x(30, 50), fd=150, fs=10,
            l1sync=SYNC["1/16"], l1shape=4, l1cut=x(20, 40), dlysend=x(20, 35), dlytime=2, drift=10)
    return mat(v, ("RANDOM", "SHAPE1", x(20, 40)))


def arp_chord(x):
    return arp(arpmode=5, arpoct=1, arprate=x.c(4, 3, 2), arpgate=x(20, 40), o1model=1, o1shape=90, o1spread=x(30, 50),
               o2shape=SAW, o2semi=12, o2vol=30, cutoff=x(700, 1500), reso1=x(20, 35), env1=x(40, 60), fd=x(120, 220), fs=0,
               ad=300, as_=0, ar=150, rvbsend=x(20, 30), dlysend=x(10, 25), dlytime=3)


def arp_wave(x):
    return arp(arpmode=x.c(0, 1, 2), arpoct=x.c(2, 3), arprate=2, arpgate=x(50, 75), o1model=2,
               o1table=x.c(T["SPECTRAL"], T["GRANITE"], T["VACINA"], T["BENT"]), o1shape=x(10, 40), cutoff=x(1500, 4000),
               reso1=x(15, 30), env1=x(20, 40), fd=200, fs=20, ad=500, as_=30, ar=250,
               l3sync=SYNC["4BARS"], l3dst=DST["SHAPE1"], l3amt=x(40, 60), l3shape=1, dlysend=x(20, 30), dlytime=3,
               rvbsend=x(15, 25), chomix=20)


def keys_ep(x):
    v = S(o1shape=x(SINE, 15), o2shape=SINE, o2semi=x.c(12, 24), fm=x(15, 35), o1vol=90, cutoff=x(3000, 6000),
          reso1=5, env1=x(10, 25), fd=900, fs=30, aa=0.5, ad=x(1500, 3000), as_=x(20, 40), ar=x(400, 700), avel=65,
          fvel=60, l1rate=x(3.5, 5), l1amp=x(10, 30), chomix=x(10, 30), rvbsend=x(15, 25), drift=20)
    return mat(v, ("VEL", "FM", x(15, 30)), ("AENV", "FM", x(10, 25)))


def keys_organ(x):
    return S(o1model=2, o1table=T["ORGAN"], o1shape=x(20, 100), o1vol=90, cutoff=x(5000, 12000), reso1=0, env1=0,
             aa=x(3, 10), as_=100, ar=x(40, 120), avel=0, l1rate=x(5, 7), l1amp=x(0, 15), chomix=x(30, 60),
             choint=x(50, 80), distamt=x(0, 20), rvbsend=x(10, 25), drift=10)


def keys_clav(x):
    return S(o1shape=PULSE, o1pw=x(15, 30), o2shape=PULSE, o2pw=x(30, 45), o2semi=12, o2vol=30, f1mode=x.c(F1["BP"], F1["HP"], F1["LP12"]),
             cutoff=x(900, 2200), reso1=x(25, 45), env1=x(30, 55), fd=x(100, 220), fs=10, aa=0.5, ad=x(300, 700),
             as_=0, ar=x(60, 150), avel=60, fvel=60, phamix=x(0, 30), drift=15)


def keys_bell(x):
    v = S(o1model=2, o1table=T["BELLS"], o1shape=x(10, 60), o2shape=SINE, o2semi=x.c(24, 19), o2vol=x(10, 25), cutoff=10000,
          reso1=0, env1=0, aa=0.5, ad=x(1500, 3500), as_=0, ar=x(1000, 2200), avel=60, rvbsend=x(25, 40), rvbsize=75, drift=5)
    return mat(v, ("AENV", "SHAPE1", x(15, 30)))


def keys_brass(x):
    v = S(keymode=x.c(0, 0, 1), o1shape=SAW, o2shape=SAW, o2detune=x(6, 12), o2vol=80, cutoff=x(400, 900), reso1=x(5, 15),
          env1=x(45, 65), fa=x(40, 120), fd=x(400, 900), fs=x(40, 60), fvel=60, aa=x(10, 40), as_=90, ar=x(150, 300),
          avel=50, rvbsend=x(15, 25), drift=40, l1rate=5.2, l1pitch=x(0, 4))
    return mat(v, ("AFTER", "CUTOFF", 30))


def keys_strings(x):
    return S(o1model=1, o1shape=x(60, 80), o1spread=x(25, 40), o1vol=80, o2shape=SAW, o2semi=12, o2detune=-6, o2vol=40,
             f1mode=F1["LP12"], cutoff=x(1500, 3000), reso1=5, env1=10, fa=200, fs=70, aa=x(80, 250), as_=100,
             ar=x(500, 900), chomix=x(40, 60), choint=60, rvbsend=x(25, 40), drift=35, l1rate=5.5, l1pitch=3)


def keys_harp(x):
    return S(o1vol=10, noisevol=85, noisecolor=x(-50, -20), f1mode=F1["COMB"], key1=100, cutoff=261.6, reso1=x(90, 97),
             env1=0, aa=0.5, ad=x(1500, 3000), as_=0, ar=x(900, 1600), avel=60, f2mode=F2["LP"], filtbal=100,
             cutoff2=x(24, 40), env2=20, key2=50, chomix=x(0, 25), rvbsend=x(20, 35), drift=10)


def keys_lofi(x):
    return S(o1shape=x(10, 25), o2shape=TRI, o2semi=12, o2vol=x(15, 30), fm=x(5, 20), cutoff=x(1800, 3500), reso1=5, env1=x(15, 30),
             fd=700, fs=30, aa=0.5, ad=x(1200, 2400), as_=x(15, 35), ar=x(300, 600), avel=60,
             sattype=x.c(SAT["RATE"], SAT["BITS"]), satamt=x(25, 45), noisevol=x(3, 8), noisecolor=-40,
             l1rate=x(0.3, 0.7), l1pitch=x(2, 4), chomix=x(15, 30), rvbsend=x(15, 30), drift=60)


def ss_anthem(x):
    return S(o1model=1, o1shape=x(85, 100), o1spread=x(55, 80), o1vol=95, o2shape=SAW, o2semi=12, o2detune=x(-10, 10), o2vol=x(15, 35),
             unison=x.c(2, 3), unidetune=x(25, 40), unispread=100, cutoff=x(5000, 10000), reso1=10, env1=10, fs=90,
             aa=x(2, 15), as_=95, ar=x(500, 900), dlysend=x(18, 28), dlytime=x.c(3, 5), rvbsend=x(25, 40), rvbsize=80,
             eqhigh=x(1, 3), drift=20)


def ss_hoover(x):
    v = S(keymode=x.c(1, 2), porta=x(80, 200), o1model=1, o1shape=x(70, 90), o1spread=x(70, 95), o2shape=PULSE, o2pw=x(20, 35),
          o2semi=-12, o2vol=60, cutoff=x(1500, 3500), reso1=x(10, 25), env1=x(15, 30), fa=x(60, 150), fd=x(200, 400), fs=60,
          aa=1, as_=100, ar=x(300, 600), chomix=x(30, 50), choint=70, l2rate=x(3, 6), drift=30, oscvol=60)
    return mat(v, ("FENV", "PITCH", x(-6, -3)), ("LFO2", "PW", x(20, 35)))


def ss_wall(x):
    return S(o1model=1, o1shape=x(70, 95), o1spread=x(60, 90), o1vol=95, unison=x.c(3, 4), unidetune=x(25, 45), unispread=100,
             f1mode=F1["LP12"], cutoff=x(2000, 5000), reso1=5, env1=x(5, 15), fa=x(400, 1200), fs=80, aa=x(400, 1200),
             as_=100, ar=x(1500, 3000), rvbsend=x(35, 55), rvbsize=90, chomix=x(10, 25), drift=30)


def ss_stab(x):
    return S(o1model=1, o1shape=x(80, 100), o1spread=x(45, 65), o1vol=95, unison=2, unidetune=25, cutoff=x(700, 1600), reso1=x(15, 30),
             env1=x(45, 65), fd=x(150, 280), fs=0, aa=0.5, ad=x(250, 450), as_=0, ar=x(200, 400),
             rvbsend=x(20, 35), dlysend=x(15, 30), dlytime=x.c(3, 5), drift=20)


def ss_sharp(x):
    v = S(o1model=1, o1shape=100, o1spread=x(40, 60), o1vol=85, o2shape=SAW, o2sync=1, o2semi=x.c(12, 19), o2vol=x(40, 60),
          cutoff=x(3500, 8000), reso1=x(15, 30), env1=x(10, 25), fs=80, as_=100, ar=x(300, 600), sattype=SAT["HARD"],
          satamt=x(30, 55), distamt=x(10, 25), dlysend=x(15, 25), rvbsend=20, drift=20)
    return mat(v, ("FENV", "PITCH2", x(10, 25)))


def ss_hyper(x):
    return S(o1model=1, o1shape=x(80, 100), o1spread=x(50, 75), o1vol=80, o2model=1, o2table=x.c(T["DOUBLESAW"], T["ELECTRIC"], T["VACINA"]),
             o2shape=x(30, 80), o2detune=x(-12, 12), o2vol=x(50, 70), o3shape=SAW, o3semi=-12, o3vol=x(20, 40), unison=2,
             unidetune=x(20, 35), cutoff=x(3000, 8000), reso1=10, env1=15, fs=80, aa=x(2, 20), as_=100, ar=x(400, 900),
             rvbsend=x(25, 40), dlysend=x(15, 25), dlytime=5, drift=25, l3rate=x(0.1, 0.3), l3dst=DST["SHAPE2"], l3amt=x(20, 40))


def ss_dark(x):
    return S(o1model=1, o1shape=x(70, 90), o1spread=x(40, 65), o1vol=95, o3shape=SAW, o3semi=-12, o3vol=x(30, 50), cutoff=x(350, 800),
             reso1=x(20, 40), env1=x(10, 25), fa=x(200, 800), fs=60, aa=x(50, 400), as_=100, ar=x(800, 1800),
             phamix=x(15, 35), rvbsend=x(30, 45), rvbsize=85, l2rate=x(0.05, 0.15), l2cut=x(10, 20), drift=50)


def ss_gate(x):
    return S(o1model=1, o1shape=x(85, 100), o1spread=x(50, 70), o1vol=95, unison=2, unidetune=30, cutoff=x(3000, 7000), reso1=10,
             env1=10, fs=80, aa=1, as_=100, ar=x(200, 400), l1sync=x.c(SYNC["1/16"], SYNC["1/8"]), l1shape=3, l1amp=x(80, 100),
             dlysend=x(15, 25), dlytime=x.c(3, 5), rvbsend=x(20, 35), drift=20)


def atm_wind(x):
    return S(o1vol=0, noisevol=95, noisecolor=x(-40, 10), f1mode=x.c(F1["BP"], F1["LP12"]), cutoff=x(400, 1500), reso1=x(50, 75),
             env1=x(10, 30), fa=x(1000, 3000), fs=60, aa=x(1500, 3500), as_=100, ar=x(3000, 6000),
             l2rate=x(0.05, 0.25), l2cut=x(30, 55), l2pan=x(40, 80), l2shape=5, rvbsend=x(40, 60), rvbsize=95,
             l3rate=x(0.1, 0.4), l3dst=DST["RESO"], l3amt=x(10, 25), l3shape=5, drift=60)


def atm_drone(x):
    return S(o1shape=SAW, o1semi=-12, o2shape=x(SAW, PULSE), o2semi=x.c(-5, 7, -12), o2detune=x(5, 15), o2vol=80, o3shape=TRI,
             o3semi=-24, o3vol=x(30, 50), cutoff=x(200, 500), reso1=x(40, 65), env1=10, fa=3000, fs=70, aa=x(2000, 4000),
             as_=100, ar=x(4000, 7000), sattype=SAT["TUBE"], satamt=x(30, 50), l2rate=x(0.03, 0.1), l2cut=x(15, 30),
             phamix=x(20, 40), phaint=20, rvbsend=x(35, 55), rvbsize=90, drift=80)


def atm_space(x):
    return S(o1model=2, o1table=x.c(T["CRYSTAL"], T["HARMONICS"], T["SPECTRAL"]), o1shape=x(20, 80), o2model=1,
             o2table=T["CRYSTAL"], o2semi=x.c(12, 19, 24), o2vol=x(40, 60), cutoff=x(4000, 10000), reso1=10, env1=0,
             aa=x(1500, 3000), as_=100, ar=x(4000, 7000), dlysend=x(30, 45), dlytime=x.c(5, 6), dlyfb=x(55, 70),
             rvbsend=x(55, 75), rvbsize=98, rvbdamp=20, l3rate=x(0.05, 0.2), l3dst=DST["SHAPE1"], l3amt=x(30, 50), l3shape=5,
             l2rate=0.1, l2pan=x(40, 70), drift=40)


def atm_comb(x):
    return S(o1vol=x(0, 20), noisevol=x(60, 90), noisecolor=x(-30, 30), f1mode=F1["COMB"], key1=100, cutoff=x(130, 520),
             reso1=x(80, 95), env1=x(-10, 10), aa=x(1000, 3000), as_=100, ar=x(3000, 6000), l2rate=x(0.05, 0.2),
             l2cut=x(3, 8), rvbsend=x(40, 60), rvbsize=90, dlysend=x(10, 25), dlytime=6, drift=40,
             f2mode=F2["LP"], filtbal=100, cutoff2=x(12, 36), reso2=10, env2=0)


def atm_ghost(x):
    return S(o1model=2, o1table=T["VOCAL"], o1shape=x(0, 100), o2model=1, o2table=T["FORMANT"], o2shape=x(0, 100), o2vol=50,
             f1mode=F1["VOWEL"], cutoff=x(400, 2000), reso1=x(40, 70), env1=0, aa=x(1500, 4000), as_=100, ar=x(3000, 6000),
             l2rate=x(0.05, 0.2), l2cut=x(20, 40), l2shp=x(30, 50), l2pan=x(30, 60), rvbsend=x(50, 70), rvbsize=95,
             dlysend=x(15, 30), dlytime=5, dlyfb=60, drift=70, l1rate=x(3, 5), l1pitch=x(3, 7))


def atm_granite(x):
    return S(o1model=2, o1table=x.c(T["GRANITE"], T["BRUSH"]), o1shape=x(0, 30), o2model=1, o2table=T["GRANITE"], o2shape=x(50, 100),
             o2detune=x(10, 25), o2vol=70, cutoff=x(800, 2500), reso1=x(20, 40), env1=x(10, 25), fa=4000, fs=50,
             fslope=x(20, 50), aa=x(2000, 4000), as_=100, aslope=x(-20, 10), ar=x(4000, 7000),
             l3rate=x(0.02, 0.08), l3dst=DST["SHAPE1"], l3amt=x(40, 70), l3shape=1, l2rate=0.05, l2shp=x(20, 40),
             rvbsend=x(40, 60), rvbsize=90, drift=60)


def atm_water(x):
    v = S(o1shape=x(SINE, TRI), o2shape=TRI, o2semi=x.c(7, 12), o2vol=60, cutoff=x(400, 900), reso1=x(50, 70), env1=x(10, 25),
          fa=1500, fs=60, aa=x(800, 2000), as_=100, ar=x(2500, 5000), l1rate=x(0.3, 1.0), l1shape=5, l1cut=x(25, 45),
          phamix=x(40, 70), phaint=x(50, 80), rvbsend=x(45, 60), rvbsize=90, rvbdamp=70, drift=50)
    return mat(v, ("RANDOM", "CUTOFF", x(10, 25)))


def atm_lab(x):
    return S(o1model=2, o1table=x.c(T["DIGITAL"], T["SYNC"], T["HARMONICS"]), o1shape=x(10, 60), o2shape=SINE, o2semi=x.c(19, 24),
             o2vol=x(20, 40), ringvol=x(20, 45), cutoff=x(1500, 5000), reso1=x(25, 45), env1=0, aa=x(800, 2500), as_=100,
             ar=x(3000, 5000), l1sync=x.c(SYNC["1/8"], SYNC["1/4"]), l1shape=4, l1cut=x(10, 25),
             l3rate=x(0.2, 0.8), l3dst=DST["PITCH2"], l3amt=x(2, 5), l3shape=4, dlysend=x(25, 40), dlytime=x.c(2, 3, 5),
             dlyfb=x(50, 65), rvbsend=x(35, 50), drift=30)


def fx_riser(x):
    v = S(o1model=1, o1shape=90, o1spread=x(50, 80), noisevol=x(30, 60), noisecolor=30, cutoff=x(150, 300), reso1=x(30, 50),
          env1=x(70, 95), fa=x(3000, 7000), fd=8000, fs=100, aa=x(1500, 4000), as_=100, ar=1500,
          phamix=x(20, 40), rvbsend=x(30, 50), dlysend=20, drift=20)
    return mat(v, ("FENV", "PITCH", x(25, 50)))


def fx_down(x):
    v = S(o1shape=SAW, o2shape=SAW, o2detune=15, o2vol=70, noisevol=x(20, 40), cutoff=x(3000, 8000), reso1=x(20, 40), env1=x(-60, -30),
          fa=0.5, fd=x(1500, 3000), fs=0, aa=0.5, ad=x(2000, 3500), as_=0, ar=1500, rvbsend=x(30, 50), dlysend=20, drift=20)
    return mat(v, ("AENV", "PITCH", x(20, 45)))


def fx_laser(x):
    v = S(keymode=1, o1shape=x(SAW, PULSE), o2shape=SINE, o2vol=x(30, 60), fm=x(20, 50), cutoff=x(3000, 9000), reso1=x(30, 60),
          env1=10, fa=0.5, fd=x(60, 200), fs=0, aa=0.5, ad=x(150, 400), as_=0, ar=x(100, 300),
          dlysend=x(20, 40), dlytime=x.c(1, 2), dlyfb=x(40, 60), drift=0)
    return mat(v, ("FENV", "PITCH", x(40, 80)))


def fx_scifi(x):
    return S(o1model=2, o1table=x.c(T["DIGITAL"], T["FMMETAL"], T["SYNC"]), o1shape=x(20, 80), o2shape=SINE, o2vol=30, fm=x(20, 50),
             cutoff=x(1500, 4000), reso1=x(40, 70), env1=0, aa=x(1, 30), as_=100, ar=x(300, 900),
             l1sync=x.c(SYNC["1/16"], SYNC["1/8T"]), l1shape=4, l1pitch=x(30, 60), l1cut=x(20, 40), l2rate=x(2, 8), l2shape=4, l2fm=x(30, 60),
             dlysend=x(25, 40), dlytime=x.c(1, 2), dlyfb=x(40, 60), drift=0)


def fx_impact(x):
    v = S(o1shape=SINE, o1semi=-24, o1vol=90, noisevol=x(60, 90), noisecolor=x(-60, -20), cutoff=x(1500, 5000), reso1=x(10, 30),
          env1=x(40, 70), fa=0.5, fd=x(300, 900), fs=0, aa=0.5, ad=x(1200, 3000), as_=0, ar=x(1000, 2500),
          sattype=SAT["TUBE"], satamt=x(40, 70), rvbsend=x(40, 60), rvbsize=90, boostamt=x(20, 45), boosttune=60, drift=0)
    return mat(v, ("AENV", "PITCH", x(15, 30)))


def fx_glitch(x):
    v = S(o1model=2, o1table=x.c(T["DIGITAL"], T["HARMONICS"], T["SPECTRAL"]), o1shape=x(0, 100), o2shape=PULSE, o2vol=40,
          o2semi=x.c(-12, 7, 19), ringvol=x(20, 50), cutoff=x(2000, 8000), reso1=x(20, 50), env1=x(-30, 30),
          as_=100, ar=x(80, 300), sattype=x.c(SAT["BITS"], SAT["RATE"]), satamt=x(50, 85),
          l1sync=x.c(SYNC["1/16"], SYNC["1/8T"]), l1shape=4, l1amp=x(40, 80),
          dlysend=x(15, 30), dlytime=x.c(0, 1), dlyfb=x(30, 50), drift=0)
    return mat(v, ("LFO1", "SHAPE1", x(40, 80)), ("LFO1", "SAT", x(20, 40)))


def fx_siren(x):
    return S(keymode=1, o1shape=x(SAW, PULSE), o2shape=SAW, o2detune=10, o2vol=50, cutoff=x(2000, 5000), reso1=x(15, 30), env1=10,
             as_=100, ar=400, l1rate=x(0.4, 2.5), l1shape=x.c(1, 0, 3), l1pitch=x(45, 70), dlysend=x(15, 25),
             rvbsend=x(20, 35), distamt=x(0, 25), drift=0)


def fx_metal(x):
    return S(o1model=2, o1table=x.c(T["METAL"], T["FMMETAL"], T["BELLS"]), o1shape=x(40, 100), o2shape=SINE, o2semi=x.c(5, 11, 13, 17),
             o2detune=x(-40, 40), o2vol=30, ringvol=x(40, 70), cutoff=x(3000, 9000), reso1=x(30, 50), env1=x(10, 30),
             fd=400, fs=20, aa=0.5, ad=x(1000, 3000), as_=0, ar=x(1000, 2500), f2mode=F2["COMB"], filtbal=x(30, 60),
             cutoff2=x(-24, 0), reso2=x(70, 90), routing=1, rvbsend=x(30, 45), drift=0)


# =====================================================================================================
# os 8 arquétipos de cada categoria, com os 5 nomes (o 1º nome de cada um abre a fila no menu)
# =====================================================================================================
ARQUETIPOS = {
    "BASS": [
        (["Reese Paulista", "Ponte Estaiada", "Concreto Armado", "Marginal Pinheiros", "Viaduto"], bass_reese),
        (["Ácido Sulfúrico", "Fermento", "Gota Corrosiva", "Bisturi", "Tubo de Ensaio"], bass_acid),
        (["Subsolo", "Caixa d'Água", "Grave de Garagem", "Tambor de Borracha", "Batimento"], bass_sub),
        (["Elástico", "Mola Solta", "Borracha Quente", "Dente de Metal", "Fio Desencapado"], bass_fm),
        (["Slap de Fábrica", "Contrabaixo de Bar", "Dedão", "Groove do Centro", "Baile de Sexta"], bass_funk),
        (["Rosnado", "Dragão de Metrô", "Mastigador", "Turbina", "Wobble do Vizinho"], bass_growl),
        (["Serra Elétrica", "Motor de Popa", "Rasgo", "Faísca Grave", "Lixa"], bass_sync),
        (["Boca de Lobo", "Vogal Grave", "Uau Grave", "Garganta", "Tuba Falante"], bass_vowel),
    ],
    "LEAD": [
        (["Sirene da Madrugada", "Agulha Quente", "Fio de Prata", "Holofote", "Linha Cruzada"], lead_sync),
        (["Febre Alta", "Torcida", "Grito de Gol", "Farol Alto", "Pico de Energia"], lead_super),
        (["Fliperama", "Cartucho", "Oito Bits", "Pixel Teimoso", "Game Over"], lead_chip),
        (["Cantora de Rádio", "Coral de Uma Só", "Voz no Túnel", "Aaah Elétrico", "Sereia"], lead_vocal),
        (["Sino de Vidro", "Campainha", "Metal Líquido", "Prata Fina", "Cristal Partido"], lead_bell),
        (["Guitarra de Mentira", "Amplificador Fritando", "Microfonia", "Grito Sujo", "Válvula Quente"], lead_scream),
        (["Flauta de Lata", "Assobio", "Sopro", "Ocarina", "Pífano"], lead_flute),
        (["Robô Cantor", "Telefone Antigo", "Rádio Pirata", "Lata de Sardinha", "Sinal Fraco"], lead_ring),
    ],
    "PAD": [
        (["Cobertor", "Fim de Tarde", "Café Coado", "Varanda", "Domingo Lento"], pad_warm),
        (["Maré Cheia", "Respiração", "Aurora", "Movimento Lento", "Vidro Soprado"], pad_evolve),
        (["Coro da Catedral", "Monges", "Vozes do Porão", "Capela", "Hino Distante"], pad_choir),
        (["Céu Aberto", "Ponte de Luz", "Clarão", "Vitral", "Alvorada"], pad_bright),
        (["Cordas da Chuva", "Violoncelo Fantasma", "Orquestra Vazia", "Sala de Ensaio", "Arco Lento"], pad_strings),
        (["Cristaleira", "Lustre", "Gelo Fino", "Caixinha de Joias", "Orvalho"], pad_glass),
        (["Veludo", "Seda Antiga", "Fita Cassete", "Anos Oitenta", "Neon Rosa"], pad_pwm),
        (["Subida de Serra", "Nascente", "Ventania", "Cortina Abrindo", "Holofotes"], pad_sweep),
    ],
    "PLUCK": [
        (["Violão de Nylon", "Harpa de Corda", "Cavaquinho", "Koto", "Berimbau"], pluck_comb),
        (["Pingo de Chuva", "Estalo de Dedo", "Gota de Néon", "Picada", "Seringa"], pluck_saw),
        (["Marimba de Vidro", "Kalimba", "Xilofone", "Gamelão", "Vibrafone"], pluck_fm),
        (["Moedinha", "Botão de Elevador", "Ping", "Bip", "Toque de Celular"], pluck_chip),
        (["Lâmina", "Moeda de Prata", "Clique Metálico", "Arame", "Alfinete"], pluck_wave),
        (["Ampola", "Dose Única", "Reforço", "Vacina de Gotinha", "Anticorpo"], pluck_super),
        (["Uó", "Bolha", "Tô", "Pato de Borracha", "Boca Fechada"], pluck_vowel),
        (["Palma", "Graveto", "Casca de Noz", "Pedrinha", "Chocalho"], pluck_perc),
    ],
    "ARP": [
        (["Escada Rolante", "Metrô Linha Azul", "Esteira", "Relógio de Ponto", "Elevador"], arp_saw),
        (["Joystick", "Contador", "Cronômetro", "Dígito", "Calculadora"], arp_chip),
        (["Carrilhão", "Caixa de Música", "Sinos da Praça", "Cata-Vento", "Móbile"], arp_bell),
        (["Formigueiro", "Ciranda Ácida", "Centrífuga", "Pipeta", "Reagente"], arp_acid),
        (["Pingue-Pongue", "Eco do Vale", "Ricochete", "Bate-Volta", "Quique"], arp_pingpong),
        (["Dado Viciado", "Loteria", "Cara ou Coroa", "Roleta", "Sorteio"], arp_random),
        (["Acorde de Pista", "Stab de Garagem", "Rave de 92", "Soco", "Batida Seca"], arp_chord),
        (["Caleidoscópio", "Engrenagem", "Hélice", "Vitrola", "Carrossel"], arp_wave),
    ],
    "KEYS": [
        (["Piano Elétrico", "Rhodes de Mentira", "Sala de Jazz", "Bossa", "Fim de Noite"], keys_ep),
        (["Órgão de Igreja", "Órgão de Quermesse", "Hammond da Tia", "Rock Progressivo", "Gospel"], keys_organ),
        (["Clavinete", "Funk de Elevador", "Wah Wah", "Pé de Pato", "Suingue"], keys_clav),
        (["Celesta", "Sino de Natal", "Glockenspiel", "Ding Dong", "Vidro Tocado"], keys_bell),
        (["Metais", "Fanfarra", "Trompete de Plástico", "Banda da Cidade", "Sopro de Bronze"], keys_brass),
        (["Cordas de Novela", "Orquestra de Bolso", "Violinos", "Sala de Concerto", "Abertura"], keys_strings),
        (["Harpa", "Saltério", "Cítara", "Dulcimer", "Bandolim"], keys_harp),
        (["Piano de Fita", "Rádio de Pilha", "Toca-Discos", "Lo-Fi de Domingo", "Piano Empoeirado"], keys_lofi),
    ],
    "SUPERSAW": [
        (["Dose Dupla", "Hino do Clube", "Mãos pro Alto", "Pista Cheia", "Clímax"], ss_anthem),
        (["Aspirador", "Hoover de 91", "Sirene de Rave", "Furacão", "Turbilhão"], ss_hoover),
        (["Muralha", "Estádio", "Catedral de Luz", "Horizonte", "Imensidão"], ss_wall),
        (["Stab de Trance", "Centelha", "Fogos", "Estouro", "Confete"], ss_stab),
        (["Serra Afiada", "Navalha", "Motosserra", "Corte Fino", "Esmeril"], ss_sharp),
        (["Exército", "Enxame", "Multidão", "Arquibancada Lotada", "Avalanche"], ss_hyper),
        (["Neblina Densa", "Sombra", "Eclipse", "Penumbra", "Noite Fechada"], ss_dark),
        (["Gate Picotado", "Trance Cortado", "Estrobo", "Pulsação", "Cardíaco"], ss_gate),
    ],
    "ATMOSFERA": [
        (["Vento Sul", "Brisa do Mar", "Tempestade", "Rajada", "Corrente de Ar"], atm_wind),
        (["Subterrâneo", "Caverna", "Usina", "Transformador", "Zumbido Antigo"], atm_drone),
        (["Estação Espacial", "Órbita", "Satélite", "Via Láctea", "Poeira Cósmica"], atm_space),
        (["Tubos de Metal", "Cano Cantante", "Garrafa Soprada", "Túnel", "Poço"], atm_comb),
        (["Fantasma", "Sussurro", "Assombração", "Eco de Voz", "Vozes no Rádio"], atm_ghost),
        (["Geleira", "Lava Lenta", "Placas Tectônicas", "Erosão", "Fóssil"], atm_granite),
        (["Submarino", "Aquário", "Fundo do Mar", "Mergulho", "Bolhas"], atm_water),
        (["Sala de Espera", "Quarentena", "Laboratório Noturno", "Isolamento", "Imunidade"], atm_lab),
    ],
    "FX": [
        (["Decolagem", "Subida", "Turbina Ligando", "Lançamento", "Contagem Regressiva"], fx_riser),
        (["Queda Livre", "Pouso", "Desliga", "Apagão", "Mergulho de Avião"], fx_down),
        (["Laser", "Raio", "Zap", "Pistola de Brinquedo", "Disparo"], fx_laser),
        (["Computador de Bordo", "Androide", "Telemetria", "Código Morse", "Sonda"], fx_scifi),
        (["Impacto", "Trovão", "Porta Batendo", "Explosão Distante", "Estrondo"], fx_impact),
        (["Defeito", "Tela Azul", "Vírus", "Bug", "Pane"], fx_glitch),
        (["Alarme", "Ambulância", "Sirene", "Alerta", "Plantão"], fx_siren),
        (["Sucata", "Ferro-Velho", "Panela", "Metal Retorcido", "Sino Rachado"], fx_metal),
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

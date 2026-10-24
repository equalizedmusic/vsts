#!/usr/bin/env python3
"""Gera o banco FACTORY 1 do VACINA: Source/presets/fabrica.inc (40 presets por categoria, 9 categorias).

Uso:
    python gerar-presets.py

Os presets feitos à mão (MANUAIS) entram primeiro na categoria; o resto sai de receitas por categoria
(faixas de parâmetro e escolhas), com semente fixa pelo nome — rodar de novo dá exatamente o mesmo arquivo.
Valores REAIS (iguais ao estado salvo pelo plugin); o que o preset não traz volta ao padrão do parâmetro.
"""
import random
import zlib
from pathlib import Path

CATEGORIAS = ["BASS", "LEAD", "PAD", "PLUCK", "ARP", "KEYS", "SUPERSAW", "ATMOSFERA", "FX"]
POR_CATEGORIA = 40

# faixas dos parâmetros (iguais ao createLayout de Params.h)
FAIXAS = {
    "volume": (-36, 6), "keymode": (0, 2), "porta": (0, 2000), "unison": (1, 4), "unidetune": (0, 100), "unispread": (0, 100),
    "o1model": (0, 1), "o1shape": (0, 100), "o1pw": (5, 95), "o1semi": (-24, 24), "o1spread": (0, 100),
    "o2shape": (0, 100), "o2pw": (5, 95), "o2semi": (-24, 24), "o2detune": (-50, 50), "o2sync": (0, 1), "fm": (0, 100),
    "oscbal": (0, 100), "subvol": (0, 100), "oscvol": (0, 100), "noisevol": (0, 100),
    "cutoff": (20, 20000), "cutoff2": (-48, 48), "filtbal": (0, 100), "routing": (0, 1), "f1mode": (0, 4), "f2mode": (0, 3),
    "reso1": (0, 100), "reso2": (0, 100), "env1": (-100, 100), "env2": (-100, 100), "key1": (0, 100), "key2": (0, 100),
    "fa": (0.5, 10000), "fd": (1, 10000), "fs": (0, 100), "fr": (1, 10000), "fvel": (0, 100),
    "aa": (0.5, 10000), "ad": (1, 10000), "as": (0, 100), "ar": (1, 10000), "avel": (0, 100),
    "l1rate": (0.02, 30), "l1shape": (0, 5), "l1sync": (0, 8), "l1pitch": (-100, 100), "l1cut": (-100, 100),
    "l1pw": (-100, 100), "l1amp": (0, 100),
    "l2rate": (0.02, 30), "l2shape": (0, 5), "l2sync": (0, 8), "l2shp": (-100, 100), "l2fm": (-100, 100),
    "l2cut": (-100, 100), "l2pan": (-100, 100),
    "m1src": (0, 9), "m1dst": (0, 14), "m1amt": (-100, 100), "m2src": (0, 9), "m2dst": (0, 14), "m2amt": (-100, 100),
    "m3src": (0, 9), "m3dst": (0, 14), "m3amt": (-100, 100),
    "arpon": (0, 1), "arpmode": (0, 5), "arpoct": (1, 4), "arprate": (0, 5), "arpgate": (5, 100), "arphold": (0, 1),
    "dlysend": (0, 100), "dlytime": (0, 6), "dlyfb": (0, 95), "rvbsend": (0, 100), "rvbsize": (0, 100), "rvbdamp": (0, 100),
    "eqlow": (-12, 12), "eqmid": (-12, 12), "eqhigh": (-12, 12), "disttype": (0, 3), "distamt": (0, 100),
    "boostamt": (0, 100), "boosttune": (40, 400), "chomix": (0, 100), "choint": (0, 100), "phamix": (0, 100), "phaint": (0, 100),
}
INTEIROS = {"keymode", "unison", "o1model", "o1semi", "o2semi", "o2sync", "cutoff2", "routing", "f1mode", "f2mode",
            "l1shape", "l1sync", "l2shape", "l2sync", "m1src", "m1dst", "m2src", "m2dst", "m3src", "m3dst",
            "arpon", "arpmode", "arpoct", "arprate", "arphold", "dlytime", "disttype"}

# índices das listas (iguais a Params.h)
SRC = {"OFF": 0, "WHEEL": 1, "AFTER": 2, "VEL": 3, "KEY": 4, "LFO1": 5, "LFO2": 6, "FENV": 7, "AENV": 8, "RANDOM": 9}
DST = {"OFF": 0, "CUTOFF": 1, "CUTOFF2": 2, "RESO": 3, "PITCH": 4, "SHAPE1": 5, "SHAPE2": 6, "PW": 7, "FM": 8, "BAL": 9,
       "SPREAD": 10, "NOISE": 11, "PAN": 12, "VOLUME": 13, "FILTBAL": 14}

# ---------------- os feitos à mão ----------------
MANUAIS = [
    ("SUPERSAW", "Dose Dupla",
     {"o1model": 1, "o1shape": 85, "o1spread": 62, "o2shape": 66, "o2detune": -9, "oscbal": 30, "unison": 2, "unidetune": 30,
      "unispread": 90, "cutoff": 5200, "reso1": 18, "env1": 22, "fa": 2, "fd": 900, "fs": 55, "fr": 700,
      "aa": 3, "ad": 1200, "as": 85, "ar": 650, "chomix": 18, "choint": 40, "dlysend": 18, "dlytime": 5, "dlyfb": 35,
      "rvbsend": 26, "rvbsize": 70, "volume": -9}),
    ("SUPERSAW", "Hino do Clube",
     {"o1model": 1, "o1shape": 100, "o1spread": 78, "oscbal": 15, "unison": 3, "unidetune": 35, "unispread": 100,
      "cutoff": 8000, "reso1": 10, "env1": 12, "aa": 8, "as": 95, "ar": 900, "dlysend": 22, "dlytime": 3, "rvbsend": 35,
      "rvbsize": 80, "volume": -10}),
    ("BASS", "Rolo de Borracha",
     {"keymode": 2, "porta": 40, "o1shape": 66, "o2semi": -12, "o2shape": 90, "oscbal": 40, "subvol": 55, "oscvol": 62,
      "cutoff": 260, "reso1": 32, "env1": 58, "fa": 1, "fd": 260, "fs": 8, "fr": 120, "aa": 1, "ad": 900, "as": 85, "ar": 90,
      "fvel": 50, "boostamt": 30, "boosttune": 90}),
    ("BASS", "Sub Pulso",
     {"keymode": 1, "o1shape": 95, "o1pw": 32, "oscbal": 0, "subvol": 75, "cutoff": 420, "reso1": 20, "env1": 45,
      "fd": 180, "fs": 15, "ad": 600, "as": 90, "ar": 70, "l1pw": 18, "l1rate": 0.6}),
    ("LEAD", "Agulha",
     {"keymode": 2, "porta": 70, "o1shape": 66, "o2shape": 100, "o2sync": 1, "o2semi": 12, "oscbal": 60, "cutoff": 3800,
      "reso1": 28, "env1": 30, "fd": 500, "fs": 40, "as": 100, "ar": 260, "l1pitch": 12, "l1rate": 5.6,
      "m2src": 1, "m2dst": 6, "m2amt": 40, "dlysend": 24, "dlytime": 5, "dlyfb": 40, "rvbsend": 18, "volume": -8}),
    ("LEAD", "Febre Alta",
     {"keymode": 1, "porta": 30, "o1model": 1, "o1spread": 45, "o1shape": 70, "oscbal": 20, "unison": 2, "unidetune": 22,
      "cutoff": 2600, "reso1": 35, "env1": 40, "fd": 350, "fs": 45, "distamt": 28, "disttype": 0, "dlysend": 20, "rvbsend": 15,
      "volume": -9}),
    ("PAD", "Quarentena",
     {"o1model": 1, "o1spread": 55, "o1shape": 60, "o2shape": 40, "o2detune": 12, "oscbal": 45, "unison": 2, "unidetune": 28,
      "cutoff": 1400, "reso1": 18, "env1": 20, "f1mode": 1, "fa": 1200, "fd": 3000, "fs": 60, "fr": 2500,
      "aa": 900, "ad": 2000, "as": 90, "ar": 3000, "l2rate": 0.15, "l2cut": 18, "chomix": 35, "rvbsend": 45, "rvbsize": 85,
      "volume": -9}),
    ("PLUCK", "Gota",
     {"o1shape": 80, "o2semi": 12, "o2shape": 40, "oscbal": 35, "cutoff": 500, "reso1": 25, "env1": 75, "fd": 220, "fs": 0,
      "fr": 200, "ad": 450, "as": 0, "ar": 300, "dlysend": 30, "dlytime": 3, "dlyfb": 45, "rvbsend": 20}),
    ("ARP", "Contágio",
     {"arpon": 1, "arpmode": 2, "arpoct": 2, "arprate": 2, "arpgate": 45, "o1shape": 90, "o1pw": 40, "cutoff": 900,
      "reso1": 35, "env1": 55, "fd": 200, "fs": 10, "ad": 300, "as": 30, "ar": 150, "dlysend": 28, "dlytime": 5, "dlyfb": 50,
      "rvbsend": 15}),
    ("KEYS", "Vidro Elétrico",
     {"o1shape": 10, "o2shape": 0, "o2semi": 12, "fm": 28, "oscbal": 35, "cutoff": 6000, "env1": 10, "ad": 1800, "as": 25,
      "ar": 500, "avel": 60, "chomix": 25, "phamix": 0, "rvbsend": 18}),
    ("ATMOSFERA", "Ar Condicionado",
     {"o1model": 1, "o1spread": 85, "noisevol": 30, "f1mode": 3, "cutoff": 900, "reso1": 55, "env1": 10, "fa": 2500,
      "fs": 70, "fr": 4000, "aa": 2000, "as": 100, "ar": 5000, "l2shape": 5, "l2rate": 0.3, "l2cut": 35, "l2pan": 50,
      "rvbsend": 65, "rvbsize": 95, "dlysend": 25, "volume": -8}),
    ("FX", "Efeito Colateral",
     {"o2sync": 1, "o2semi": 19, "fm": 60, "l1rate": 9, "l1shape": 4, "l1cut": 40, "l2rate": 0.25, "l2fm": 50,
      "cutoff": 1800, "reso1": 60, "env1": 40, "distamt": 30, "disttype": 2, "dlysend": 35, "dlyfb": 60, "rvbsend": 30,
      "volume": -10}),
]

# ---------------- nomes por categoria ----------------
NOMES = {
    "BASS": (["Grave", "Rolo", "Motor", "Tronco", "Pulso", "Fundo", "Porão", "Concreto", "Tambor", "Pistão", "Graxa", "Lastro"],
             ["Escuro", "Seco", "de Borracha", "Quente", "Turbo", "de Ferro", "Gordo", "Mudo", "Elástico", "de Aço", "Rasgado", "Redondo"]),
    "LEAD": (["Agulha", "Faísca", "Sirene", "Lâmina", "Raio", "Grito", "Tiro", "Laser", "Flecha", "Chicote", "Apito", "Brasa"],
             ["Alta", "Afiada", "Fina", "Solar", "de Neon", "Elétrica", "Rasante", "Aberta", "Nervosa", "Brilhante", "Veloz", "Doce"]),
    "PAD": (["Nuvem", "Manto", "Névoa", "Maré", "Aurora", "Cortina", "Seda", "Horizonte", "Vapor", "Lençol", "Brisa", "Oceano"],
            ["Lenta", "Morna", "Profunda", "Larga", "de Vidro", "Dourada", "Calma", "Distante", "Suave", "Cinza", "Clara", "Imensa"]),
    "PLUCK": (["Gota", "Pingo", "Estalo", "Corda", "Bolha", "Toque", "Pedra", "Grão", "Faísca", "Clique", "Beliscão", "Ponto"],
              ["Seca", "de Vidro", "Curta", "Molhada", "Brilhante", "de Madeira", "Leve", "Rápida", "Clara", "Fina", "Doce", "Metálica"]),
    "ARP": (["Escada", "Ciranda", "Roda", "Ponteiro", "Relógio", "Corrente", "Espiral", "Trilho", "Carrossel", "Ciclo", "Engrenagem", "Hélice"],
            ["Rápida", "Infinita", "Subindo", "Descendo", "Nervosa", "Noturna", "Solar", "de Vidro", "Acesa", "Viva", "Torta", "Leve"]),
    "KEYS": (["Piano", "Órgão", "Vidro", "Sino", "Teclado", "Marfim", "Cristal", "Celesta", "Caixa", "Vibra", "Lâmpada", "Ébano"],
             ["Elétrico", "Velho", "de Bar", "Macio", "Brilhante", "Morno", "de Lata", "Claro", "Antigo", "Suave", "Duro", "Limpo"]),
    "SUPERSAW": (["Hino", "Muralha", "Exército", "Arena", "Estádio", "Coro", "Onda", "Avalanche", "Tempestade", "Explosão", "Multidão", "Catedral"],
                 ["do Clube", "Gigante", "Aberto", "Largo", "de Ouro", "Eterno", "Total", "Brilhante", "Infinito", "Solar", "Épico", "Final"]),
    "ATMOSFERA": (["Deserto", "Caverna", "Estação", "Satélite", "Abismo", "Floresta", "Neblina", "Vento", "Gelo", "Eco", "Poeira", "Silêncio"],
                  ["Vazio", "Frio", "Distante", "Escuro", "Molhado", "Antigo", "Sideral", "Branco", "Perdido", "Lunar", "Fundo", "Calmo"]),
    "FX": (["Alarme", "Robô", "Rádio", "Sinal", "Ruído", "Curto", "Vírgula", "Choque", "Sonda", "Glitch", "Disco", "Defeito"],
           ["Quebrado", "Maluco", "Torto", "de Lata", "Elétrico", "Digital", "Fora do Ar", "Solto", "Doido", "Errado", "Espacial", "Falho"]),
}


def faixa(rnd, a, b):
    return a + (b - a) * rnd.random()


def escolha(rnd, *opcoes):
    return rnd.choice(opcoes)


def chance(rnd, p):
    return rnd.random() < p


def receita(cat, rnd):
    """Valores de um preset da categoria (o resto fica no padrão)."""
    v = {}
    r = lambda a, b: faixa(rnd, a, b)

    if cat == "BASS":
        v.update(keymode=escolha(rnd, 1, 2, 2, 0), porta=escolha(rnd, 0, 0, r(20, 90)),
                 o1model=1 if chance(rnd, 0.15) else 0, o1shape=r(55, 100), o1pw=r(25, 50), o1spread=r(15, 40),
                 o2shape=r(50, 100), o2semi=escolha(rnd, -12, 0, 0, 12, 7), o2detune=r(-12, 12), oscbal=r(15, 60),
                 subvol=r(20, 80), oscvol=r(50, 80), cutoff=r(140, 900), reso1=r(5, 60), env1=r(25, 80), key1=r(10, 50),
                 f1mode=escolha(rnd, 0, 0, 0, 1), fa=r(0.5, 4), fd=r(120, 700), fs=r(0, 30), fr=r(40, 220),
                 aa=r(0.5, 3), ad=r(300, 1500), as_=r(55, 100), ar=r(40, 220), fvel=r(20, 60), avel=r(10, 40))
        if chance(rnd, 0.35): v.update(distamt=r(10, 45), disttype=escolha(rnd, 0, 1, 2))
        if chance(rnd, 0.4): v.update(boostamt=r(15, 55), boosttune=r(60, 160))
        if chance(rnd, 0.2): v.update(o2sync=1, o2semi=escolha(rnd, 12, 19, 24))
        if chance(rnd, 0.25): v.update(fm=r(10, 40))
        v["volume"] = r(-7, -4)

    elif cat == "LEAD":
        v.update(keymode=escolha(rnd, 1, 2, 2), porta=r(15, 160), unison=escolha(rnd, 1, 1, 2, 3), unidetune=r(12, 40),
                 unispread=r(40, 90), o1model=1 if chance(rnd, 0.4) else 0, o1shape=r(60, 100), o1spread=r(25, 60),
                 o2shape=r(55, 100), o2semi=escolha(rnd, 0, 7, 12, -12, 0), o2detune=r(-15, 15), oscbal=r(20, 70),
                 cutoff=r(1200, 7000), reso1=r(15, 55), env1=r(15, 50), key1=r(20, 60), fd=r(200, 900), fs=r(30, 70),
                 aa=r(0.5, 15), ad=r(400, 2000), as_=r(70, 100), ar=r(120, 600), l1rate=r(4.5, 6.5), l1pitch=r(0, 16),
                 dlysend=r(10, 35), dlytime=escolha(rnd, 2, 3, 5), dlyfb=r(25, 55), rvbsend=r(8, 25))
        if chance(rnd, 0.3): v.update(o2sync=1, o2semi=escolha(rnd, 12, 19, 24), m2src=SRC["WHEEL"], m2dst=DST["SHAPE2"], m2amt=r(20, 60))
        if chance(rnd, 0.25): v.update(distamt=r(10, 35))
        if chance(rnd, 0.2): v.update(fm=r(10, 45))
        v["volume"] = r(-10, -7)

    elif cat == "PAD":
        v.update(unison=escolha(rnd, 2, 2, 3, 4, 1), unidetune=r(18, 50), unispread=r(60, 100),
                 o1model=1 if chance(rnd, 0.5) else 0, o1shape=r(35, 90), o1spread=r(35, 80), o2shape=r(30, 80),
                 o2semi=escolha(rnd, 0, 0, 12, 7, -12), o2detune=r(-18, 18), oscbal=r(30, 60), cutoff=r(500, 3200),
                 reso1=r(8, 35), env1=r(5, 40), f1mode=escolha(rnd, 0, 1, 1), fa=r(300, 2500), fd=r(1000, 5000),
                 fs=r(40, 80), fr=r(1000, 5000), aa=r(300, 2500), ad=r(1500, 4000), as_=r(80, 100), ar=r(1500, 5000),
                 l2rate=r(0.08, 0.6), l2cut=r(5, 25), l1rate=r(0.2, 1.2), l1pw=r(0, 30),
                 chomix=r(15, 50), choint=r(30, 70), rvbsend=r(30, 65), rvbsize=r(65, 95))
        if chance(rnd, 0.25): v.update(phamix=r(20, 50), phaint=r(20, 60))
        if chance(rnd, 0.3): v.update(noisevol=r(5, 20))
        v["volume"] = r(-11, -8)

    elif cat == "PLUCK":
        v.update(unison=escolha(rnd, 1, 1, 2), unidetune=r(10, 30), o1model=1 if chance(rnd, 0.2) else 0,
                 o1shape=r(55, 100), o1pw=r(20, 50), o2shape=r(30, 100), o2semi=escolha(rnd, 0, 12, 7, 19, -12),
                 o2detune=r(-10, 10), oscbal=r(20, 55), cutoff=r(250, 1500), reso1=r(10, 50), env1=r(45, 90), key1=r(30, 70),
                 fa=r(0.5, 3), fd=r(100, 500), fs=r(0, 12), fr=r(100, 500), aa=r(0.5, 3), ad=r(200, 1000), as_=r(0, 20),
                 ar=r(150, 700), fvel=r(30, 70), avel=r(30, 60), dlysend=r(15, 40), dlytime=escolha(rnd, 2, 3, 5),
                 dlyfb=r(25, 55), rvbsend=r(10, 30))
        if chance(rnd, 0.3): v.update(fm=r(10, 50))
        v["volume"] = r(-8, -5)

    elif cat == "ARP":
        v.update(arpon=1, arpmode=escolha(rnd, 0, 0, 1, 2, 2, 3, 4), arpoct=escolha(rnd, 1, 2, 2, 3), arprate=escolha(rnd, 2, 2, 2, 1, 3, 0),
                 arpgate=r(25, 75), o1model=1 if chance(rnd, 0.25) else 0, o1shape=r(55, 100), o1pw=r(20, 50),
                 o2shape=r(40, 100), o2semi=escolha(rnd, 0, 12, 7, -12), o2detune=r(-10, 10), oscbal=r(20, 55),
                 cutoff=r(350, 2200), reso1=r(15, 55), env1=r(35, 85), fd=r(100, 450), fs=r(0, 25), fr=r(100, 400),
                 ad=r(200, 800), as_=r(10, 60), ar=r(100, 400), dlysend=r(15, 45), dlytime=escolha(rnd, 1, 2, 3, 5),
                 dlyfb=r(30, 60), rvbsend=r(8, 30))
        if chance(rnd, 0.35): v.update(l1rate=r(0.1, 0.5), l1cut=r(10, 35))
        if chance(rnd, 0.2): v.update(arphold=0, fm=r(10, 40))
        v["volume"] = r(-9, -6)

    elif cat == "KEYS":
        v.update(o1shape=r(0, 45), o2shape=r(0, 40), o2semi=escolha(rnd, 12, 19, 24, 0, 7), o2detune=r(-6, 6),
                 fm=r(0, 45), oscbal=r(20, 50), cutoff=r(1500, 9000), reso1=r(0, 25), env1=r(5, 35), key1=r(30, 70),
                 fd=r(400, 2000), fs=r(10, 50), aa=r(0.5, 5), ad=r(600, 3000), as_=r(15, 65), ar=r(250, 900),
                 avel=r(40, 75), fvel=r(30, 60), chomix=r(0, 40), rvbsend=r(10, 30))
        if chance(rnd, 0.3): v.update(phamix=r(20, 55), phaint=r(20, 60))
        if chance(rnd, 0.3): v.update(l1amp=r(10, 35), l1rate=r(3, 7))
        if chance(rnd, 0.2): v.update(distamt=r(5, 20))
        v["volume"] = r(-8, -5)

    elif cat == "SUPERSAW":
        v.update(o1model=1, o1shape=r(55, 100), o1spread=r(35, 90), unison=escolha(rnd, 1, 2, 2, 3), unidetune=r(15, 45),
                 unispread=r(60, 100), o2shape=r(55, 90), o2detune=r(-14, 14), o2semi=escolha(rnd, 0, 0, 12, -12, 7),
                 oscbal=r(10, 40), cutoff=r(1800, 11000), reso1=r(5, 30), env1=r(5, 40), fd=r(400, 1500), fs=r(40, 80),
                 aa=r(0.5, 60), ad=r(800, 2500), as_=r(70, 100), ar=r(300, 1500), chomix=r(0, 30),
                 dlysend=r(10, 30), dlytime=escolha(rnd, 3, 5, 6), rvbsend=r(15, 40), rvbsize=r(55, 85))
        if chance(rnd, 0.3): v.update(keymode=escolha(rnd, 1, 2), porta=r(20, 120))
        if chance(rnd, 0.2): v.update(arpon=1, arpmode=escolha(rnd, 0, 2, 5), arprate=escolha(rnd, 2, 4), arpgate=r(30, 60))
        v["volume"] = r(-12, -9)

    elif cat == "ATMOSFERA":
        v.update(unison=escolha(rnd, 1, 2, 3), unidetune=r(20, 60), unispread=r(70, 100), o1model=1 if chance(rnd, 0.5) else 0,
                 o1shape=r(20, 90), o1spread=r(40, 95), o2shape=r(0, 80), o2semi=escolha(rnd, 0, 7, 12, 19, -12),
                 fm=r(0, 40) if chance(rnd, 0.4) else 0, noisevol=r(0, 40), cutoff=r(300, 2500), reso1=r(20, 70),
                 f1mode=escolha(rnd, 0, 1, 3, 3, 2), env1=r(-20, 30), fa=r(800, 4000), fs=r(40, 90), fr=r(2000, 6000),
                 aa=r(800, 4000), as_=r(80, 100), ar=r(2500, 7000), l2shape=escolha(rnd, 0, 1, 5), l2rate=r(0.05, 0.5),
                 l2cut=r(10, 40), l2pan=r(20, 70), l1rate=r(0.1, 2), l1pw=r(0, 40),
                 rvbsend=r(45, 85), rvbsize=r(75, 100), rvbdamp=r(20, 70), dlysend=r(10, 35), dlytime=escolha(rnd, 4, 5, 6),
                 dlyfb=r(40, 70))
        if chance(rnd, 0.35): v.update(phamix=r(20, 60), phaint=r(10, 50))
        if chance(rnd, 0.3): v.update(routing=1, filtbal=r(30, 70), f2mode=escolha(rnd, 1, 2), cutoff2=int(r(-12, 24)))
        v["volume"] = r(-11, -7)

    else:  # FX
        v.update(o1shape=r(30, 100), o2shape=r(30, 100), o2semi=int(r(-24, 24)), o2sync=escolha(rnd, 0, 1), fm=r(20, 100),
                 noisevol=r(0, 50), cutoff=r(300, 6000), reso1=r(30, 90), env1=r(-60, 80), f1mode=escolha(rnd, 0, 1, 2, 3, 4),
                 fd=r(100, 3000), fs=r(0, 60), aa=r(0.5, 500), ad=r(200, 3000), as_=r(20, 100), ar=r(200, 3000),
                 l1rate=r(0.5, 20), l1shape=escolha(rnd, 0, 2, 3, 4, 5), l1pitch=r(-60, 60) if chance(rnd, 0.4) else 0,
                 l1cut=r(-60, 60), l2rate=r(0.1, 8), l2shape=escolha(rnd, 1, 4, 5), l2fm=r(-60, 60), l2pan=r(0, 80),
                 m2src=SRC["RANDOM"], m2dst=escolha(rnd, DST["PITCH"], DST["CUTOFF"], DST["SHAPE2"], DST["FM"]), m2amt=r(-40, 40),
                 m3src=escolha(rnd, SRC["FENV"], SRC["LFO2"], SRC["AENV"]), m3dst=escolha(rnd, DST["PITCH"], DST["FM"], DST["PAN"]),
                 m3amt=r(-60, 60), dlysend=r(15, 50), dlyfb=r(40, 85), dlytime=int(r(0, 6.99)), rvbsend=r(15, 50))
        if chance(rnd, 0.4): v.update(distamt=r(15, 70), disttype=escolha(rnd, 1, 2, 3))
        if chance(rnd, 0.3): v.update(phamix=r(30, 80), phaint=r(40, 90))
        if chance(rnd, 0.25): v.update(arpon=1, arpmode=3, arprate=escolha(rnd, 0, 1, 2), arpoct=escolha(rnd, 2, 3, 4), arpgate=r(10, 50))
        v["volume"] = r(-12, -8)

    if "as_" in v:
        v["as"] = v.pop("as_")
    return v


def limpa(valores):
    """Arredonda, limita às faixas e tira o que é igual a zero/padrão óbvio."""
    out = {}
    for k, x in valores.items():
        lo, hi = FAIXAS[k]
        x = max(lo, min(hi, x))
        if k in INTEIROS:
            x = int(round(x))
        elif k in ("l1rate", "l2rate"):
            x = round(x, 2)
        elif abs(x) >= 100:
            x = round(x)
        else:
            x = round(x, 1)
        out[k] = x
    return out


def nomes_da_categoria(cat, usados, rnd):
    a, b = NOMES[cat]
    pares = [f"{x} {y}" for x in a for y in b]
    rnd.shuffle(pares)
    for nome in pares:
        if nome.lower() not in usados:
            usados.add(nome.lower())
            yield nome


def fmt(x):
    if isinstance(x, int):
        return f"{x}.0f"
    s = f"{x:.2f}".rstrip("0").rstrip(".")
    return (s if "." in s else s + ".0") + "f"


def main():
    usados = {m[1].lower() for m in MANUAIS}
    linhas = [
        "// GERADO por tools/gerar-presets.py — não editar à mão (mude o script e rode de novo).",
        "// VACINA — banco FACTORY 1: 40 presets por categoria, na ordem de categoryNames.",
        "// { nome, categoria, { valores REAIS } }",
    ]
    total = 0
    for ci, cat in enumerate(CATEGORIAS):
        linhas += ["", f"// ---- {cat} ----"]
        lista = [(nome, limpa(v)) for c, nome, v in MANUAIS if c == cat]
        gerador = nomes_da_categoria(cat, usados, random.Random(zlib.crc32(cat.encode())))
        while len(lista) < POR_CATEGORIA:
            nome = next(gerador)
            rnd = random.Random(zlib.crc32(nome.encode("utf-8")))
            lista.append((nome, limpa(receita(cat, rnd))))
        for nome, vals in lista:
            corpo = ", ".join(f'{{ "{k}", {fmt(x)} }}' for k, x in vals.items())
            linhas.append(f'{{ "{nome}", {ci}, {{ {corpo} }} }},')
            total += 1
    destino = Path(__file__).resolve().parent.parent / "Source" / "presets" / "fabrica.inc"
    destino.write_text("\n".join(linhas) + "\n", encoding="utf-8", newline="\n")
    print(f"{total} presets em {destino}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Gera o banco FACTORY 1 do RESENHA: Source/presets/fabrica.inc (12 presets por categoria, 9 categorias).

Uso:
    python gerar-presets.py

Cada preset traz os 4 timbres, o gerador, a troca de timbre, os efeitos E a frase. Sai de receitas por categoria
(faixas de parâmetro + jeito de montar a frase), com semente fixa pelo nome — rodar de novo dá o mesmo arquivo.
Frase em texto (ver parsePattern em Phrase.h): um item por passo, "-" pausa, "~" ligadura, "grau:gate" e "!" = accent.
"""
import random
import zlib
from pathlib import Path

CATEGORIAS = ["PSY CLÁSSICO", "FULL ON", "DARK", "GOA", "PROG", "HI-TECH", "MINIMAL", "FX", "CONVERSA FM"]

NOMES = [
    ["Papo Reto", "Bate-Papo", "Resenha Clássica", "Pergunta e Resposta", "Trocando Ideia", "Conversa Fiada",
     "Dois Dedos de Prosa", "Eco no Vale", "Diálogo Ácido", "Chamada", "Tréplica", "Fala Comigo"],
    ["Papo de Pista", "Zap Zap", "Pico da Festa", "Mão pro Alto", "Voa Lead", "Full Resenha",
     "Alto Astral", "Turbo Papo", "Sol Nascendo", "Grito no Morro", "Chama no Lead", "Rodinha"],
    ["Papo Sombrio", "Sussurro", "Floresta Noturna", "Voz do Porão", "Lua Negra", "Conversa no Breu",
     "Assombração", "Neblina", "Ritual", "Eco Profundo", "Vulto", "Meia-Noite"],
    ["Papo de Goa", "Mantra", "Anjuna", "Templo", "Incenso", "Caravana",
     "Om Lead", "Praia de Goa", "Bazar", "Serpente", "Lótus", "Kali"],
    ["Conversa Longa", "Rolando", "Maré", "Horizonte", "Passo a Passo", "Respira",
     "Deriva", "Fluxo", "Ondas", "Viagem", "Brisa", "Ponte"],
    ["Fofoca Rápida", "Metralhadora", "Curto-Circuito", "Hiperpapo", "Pixel", "Laser",
     "Faísca", "Turbina", "Glitch", "Nanobate", "Elétrico", "Quântico"],
    ["Pouca Ideia", "Monossílabo", "Ponto e Vírgula", "Quase Nada", "Gota", "Pingo",
     "Respiro", "Silêncio Bom", "Clique", "Toque", "Sopro", "Pausa"],
    ["Alienígena", "Rádio Perdido", "Golfinho", "Robô Falante", "Disco Voador", "Sirene",
     "Telefone Sem Fio", "Pássaro Metálico", "Vórtice", "Arma Laser", "Eco Cósmico", "Ruído Branco"],
    ["Mergulho", "Escada e Queda", "Fala Rasgada", "Corrida no Morro", "Pergunta Afiada", "Resposta Grave",
     "Dois Leads", "Voo Rasante", "Bate e Volta", "Ladeira Abaixo", "Papo de 144", "Última Palavra"],
]

# índices das listas de Params.h / Scales.h
ESCALAS = {"CHROMATIC": 0, "MAJOR": 1, "MINOR": 2, "DORIAN": 3, "PHRYGIAN": 4, "LYDIAN": 5, "MIXOLYDIAN": 6,
           "LOCRIAN": 7, "HARM MINOR": 8, "MEL MINOR": 9, "PENTA MAJ": 10, "PENTA MIN": 11, "BLUES": 12, "PHRYG DOM": 13}
GRAUS_POR_OITAVA = [12, 7, 7, 7, 7, 7, 7, 7, 7, 7, 5, 5, 6, 7]
RATIOS = [0.5, 1, 1.5, 2, 2.5, 3, 3.5, 4, 5, 6, 7, 8, 9, 11, 13]   # o parâmetro guarda o índice


def ratio_idx(r):
    return RATIOS.index(r)


# receita de cada categoria (ratios = a razão em si, não o índice): escalas, rates (0=1/32 1=1/16T 2=1/16 3=1/8T 4=1/8), compassos (0=1 1=2 2=4),
# densidade, gate, troca (índice em 1,2,3,4,6,8), respostas (0 LIVRE 1 ECO 2 VARIA 3 INVERTE), alturas
# (0 MESMA 1 +OITAVA 2 +5ª 3 −OITAVA), vozes (índice: 1..4 → 0..3), e o "jeito" dos timbres
RECEITAS = [
    dict(escalas=["PHRYGIAN", "MINOR", "HARM MINOR"], rates=[2], bars=[1], dens=(55, 80), gate=(35, 60),
         every=[2, 3, 3], resp=[1, 1, 2, 3], itv=[1, 1, 0, 2], vozes=[1, 2], ondas=[2, 3, 2, 0],
         ratios=[2, 3, 4, 1], fm=(35, 70), fb=(10, 40), fmdec=(80, 300), cut=(1800, 6000), res=(15, 50),
         dec=(180, 420), glide=(0.4, 20, 90), penv=(0.3, [12, 7]), fenv=(15, 60), dl=(0.3, 0.5), rv=(0.12, 0.25)),
    dict(escalas=["PHRYGIAN", "HARM MINOR", "PHRYG DOM"], rates=[2], bars=[1, 2], dens=(65, 90), gate=(30, 55),
         every=[2, 3, 3, 4], resp=[1, 2, 1], itv=[1, 1, 2], vozes=[1, 2, 3], ondas=[2, 2, 0, 3],
         ratios=[2, 3, 4, 6, 8], fm=(45, 80), fb=(15, 45), fmdec=(60, 220), cut=(2500, 8000), res=(20, 55),
         dec=(150, 380), glide=(0.5, 30, 110), penv=(0.6, [12, 12, 7, 24]), fenv=(30, 70), dl=(0.35, 0.5), rv=(0.12, 0.22)),
    dict(escalas=["PHRYGIAN", "LOCRIAN", "CHROMATIC"], rates=[2, 1], bars=[1, 2], dens=(50, 85), gate=(20, 45),
         every=[1, 2, 3], resp=[3, 2, 0], itv=[0, 3, 1], vozes=[1, 2], ondas=[3, 2, 0],
         ratios=[6, 7, 11, 13, 3.5, 9], fm=(50, 90), fb=(35, 75), fmdec=(40, 180), cut=(900, 3500), res=(30, 70),
         dec=(120, 320), glide=(0.3, 10, 60), penv=(0.4, [-12, -7, 12]), fenv=(25, 65), dl=(0.25, 0.45), rv=(0.18, 0.32)),
    dict(escalas=["PHRYG DOM", "HARM MINOR", "PHRYGIAN"], rates=[2, 3], bars=[1, 2], dens=(55, 80), gate=(40, 75),
         every=[3, 3, 4], resp=[1, 2, 1, 3], itv=[1, 2, 1], vozes=[1, 2, 3], ondas=[2, 0, 2, 1],
         ratios=[1, 3, 2, 5], fm=(30, 65), fb=(10, 40), fmdec=(120, 450), cut=(1500, 5000), res=(35, 70),
         dec=(250, 600), glide=(0.6, 40, 140), penv=(0.3, [12, 5]), fenv=(20, 55), dl=(0.4, 0.6), rv=(0.2, 0.35)),
    dict(escalas=["MINOR", "DORIAN", "PENTA MIN"], rates=[2, 4], bars=[2, 2, 1], dens=(40, 70), gate=(25, 50),
         every=[3, 4, 5], resp=[2, 1, 0], itv=[0, 1, 3], vozes=[1, 2], ondas=[0, 1, 3, 2],
         ratios=[1, 3, 5, 7], fm=(20, 55), fb=(0, 30), fmdec=(150, 600), cut=(1200, 4500), res=(20, 55),
         dec=(200, 500), glide=(0.3, 20, 80), penv=(0.15, [12]), fenv=(25, 70), dl=(0.3, 0.45), rv=(0.2, 0.35)),
    dict(escalas=["PHRYGIAN", "HARM MINOR", "MINOR"], rates=[0, 1], bars=[0, 1], dens=(70, 95), gate=(20, 45),
         every=[1, 2, 3], resp=[1, 3, 2], itv=[1, 0, 2], vozes=[2, 3], ondas=[0, 2, 3, 1],
         ratios=[7, 9, 11, 8, 13], fm=(55, 95), fb=(20, 55), fmdec=(25, 120), cut=(3000, 9000), res=(15, 45),
         dec=(80, 220), glide=(0.2, 10, 40), penv=(0.5, [12, 24]), fenv=(20, 60), dl=(0.2, 0.35), rv=(0.08, 0.18)),
    dict(escalas=["MINOR", "PENTA MIN", "DORIAN"], rates=[2, 4], bars=[1, 2], dens=(20, 45), gate=(15, 40),
         every=[1, 2, 3], resp=[1, 0, 2], itv=[0, 1, 2], vozes=[0, 1], ondas=[0, 1, 0],
         ratios=[1, 3, 5], fm=(15, 45), fb=(0, 25), fmdec=(60, 250), cut=(1500, 5000), res=(10, 40),
         dec=(120, 350), glide=(0.2, 10, 50), penv=(0.2, [12, 7]), fenv=(10, 40), dl=(0.35, 0.55), rv=(0.2, 0.35)),
    dict(escalas=["CHROMATIC", "LOCRIAN", "BLUES", "LYDIAN"], rates=[1, 2, 3], bars=[0, 1], dens=(35, 80), gate=(20, 80),
         every=[0, 1, 2], resp=[3, 2, 0, 1], itv=[2, 3, 1], vozes=[1, 2, 3], ondas=[0, 1, 2, 3],
         ratios=[0.5, 1.5, 2.5, 3.5, 13, 11], fm=(50, 100), fb=(30, 90), fmdec=(200, 1200), cut=(800, 9000), res=(40, 85),
         dec=(150, 900), glide=(0.8, 80, 400), penv=(0.9, [24, -24, 12, -12, 19]), fenv=(40, 100), dl=(0.4, 0.6), rv=(0.25, 0.45)),
    # CONVERSA FM — dos trechos que ele mandou (29/10, full on a 144): leads FM cheios de harmônico (serra/quadrada
    # com feedback alto, razão baixa), notas de 1/16 em escada que fecham a fala num mergulho de 7 a 24 semitons, a
    # resposta numa oitava diferente, e o lead "abaixando" a cada tempo (sidechain do bumbo).
    dict(escalas=["PHRYGIAN", "HARM MINOR", "PHRYG DOM", "MINOR"], rates=[2], bars=[1, 1, 0], dens=(60, 85), gate=(45, 80),
         every=[3, 3, 4], resp=[1, 2, 1, 3], itv=[3, 1, 3, 0], vozes=[1, 1, 2], ondas=[2, 3, 2],
         ratios=[1, 1, 2, 0.5, 3, 1.5], fm=(50, 85), fb=(35, 75), fmdec=(120, 500), cut=(1500, 5000), res=(30, 65),
         dec=(250, 700), glide=(0.6, 15, 70), penv=(0.35, [12, 7]), fenv=(35, 75), dl=(0.18, 0.32), rv=(0.08, 0.18),
         bend=(0.85, 0.55, [-12, -12, -24, -7, -12, 12]), contorno=[1, 1, 3, 2, 0], duck=(45, 75)),
]

CELULAS = ["n---", "n-~-", "n~~~", "n--n", "n-n-", "nn--", "n~-n", "-nn-", "n-nn", "nn-n",
           "nn~n", "n~nn", "nnn-", "-nnn", "nnnn", "nnnn"]
PASSOS = 64


def corridas(rnd, per_g, top, contorno, guardado):
    """CORRIDA/QUEDA/ONDA (generateRuns em Phrase.h): escadas de 4 a 8 passos, de grau em grau ou em arpejo"""
    stride = 1 if rnd.randrange(3) == 0 else 2
    top = min(top, per_g + 3)
    span = max(per_g, top)
    graus = []
    while len(graus) < PASSOS:
        tam = 4 + 2 * rnd.randrange(3)
        if contorno == 2:
            ini = span - rnd.randrange(3)
        else:
            ini = rnd.randrange(3) - 1 + (per_g // 2 if rnd.randrange(4) == 0 else 0)
        for j in range(tam):
            if len(graus) >= PASSOS:
                break
            if contorno == 1:
                d = ini + j * stride
            elif contorno == 2:
                d = ini - j * stride
            else:
                d = ini + (j if j < tam // 2 else tam - 1 - j) * stride
            while d > top + 1:
                d -= per_g
            while d < -2:
                d += per_g
            graus.append(max(-14, min(21, guardado(d))))
    return graus


def frase(rnd, per, oitavas, dens, gate, gatevar, cromatica, contorno=0):
    """o mesmo jeito do gerador do plugin (Phrase.h): ritmo por células, motivo curto repetido com variações
    (ou, com CONTORNO, escadas que sobem/descem)"""
    modo = []
    for g in range(0, PASSOS, 4):
        c = round(dens * 15 + (rnd.random() - 0.5) * 7)
        if rnd.randrange(8) == 0:
            c = rnd.randrange(16)
        modo += list(CELULAS[max(0, min(15, c))])
    modo[0] = "n"

    menor = [0, 2, 3, 5, 7, 8, 10]
    per_g = 7 if cromatica else per
    top = per_g * oitavas

    def guardado(d):
        if not cromatica:
            return d
        o = d // 7
        return o * 12 + menor[d - o * 7]

    escada = corridas(rnd, per_g, top, contorno, guardado) if contorno else None

    tam = 4 + rnd.randrange(5)
    motivo, d = [], (per_g if rnd.randrange(3) == 0 else 0)
    for _ in range(tam):
        motivo.append(d)
        r = rnd.randrange(100)
        mv = 1 if r < 30 else -1 if r < 55 else 2 if r < 68 else -2 if r < 80 else 0 if r < 88 else rnd.choice([per_g, -per_g])
        if rnd.randrange(6) == 0:
            mv = rnd.choice([0, per_g]) - d
        d = max(-2, min(top, d + mv))
    desloc = [0, 0, 1, -1, 2, 0, -2, 3]

    itens = []
    for i in range(PASSOS):
        rep = i // tam
        deg = motivo[i % tam] + (0 if rep == 0 else desloc[rnd.randrange(8)] * (1 if rnd.randrange(3) == 0 else 0))
        if rnd.randrange(12) == 0:
            deg += rnd.choice([1, -1])
        deg = max(-14, min(21, guardado(max(-2, min(top, deg)))))
        if escada:
            deg = escada[i]
        g = gate + (rnd.random() * 2 - 1) * gatevar * 0.6
        gq = max(0, min(15, round(max(0.05, min(1.0, g)) * 16) - 1))
        acc = rnd.randrange(100) < (30 if i % 4 == 0 else 12)
        m = modo[i]
        itens.append("-" if m == "-" else "~" if m == "~" else f"{deg}:{gq}" + ("!" if acc else ""))
    return " ".join(itens)


def faixa(rnd, a, b, log=False):
    if log:
        import math
        return a * math.exp(rnd.random() * math.log(b / a))
    return a + rnd.random() * (b - a)


def preset(nome, cat):
    rnd = random.Random(zlib.crc32(nome.encode("utf-8")))
    rc = RECEITAS[cat]
    v = {}
    for k in range(4):
        p = "abcd"[k]
        v[p + "wave"] = rc["ondas"][(k + rnd.randrange(2)) % len(rc["ondas"])]
        v[p + "ratio"] = ratio_idx(rc["ratios"][rnd.randrange(len(rc["ratios"]))])   # receita em razão; o parâmetro guarda o índice
        v[p + "fm"] = round(faixa(rnd, *rc["fm"]))
        v[p + "fb"] = round(faixa(rnd, *rc["fb"]))
        v[p + "fmdec"] = round(faixa(rnd, *rc["fmdec"], log=True))
        v[p + "cut"] = round(faixa(rnd, *rc["cut"], log=True))
        v[p + "res"] = round(faixa(rnd, *rc["res"]))
        v[p + "dec"] = round(faixa(rnd, *rc["dec"], log=True))
        v[p + "pan"] = (-1 if k % 2 == 0 else 1) * round(faixa(rnd, 10, 45))
        prob, g0, g1 = rc["glide"]
        v[p + "glide"] = round(faixa(rnd, g0, g1)) if rnd.random() < prob or k == 1 and prob >= 0.4 else 0
        pprob, opts = rc["penv"]
        v[p + "penv"] = rnd.choice(opts) if rnd.random() < pprob or (k == 0 and pprob >= 0.3) else 0
        v[p + "fenv"] = round(faixa(rnd, *rc["fenv"]))
        if "bend" in rc:
            pa, po, opts = rc["bend"]
            v[p + "bend"] = rnd.choice(opts) if rnd.random() < (pa if k == 0 else po) else 0

    esc = rnd.choice(rc["escalas"])
    v["scale"] = ESCALAS[esc]
    v["root"] = rnd.choice([1, 1, 4, 6, 9, 11, 2, 7])            # C#, E, F#, A, B, D, G (tons comuns de psy)
    v["bars"] = rnd.choice(rc["bars"])
    v["rate"] = rnd.choice(rc["rates"])
    v["octaves"] = rnd.choice([1, 2, 2, 3])
    dens = faixa(rnd, *rc["dens"])
    gate = faixa(rnd, *rc["gate"])
    gatevar = faixa(rnd, 10, 45)
    v["density"] = round(dens)
    v["gate"] = round(gate)
    v["gatevar"] = round(gatevar)
    v["swing"] = rnd.choice([0, 0, 0, 8, 15])
    v["voices"] = rnd.choice(rc["vozes"])
    v["every"] = rnd.choice(rc["every"])
    v["order"] = rnd.choice([0, 0, 1, 2])
    v["response"] = rnd.choice(rc["resp"])
    v["interval"] = rnd.choice(rc["itv"])
    v["drvtype"] = rnd.choice([0, 0, 1, 2])
    v["drive"] = round(faixa(rnd, 10, 45))
    v["tone"] = round(faixa(rnd, 55, 85))
    v["dlon"] = 1
    v["dldiv"] = rnd.choice([1, 3, 3, 2, 5])
    v["dlfb"] = round(faixa(rnd, 30, 60))
    v["dlmix"] = round(faixa(rnd, *rc["dl"]) * 100)
    v["rvon"] = 1
    v["rvsize"] = round(faixa(rnd, 40, 75))
    v["rvmix"] = round(faixa(rnd, *rc["rv"]) * 100)
    v["volume"] = -6
    contorno = rnd.choice(rc["contorno"]) if "contorno" in rc else 0
    if "contorno" in rc:
        v["contour"] = contorno
    if "duck" in rc:
        v["duck"] = round(faixa(rnd, *rc["duck"]))

    cromatica = esc == "CHROMATIC"
    texto = frase(rnd, GRAUS_POR_OITAVA[ESCALAS[esc]], v["octaves"], dens / 100, gate / 100, gatevar / 100, cromatica, contorno)
    return v, texto


def main():
    raiz = Path(__file__).resolve().parent.parent
    saida = raiz / "Source" / "presets" / "fabrica.inc"
    linhas = ["// GERADO por tools/gerar-presets.py — não editar à mão (mude o script e rode de novo).",
              "// RESENHA — banco FACTORY 1: 12 presets por categoria, na ordem de categoryNames.",
              "// { nome, categoria, { valores REAIS }, frase }", ""]
    total = 0
    for cat, nomes in enumerate(NOMES):
        linhas.append(f"// ---- {CATEGORIAS[cat]} ----")
        for nome in nomes:
            vals, texto = preset(nome, cat)
            corpo = ", ".join(f'{{ "{k}", {float(x)}f }}' for k, x in vals.items())
            linhas.append(f'{{ "{nome}", {cat}, {{ {corpo} }},\n  "{texto}" }},')
            total += 1
        linhas.append("")
    saida.parent.mkdir(parents=True, exist_ok=True)
    saida.write_text("\n".join(linhas), encoding="utf-8", newline="\n")
    print(f"{total} presets em {saida}")


if __name__ == "__main__":
    main()

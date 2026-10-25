#!/usr/bin/env python3
"""Recorta as peças renderizadas pelo sprites.py e grava o que o plugin embute.

    python recorta.py <pasta dos renders> <pasta _comum>

- assets/fundo.jpg            o pedal sem knobs (JPEG: não precisa de transparência)
- assets/knob_00..15.png      knob grandão; rate_/mix_ os pequenos; alav_* a alavanca (recortados, com alfa)
- assets/nome_<pedal>.png     plaquinha do nome, com a borda esfumada para casar com o fundo
- Source/Mapa.h               as posições em pixels (espaço da imagem 1000 x 1284) para a interface desenhar por cima
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

REN, COMUM = sys.argv[1], sys.argv[2]
ASSETS = os.path.join(COMUM, "assets")
os.makedirs(ASSETS, exist_ok=True)
m = json.load(open(os.path.join(REN, "mapa.json"), encoding="utf-8"))
ret = m["ret"]


def recorte(arq, r, saida, esfuma=0, sombra=0):
    im = Image.open(os.path.join(REN, arq)).convert("RGBA").crop(tuple(r))
    if sombra:
        # a sombra do shadow catcher passa da borda do recorte: some aos poucos em vez de terminar num corte reto
        # (só nos pixels de sombra — pretos e translúcidos —, a peça em si fica intacta)
        a = np.asarray(im).astype(np.float32)
        h, w = a.shape[:2]
        yy, xx = np.mgrid[0:h, 0:w]
        rampa = np.clip(np.minimum.reduce([xx, yy, w - 1 - xx, h - 1 - yy]) / sombra, 0, 1)
        eh_sombra = (a[..., :3].max(axis=2) < 10) & (a[..., 3] < 250)
        a[..., 3] = np.where(eh_sombra, a[..., 3] * rampa, a[..., 3])
        im = Image.fromarray(a.astype(np.uint8), "RGBA")
    if esfuma:
        # só as letras: alfa onde o render com o nome difere do fundo (a chapa em volta fica a do fundo, sem emenda)
        f = Image.open(os.path.join(REN, "fundo.png")).convert("RGB").crop(tuple(r))
        d = np.abs(np.asarray(im.convert("RGB")).astype(np.int16) - np.asarray(f).astype(np.int16)).max(axis=2)
        letra = (d > 8) & (np.asarray(im)[..., 3] == 255)   # fora da área renderizada o render é transparente
        letra[:4, :] = letra[-4:, :] = False
        letra[:, :4] = letra[:, -4:] = False
        a = Image.fromarray((letra * 255).astype(np.uint8), "L")
        a = a.filter(ImageFilter.MaxFilter(2 * (esfuma // 3) + 1)).filter(ImageFilter.GaussianBlur(esfuma / 4))
        im.putalpha(a)
    im.save(os.path.join(ASSETS, saida), optimize=True)


if "--so-mapa" not in sys.argv:
    Image.open(os.path.join(REN, "fundo.png")).convert("RGB").save(os.path.join(ASSETS, "fundo.jpg"), quality=92, optimize=True)
    for q in range(16):
        for peca in ("knob", "rate", "mix"):
            recorte(f"{peca}_{q:02d}.png", ret[peca], f"{peca}_{q:02d}.png", sombra=60 if peca == "knob" else 14)
    for e in ("int", "midi", "audio"):
        recorte(f"alav_{e}.png", ret["alav"], f"alav_{e}.png", sombra=14)
    for arq in sorted(os.listdir(REN)):
        if arq.startswith("nome_") and arq.endswith(".png"):
            recorte(arq, ret["nome"], arq, esfuma=10)


def fl(x):
    return f"{float(x)}f"   # 500 -> 500.0f (o C++ não aceita "500f")


def e(d):
    return f"{{ {fl(d['c'][0])}, {fl(d['c'][1])}, {fl(d['rx'])}, {fl(d['ry'])} }}"


def p(v):
    return f"{{ {fl(v[0])}, {fl(v[1])} }}"


def r(v):
    return f"{{ {v[0]}, {v[1]}, {v[2]}, {v[3]} }}"


L = ["#pragma once",
     "// GERADO por tools/recorta.py a partir do mapa.json do sprites.py — não editar à mão.",
     "// Posições em pixels no espaço da imagem renderizada (1000 x 1284).",
     "namespace sk::mapa",
     "{",
     "struct E { float x, y, rx, ry; };   // centro e pixels por unidade do Blender (x e y)",
     "struct P { float x, y; };",
     "struct R { int x0, y0, x1, y1; };",
     f"constexpr float W = {fl(m['tam'][0])}, H = {fl(m['tam'][1])}, un = {fl(m['un'])};",
     "constexpr E lamp[11] = {",
     ",\n".join("    " + e(l) for l in m["lampadas"]),
     "};",
     f"constexpr E knobTampa = {e(m['knob_tampa'])}, knobSaia = {e(m['knob_saia'])}, knobBase = {e(m['knob_base'])};",
     f"constexpr E rateTampa = {e(m['rate_tampa'])}, mixTampa = {e(m['mix_tampa'])};",
     f"constexpr P txtTrigger = {p(m['txt_trigger'])}, txtRate = {p(m['txt_rate'])}, txtShape = {p(m['txt_shape'])}, txtMix = {p(m['txt_mix'])};",
     f"constexpr P alavanca = {p(m['alavanca'])};",
     f"constexpr float visor[4] = {{ {', '.join(fl(v) for v in m['visor'][0] + m['visor'][1])} }};",
     f"constexpr R retKnob = {r(ret['knob'])}, retRate = {r(ret['rate'])}, retMix = {r(ret['mix'])};",
     f"constexpr R retAlav = {r(ret['alav'])}, retNome = {r(ret['nome'])};",
     "}",
     ""]
with open(os.path.join(COMUM, "Source", "Mapa.h"), "w", encoding="utf-8", newline="\n") as f:
    f.write("\n".join(L))
tot = sum(os.path.getsize(os.path.join(ASSETS, a)) for a in os.listdir(ASSETS))
print(f"{len(os.listdir(ASSETS))} imagens, {tot / 1e6:.1f} MB; Mapa.h gravado")

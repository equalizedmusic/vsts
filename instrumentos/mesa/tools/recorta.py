#!/usr/bin/env python3
"""Prepara o que o plugin embute a partir dos renders do render_mesa.py.

    python recorta.py <pasta dos renders> <pasta instrumentos/mesa>

- Source/img/chapas.jpg   módulos, ponte de VUs, madeira e couro (sem knob/botão)
- Source/img/pecas.png    knobs (48 posições), botões e LEDs acesos/apagados, faders; com a sombra no painel
- Source/Atlas.h          onde cada peça está nas duas imagens
- Source/Layout.h         as medidas do layout.py (cm) para a tela desenhar por cima
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

REN, MESA = sys.argv[1], sys.argv[2]
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import layout as L

SRC = os.path.join(MESA, "Source")
IMG = os.path.join(SRC, "img")
os.makedirs(IMG, exist_ok=True)

ch = json.load(open(os.path.join(REN, "chapas.json"), encoding="utf-8"))
pc = json.load(open(os.path.join(REN, "pecas.json"), encoding="utf-8"))
PPC = pc["ppc"]

Image.open(os.path.join(REN, "chapas.png")).convert("RGB").save(os.path.join(IMG, "chapas.jpg"), quality=90, optimize=True)

# sombra no painel: a luz principal vem de cima/atrás ⇒ a sombra cai para baixo na tela
a = np.asarray(Image.open(os.path.join(REN, "pecas.png")).convert("RGBA")).astype(np.float32)
out = a.copy()
for nome, (x, y, w, h) in pc["pecas"].items():
    if nome.startswith("k_"):    dy, blur, op = 0.16, 0.10, 0.70
    elif nome.startswith("b_"):  dy, blur, op = 0.08, 0.06, 0.60
    elif "fcap" in nome:         dy, blur, op = 0.35, 0.14, 0.75
    else:                        dy, blur, op = 0.03, 0.03, 0.50
    cell = a[y:y + h, x:x + w]
    alpha = Image.fromarray(cell[..., 3].astype(np.uint8), "L")
    sh = alpha.filter(ImageFilter.GaussianBlur(blur * PPC))
    sh = np.asarray(sh).astype(np.float32) * op
    k = int(round(dy * PPC))
    sh = np.roll(sh, k, axis=0); sh[:k] = 0
    fa = cell[..., 3] / 255.0
    sa = sh / 255.0
    oa = fa + sa * (1 - fa)
    rgb = cell[..., :3] * fa[..., None] / np.maximum(oa[..., None], 1e-6)   # sombra preta por baixo
    out[y:y + h, x:x + w, :3] = rgb
    out[y:y + h, x:x + w, 3] = oa * 255
Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), "RGBA").save(os.path.join(IMG, "pecas.png"), optimize=True)

# ------------------------------------------------------------------ Atlas.h
def entries(m):
    return "\n".join(f'    {{ "{n}", {x}, {y}, {w}, {h} }},' for n, (x, y, w, h) in m["pecas"].items())

with open(os.path.join(SRC, "Atlas.h"), "w", encoding="utf-8", newline="\n") as f:
    f.write(f"""#pragma once
// gerado por tools/recorta.py — não editar à mão. Retângulos em pixels; {int(PPC)} px = 1 cm nas duas imagens.
namespace mesa::atlas
{{
struct Peca {{ const char* nome; int x, y, w, h; }};
constexpr float ppc = {PPC}f;
constexpr int framesKnob = 48;
inline const Peca chapas[] = {{
{entries(ch)}
}};
inline const Peca pecas[] = {{
{entries(pc)}
}};
}} // namespace mesa::atlas
""")

# ------------------------------------------------------------------ Layout.h
def fl(v): return f"{float(v)}f"
def p2(t): return "{ " + ", ".join(fl(v) for v in t) + " }"
lines = []
def c(nome, v): lines.append(f"constexpr float {nome} = {fl(v)};")
for k in ("SW", "GAP", "Y_BR_TOP", "Y_BR_BOT", "MW", "WOOD_W", "EDGE", "X_CH0", "X_GP0", "X_M0", "TOTAL_W", "TOP_FRAME",
          "Y_TOP", "ARM", "Y_BOT", "TOTAL_H", "NUM_Y", "SEL_Y", "PRE_NAME_Y", "EQ_NAME_Y", "EQ_GAIN_X", "EQ_GAIN_D", "EQ_FREQ_X",
          "EQ_FREQ_D", "DYN_NAME_Y", "DYN_KNOBS_Y", "DYN_D", "GR_LED_W", "GR_LED_H", "FAD_X", "FAD_YT", "FAD_YB", "FAD_CAP_W",
          "FAD_CAP_H", "METER_W", "MFAD_X", "MFAD_YT", "MFAD_YB", "MFAD_CAP_W", "MFAD_CAP_H", "MMETER_W", "COLA_D"):
    c(k, getattr(L, k))
lines.append(f"constexpr int VIS = {L.VIS}, METER_N = {L.METER_N}, MMETER_N = {L.MMETER_N};")
for s, (a0, a1) in L.SEC.items(): lines.append(f"constexpr float SEC_{s.upper()}[2] = {{ {fl(a0)}, {fl(a1)} }};")
for s, (a0, a1) in L.MSEC.items(): lines.append(f"constexpr float MSEC_{s.upper()}[2] = {{ {fl(a0)}, {fl(a1)} }};")
v = L.VU_CH; lines.append(f"constexpr float VU_CH[4] = {{ {fl(v['x'])}, {fl(v['y'])}, {fl(v['w'])}, {fl(v['h'])} }};")
lines.append(f"constexpr float SIG[2] = {p2(L.SIG)}, PEAK[2] = {p2(L.PEAK)};")
for k in ("gain", "drive", "hpf"): lines.append(f"constexpr float PRE_{k.upper()}[3] = {p2(L.PRE_KNOBS[k])};")
lines.append(f"constexpr float PRE_PAD[3] = {p2(L.PRE_PAD)}, EQ_BTN[3] = {p2(L.EQ_BTN)}, DYNPRE_BTN[3] = {p2(L.DYNPRE_BTN)};")
lines.append(f"constexpr float EQ_ROW_Y[4] = {p2([L.eq_row_y(k) for k in range(4)])};")
lines.append(f"constexpr float DYN_X[3] = {p2(L.DYN_X)};")
lines.append(f"constexpr float GR_LEDS[5][2] = {{ {', '.join(p2(t) for t in L.GR_LEDS)} }};")
lines.append(f"constexpr float G_BTNS[3][3] = {{ {', '.join(p2(t) for t in L.G_BTNS)} }};")
lines.append(f"constexpr float PAN[3] = {p2(L.PAN)}, MUTE[3] = {p2(L.MUTE)}, SOLO[3] = {p2(L.SOLO)};")
lines.append(f"constexpr float METER_X[2] = {p2(L.METER_X)}, MMETER_X[2] = {p2(L.MMETER_X)};")
lines.append(f"constexpr float FAD_DB[2] = {p2(L.FAD_DB)}, TAPE[4] = {p2(L.TAPE)}, DOT[3] = {p2(L.DOT)}, MTAPE[4] = {p2(L.MTAPE)};")
lines.append(f"constexpr float FAD_MARKS[{len(L.FAD_MARKS)}] = {p2([m[1] for m in L.FAD_MARKS])};")
lines.append(f"constexpr float MVU[2][4] = {{ {', '.join(p2(t) for t in L.MVU)} }};")
lines.append(f"constexpr float MODE_BTNS[3][3] = {{ {', '.join(p2(t) for t in L.MODE_BTNS)} }};")
lines.append(f"constexpr float OPT_BTNS[4][3] = {{ {', '.join(p2(t) for t in L.OPT_BTNS)} }};")
lines.append(f"constexpr float COLA_VU[4] = {p2(L.COLA_VU)};")
lines.append(f"constexpr float COLA_KNOBS[6][2] = {{ {', '.join(p2(t[1:3]) for t in L.COLA_KNOBS)} }};")
lines.append(f"constexpr float COLA_IN[3] = {p2(L.COLA_IN)}, COLA_HPF[3] = {p2(L.COLA_HPF)};")
lines.append(f"constexpr float MREAD[3][2] = {{ {', '.join(p2(t) for t in L.MREAD)} }};")
with open(os.path.join(SRC, "Layout.h"), "w", encoding="utf-8", newline="\n") as f:
    f.write("#pragma once\n// gerado por tools/recorta.py a partir do tools/layout.py — não editar à mão. Medidas em cm;\n"
            "// x a partir do centro do módulo, y para cima (o topo da ponte de VUs está em Y_BR_TOP).\n"
            "namespace mesa::lay\n{\n" + "\n".join(lines) + "\n} // namespace mesa::lay\n")
print("ok", len(pc["pecas"]), "peças,", len(ch["pecas"]), "chapas")

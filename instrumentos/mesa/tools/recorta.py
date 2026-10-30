#!/usr/bin/env python3
"""Prepara o que o plugin embute a partir dos renders do render_mesa.py (0.2: vista inclinada, acabamento real).

    python recorta.py <pasta dos renders> <pasta instrumentos/mesa>

Lê todos os <nome>.json da pasta (o de chapas e os de peças, que podem vir em vários lotes) e gera:
- Source/img/chapas.jpg    módulos, ponte de VUs, madeira e couro (opacos)
- Source/img/pecas.jpg     knobs (48 posições), teclas soltas/apertadas, LEDs acesos, faders — a cor...
- Source/img/pecas_a.png   ...e a transparência (sombra de contato inclusa); o plugin junta as duas
- Source/Atlas.h           retângulo de cada célula e onde a âncora cai dentro dela
- Source/Layout.h          as medidas do layout.py (cm) + a inclinação da câmera
Também faz a "revelação" de foto (curva e cor do pos_foto.py, granulado na chapa) e o brilho em volta do que está aceso.
"""
import glob
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
PAD = 4   # folga entre células no atlas (o plugin reduz a imagem para a visão geral)

lotes = [json.load(open(j, encoding="utf-8")) for j in sorted(glob.glob(os.path.join(REN, "*.json")))]
lotes = [m for m in lotes if "img" in m]
chapas = [m for m in lotes if m["img"].startswith("chapas")]       # chapas.json + lotes que refazem células (chapas_*.json)
pecas = [m for m in lotes if not m["img"].startswith("chapas")]
assert chapas and pecas, "faltam renders"
TILT = chapas[0]["tilt"]
assert all(m["tilt"] == TILT for m in lotes)
PPC_C, PPC_P = chapas[0]["ppc"], pecas[0]["ppc"]
assert all(m["ppc"] == PPC_P for m in pecas)


def foto(rgb):
    """curva S leve e um tico mais quente (a mesma revelação do pos_foto.py, sem o que depende da posição na tela)"""
    a = rgb / 255.0
    a = a + 0.4 * (a - a ** 2) * (2 * a - 1)
    return np.clip(a * np.array([1.012, 1.0, 0.985]), 0, 1) * 255.0


def brilho(cell):
    """halo de luz em volta do LED/lente aceso: o que é claro e saturado espalha um pouco de luz na chapa"""
    rgb, a = cell[..., :3], cell[..., 3:] / 255.0
    lum = rgb.max(axis=2) / 255.0
    sat = (rgb.max(axis=2) - rgb.min(axis=2)) / np.maximum(rgb.max(axis=2), 1)
    fonte = (np.clip((lum - 0.55) / 0.45, 0, 1) * np.clip(sat * 1.6, 0, 1) * a[..., 0])
    if fonte.max() < 0.05: return cell
    r = max(2.0, 0.09 * PPC_P)
    g = np.asarray(Image.fromarray((fonte * 255).astype(np.uint8), "L").filter(ImageFilter.GaussianBlur(r))).astype(np.float32) / 255.0
    g = np.clip(g * 1.6, 0, 0.75)
    cor = np.zeros(3)
    w = fonte[..., None]
    cor = (rgb * w).sum(axis=(0, 1)) / max(w.sum(), 1e-6)
    cor = np.clip(cor / max(cor.max(), 1) * 255, 0, 255)
    na = a[..., 0] + g * (1 - a[..., 0])
    out = cell.copy()
    out[..., :3] = (rgb * a + cor * (g * (1 - a[..., 0]))[..., None]) / np.maximum(na[..., None], 1e-6)
    out[..., 3] = na * 255
    return out


def borda(h, w):
    """a ponta da sombra macia é cortada na borda da célula: some aos poucos nos últimos ~0,12 cm (sem 'caixa' em volta)"""
    f = max(3.0, 0.12 * PPC_P)
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    d = np.minimum(np.minimum(xx, w - 1 - xx), np.minimum(yy, h - 1 - yy))
    t = np.clip(d / f, 0, 1)
    return t * t * (3 - 2 * t)


def empacota(items, maxw):
    """prateleiras, as mais altas primeiro: {nome: (x, y)} e o tamanho total (px)"""
    items = sorted(items, key=lambda t: -t[2])
    pos = {}; x = y = rowh = 0
    for n, w, h in items:
        if x > 0 and x + w > maxw: x = 0; y += rowh + PAD; rowh = 0
        pos[n] = (x, y); x += w + PAD; rowh = max(rowh, h)
    W = max(pos[n][0] + w for n, w, h in items)
    return pos, W, y + rowh


# ------------------------------------------------------------------ chapas
cel = {}
for m in chapas:
    src = np.asarray(Image.open(os.path.join(REN, m["img"])).convert("RGB")).astype(np.float32)
    for n, (x, y, w, h, ax, ay) in m["pecas"].items():
        cel[n] = (src[y:y + h, x:x + w].copy(), w, h, ax, ay)
    del src
pos, W, H = empacota([(n, r[1], r[2]) for n, r in cel.items()], max(4096, max(r[1] for r in cel.values())))
out = np.zeros((H, W, 3), np.float32)
atlas_c = {}
rng = np.random.default_rng(7)
for n, (c0, w, h, ax, ay) in cel.items():
    c = foto(c0)
    c = c + rng.normal(0, 1.6, c.shape[:2])[..., None] * (0.5 + (1 - c.mean(axis=2, keepdims=True) / 255.0))   # grão de sensor, mais na sombra
    px, py = pos[n]
    out[py:py + h, px:px + w] = c
    atlas_c[n] = (px, py, w, h, ax, ay)
Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), "RGB").save(os.path.join(IMG, "chapas.jpg"), quality=88, subsampling=0, optimize=True)

# ------------------------------------------------------------------ peças
cells = {}
for m in pecas:
    im = np.asarray(Image.open(os.path.join(REN, m["img"])).convert("RGBA")).astype(np.float32)
    for n, (x, y, w, h, ax, ay) in m["pecas"].items():
        c = im[y:y + h, x:x + w].copy()
        c[..., :3] = foto(c[..., :3])
        if n.startswith("l_") or (n.startswith("b_") and n.endswith("_1")): c = brilho(c)
        c[..., 3] *= borda(h, w)
        cells[n] = (c, ax, ay)
pos, W, H = empacota([(n, c.shape[1], c.shape[0]) for n, (c, ax, ay) in cells.items()], 4096)
out = np.zeros((H, W, 4), np.float32)
atlas_p = {}
for n, (c, ax, ay) in cells.items():
    px, py = pos[n]; h, w = c.shape[:2]
    out[py:py + h, px:px + w] = c
    atlas_p[n] = (px, py, w, h, ax, ay)
rgb = out[..., :3] * (out[..., 3:] / 255.0)        # fundo preto (a sombra é preta): o JPEG não "sangra" cor
Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8), "RGB").save(os.path.join(IMG, "pecas.jpg"), quality=90, subsampling=0, optimize=True)
Image.fromarray(np.clip(out[..., 3], 0, 255).astype(np.uint8), "L").save(os.path.join(IMG, "pecas_a.png"), optimize=True)
for velho in ("pecas.png",):
    p = os.path.join(IMG, velho)
    if os.path.exists(p): os.remove(p)

# ------------------------------------------------------------------ Atlas.h
def entries(d):
    return "\n".join(f'    {{ "{n}", {x}, {y}, {w}, {h}, {ax}f, {ay}f }},' for n, (x, y, w, h, ax, ay) in sorted(d.items()))

frames = 1 + max(int(n.rsplit("_", 1)[1]) for n in atlas_p if n.startswith("k_ssl_red_"))
with open(os.path.join(SRC, "Atlas.h"), "w", encoding="utf-8", newline="\n") as f:
    f.write(f"""#pragma once
// gerado por tools/recorta.py — não editar à mão. Retângulos em pixels; (ax, ay) = onde a âncora da célula cai
// dentro do retângulo. Chapas a {PPC_C:g} px/cm, peças a {PPC_P:g} px/cm.
namespace mesa::atlas
{{
struct Peca {{ const char* nome; int x, y, w, h; float ax, ay; }};
constexpr float ppcChapas = {PPC_C}f, ppcPecas = {PPC_P}f;
constexpr int framesKnob = {frames};
inline const Peca chapas[] = {{
{entries(atlas_c)}
}};
inline const Peca pecas[] = {{
{entries(atlas_p)}
}};
}} // namespace mesa::atlas
""")

# ------------------------------------------------------------------ Layout.h
import math
def fl(v): return f"{float(v)}f"
def p2(t): return "{ " + ", ".join(fl(v) for v in t) + " }"
lines = []
def c(nome, v): lines.append(f"constexpr float {nome} = {fl(v)};")
for k in ("SW", "GAP", "SURF", "Y_BR_TOP", "Y_BR_BOT", "MW", "WOOD_W", "EDGE", "X_CH0", "X_GP0", "X_M0", "TOTAL_W", "TOP_FRAME",
          "Y_TOP", "ARM", "Y_BOT", "TOTAL_H", "NUM_Y", "SEL_Y", "PRE_NAME_Y", "EQ_NAME_Y", "EQ_GAIN_X", "EQ_GAIN_D", "EQ_FREQ_X",
          "EQ_FREQ_D", "DYN_NAME_Y", "DYN_KNOBS_Y", "DYN_D", "GR_LED_W", "GR_LED_H", "FAD_X", "FAD_YT", "FAD_YB", "FAD_CAP_W",
          "FAD_CAP_H", "METER_W", "MFAD_X", "MFAD_YT", "MFAD_YB", "MFAD_CAP_W", "MFAD_CAP_H", "MMETER_W", "COLA_D"):
    c(k, getattr(L, k))
lines.append("// câmera inclinada: tela (cm, para cima) = y·TILT_COS + (z − SURF)·TILT_SIN")
c("TILT_COS", math.cos(math.radians(TILT))); c("TILT_SIN", math.sin(math.radians(TILT)))
c("BR_Z", 1.4); c("FRAME_Z", 3.2); c("FLOOR_Z", -1.0); c("VU_FACE_DZ", 0.36)
c("VU_R", 0.78); c("VU_ANG", 38.0 / 47.0)   # escala do VU: raio (× altura do mostrador) e arco (× tabela)
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
lines.append(f"constexpr float FAD_MARKS[{len(L.FAD_MARKS)}] = {p2([m_[1] for m_ in L.FAD_MARKS])};")
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
print("ok", len(atlas_p), "peças,", len(atlas_c), "chapas; chapas.jpg", W, H)

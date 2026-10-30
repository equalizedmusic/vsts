#!/usr/bin/env python3
"""Monta um trecho da console com as células renderizadas, do mesmo jeito que o plugin desenha (para conferir o
encaixe chapa/peça antes de compilar).

    python compoe.py <pasta dos renders> <saida.png> [ppc da saída] [canais]

Lê todos os <nome>.json da pasta (chapas e peças). Coordenadas como no PluginEditor: x em cm a partir da borda
esquerda; y da tela = TOPO − (y·cos T + (za − SURF)·sin T).
"""
import glob
import json
import math
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import layout as L

REN, OUT = sys.argv[1], sys.argv[2]
P = float(sys.argv[3]) if len(sys.argv) > 3 else 30.0
NCH = int(sys.argv[4]) if len(sys.argv) > 4 else 5

cells, imgs = {}, {}
for j in glob.glob(os.path.join(REN, "*.json")):
    m = json.load(open(j, encoding="utf-8"))
    if "img" not in m: continue
    im = Image.open(os.path.join(REN, m["img"])).convert("RGBA")
    for n, r in m["pecas"].items():
        cells[n] = (im, m["ppc"], r)
    T = math.radians(m["tilt"])
CT, ST = math.cos(T), math.sin(T)
SURF, BR_Z, FRAME_Z, FLOOR_Z = L.SURF, 1.4, 3.2, -1.0
TOP = L.Y_TOP * CT + (FRAME_Z - SURF) * ST
BOT = (L.Y_BOT - 0.2) * CT + (FLOOR_Z - SURF) * ST
W = L.WOOD_W * 2 + L.EDGE * 2 + NCH * L.SW
H = TOP - BOT
canvas = Image.new("RGBA", (round(W * P), round(H * P)), (11, 11, 12, 255))

def sy(y, za=SURF):
    return TOP - (y * CT + (za - SURF) * ST)

def draw(name, x, y, za=SURF, s=1.0, sx=None):
    im, ppc, (cx, cy, cw, ch, ax, ay) = cells[name]
    k = P / ppc * s
    kx = k if sx is None else sx * P / cw
    w, h = max(1, round(cw * kx)), max(1, round(ch * k))
    tile = im.crop((cx, cy, cx + cw, cy + ch)).resize((w, h), Image.LANCZOS)
    px, py = x * P - ax * kx, sy(y, za) * P - ay * k
    canvas.alpha_composite(tile, (round(px), round(py))) if px >= 0 and py >= 0 else canvas.paste(tile, (round(px), round(py)), tile)

CH = [("BRIT 73", "BRIT 81", 0.72, (0.62, 0.35, 0.2), [(0.55, 0.4), (0.45, 0.6), (0.38, 0.3), (0.66, 0.25)], 0.24, 1),
      ("LIMPO", "E-4K", 0.58, (0.55, 0.2, 0.45), [(0.62, 0.7), (0.58, 0.45), (0.42, 0.55), (0.5, 0.5)], 0.3, 1),
      ("API 312", "E-4K", 0.5, (0.5, 0.6, 0.3), [(0.5, 0.5), (0.4, 0.35), (0.6, 0.2), (0.57, 0.3)], 0.27, 2),
      ("BRIT 73", "BRIT 81", 0.64, (0.7, 0.45, 0.4), [(0.6, 0.8), (0.55, 0.6), (0.4, 0.35), (0.45, 0.3)], 0.2, 0),
      ("LIMPO", "E-4K", 0.3, (0.45, 0.3, 0.5), [(0.5, 0.5)] * 4, 0.34, 3)]

def fr(v): return f"{min(47, max(0, round(v * 47))):02d}"

# fundo: madeira de cima, ponte/chapas, couro, laterais (de trás para a frente)
draw("top", W / 2, L.Y_BR_TOP, sx=W)
draw("div", L.WOOD_W + L.EDGE / 2, L.Y_BR_TOP, sx=L.EDGE)
draw("div", W - L.WOOD_W - L.EDGE / 2, L.Y_BR_TOP, sx=L.EDGE)
for i in range(NCH):
    pm, em, vu, pre, eq, fpos, g = CH[i % len(CH)]
    cx = L.WOOD_W + L.EDGE + L.SW * (i + 0.5)
    draw("ch_bridge", cx, L.Y_BR_TOP)
    for nm, sec in (("pre_" + pm, "pre"), ("eq_" + em, "eq"), ("dyn_VCA", "dyn"), ("out", "out"), ("fad", "fad")):
        draw(nm, cx, L.SEC[sec][0])
    fam = L.PRE_FAM[pm]
    for (k, (x, y, d)), v in zip(L.PRE_KNOBS.items(), pre):
        spr = "k_api" if fam == "api" else ({"gain": "k_mar_gain", "drive": "k_mar_org", "hpf": "k_mar"}[k] if fam == "neve"
                                          else {"gain": "k_ssl_gry", "drive": "k_ssl_org", "hpf": "k_ssl_blk"}[k])
        draw(f"{spr}_{fr(v)}", cx + x, y, s=d / 1.5)
    draw("b_pad_0", cx + L.PRE_PAD[0], L.PRE_PAD[1])
    caps = ["red", "grn", "blu", "brn"]
    for b, (gv, fv) in enumerate(eq):
        spr = ("k_mar_" if L.EQ_FAM[em] == "neve" else "k_ssl_") + caps[b]
        draw(f"{spr}_{fr(gv)}", cx + L.EQ_GAIN_X, L.eq_row_y(b), s=L.EQ_GAIN_D / 1.5)
        draw(f"{spr}_{fr(fv)}", cx + L.EQ_FREQ_X, L.eq_row_y(b), s=L.EQ_FREQ_D / 1.5)
    draw("b_eq_1", cx + L.EQ_BTN[0], L.EQ_BTN[1]); draw("b_pre_%d" % (i == 1), cx + L.DYNPRE_BTN[0], L.DYNPRE_BTN[1])
    for x, c in zip(L.DYN_X, ("gry", "gry", "grn")): draw(f"k_ssl_{c}_{fr(0.45)}", cx + x, L.DYN_KNOBS_Y, s=L.DYN_D / 1.5)
    for k in range(3): draw(f"b_g{k + 1}_{int(g == k + 1)}", cx + L.G_BTNS[k][0], L.G_BTNS[k][1])
    draw(f"k_ssl_wht_{fr(0.5)}", cx + L.PAN[0], L.PAN[1], s=L.PAN[2] / 1.5)
    draw("b_m_%d" % (i == 4), cx + L.MUTE[0], L.MUTE[1]); draw("b_s_0", cx + L.SOLO[0], L.SOLO[1])
    step = (L.FAD_YT - L.FAD_YB) / (L.METER_N - 1)
    for c, mx in enumerate(L.METER_X):
        for j in range(int(L.METER_N * (vu - 0.05 * c))):
            f = j / (L.METER_N - 1); col = "g" if f < 0.68 else ("y" if f < 0.86 else "r")
            draw("l_m_" + col, cx + mx, L.FAD_YB + j * step)
    for j in range(i % 4): draw("l_gr_" + ("a" if j < 4 else "r"), cx + L.GR_LEDS[j][0], L.GR_LEDS[j][1])
    draw("l_d_g", cx + L.SIG[0], L.SIG[1], za=BR_Z)
    if i == 0: draw("l_d_r", cx + L.PEAK[0], L.PEAK[1], za=BR_Z)
    draw("fcap", cx + L.FAD_X, L.FAD_YT - fpos * (L.FAD_YT - L.FAD_YB))
draw("leather", W / 2, L.SEC["fad"][1], sx=W - 2 * L.WOOD_W)
draw("wood", L.WOOD_W / 2, L.Y_TOP)
draw("wood", W - L.WOOD_W / 2, L.Y_TOP)
canvas.convert("RGB").save(OUT)
print("ok", OUT, canvas.size)

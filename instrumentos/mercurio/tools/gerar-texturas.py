#!/usr/bin/env python3
"""Gera as texturas do MERCÚRIO (Source/img): knob de alumínio, tecla, parafuso e a chapa de alumínio escovado.

Uso:
    python gerar-texturas.py

Nada é foto: cada peça é um relevo (mapa de altura) iluminado pixel a pixel — luz principal de cima-esquerda,
uma luz de preenchimento fraca, reflexo especular do material e sombra macia no painel — e renderizado com
supersampling para a borda sair lisa. Rodar de novo dá exatamente os mesmos arquivos (sementes fixas).
"""
from pathlib import Path

import numpy as np
from PIL import Image

AQUI = Path(__file__).resolve().parent
DESTINO = AQUI.parent / "Source" / "img"

L_KEY = np.array([-0.45, -0.62, 0.64])     # luz principal (x para a direita, y para baixo, z para fora da tela)
L_FILL = np.array([0.55, 0.15, 0.82])
VIEW = np.array([0.0, 0.0, 1.0])


def unit(v):
    return v / np.linalg.norm(v)


L_KEY, L_FILL = unit(L_KEY), unit(L_FILL)


def blur(a, sigma):
    """Desfoque gaussiano aproximado (três caixas), separável, bordas presas."""
    if sigma <= 0:
        return a
    r = max(1, int(round(sigma * 0.9)))
    out = a.astype(np.float64)
    for axis in (0, 1):
        for _ in range(3):
            pad = [(0, 0), (0, 0)]
            pad[axis] = (r + 1, r)
            p = np.pad(out, pad, mode="edge")
            c = np.cumsum(p, axis=axis)
            if axis == 0:
                out = (c[2 * r + 1:, :] - c[:-2 * r - 1, :]) / (2 * r + 1)
            else:
                out = (c[:, 2 * r + 1:] - c[:, :-2 * r - 1]) / (2 * r + 1)
    return out


def normais(h, escala):
    """Normais de um mapa de altura (altura em px * escala)."""
    gy, gx = np.gradient(h * escala)
    n = np.dstack([-gx, -gy, np.ones_like(h)])
    return n / np.linalg.norm(n, axis=2, keepdims=True)


def para_rgba(rgb, alpha, ss):
    """Reduz o supersampling (média em blocos ss×ss) com alfa pré-multiplicado e devolve uma imagem PIL."""
    H, W = alpha.shape
    h, w = H // ss, W // ss
    pm = rgb[: h * ss, : w * ss] * alpha[: h * ss, : w * ss, None]
    pm = pm.reshape(h, ss, w, ss, 3).mean((1, 3))
    a = alpha[: h * ss, : w * ss].reshape(h, ss, w, ss).mean((1, 3))
    c = np.where(a[..., None] > 1e-5, pm / np.maximum(a[..., None], 1e-5), 0)
    srgb = np.clip(c, 0, 1) ** (1 / 2.2)
    out = np.dstack([srgb * 255, a * 255]).round().clip(0, 255).astype(np.uint8)
    return Image.fromarray(out, "RGBA")


def sombra(mask, ss, dx, dy, sigma, forca):
    """Sombra macia de uma máscara (deslocada e desfocada), em alfa 0..1."""
    m = np.roll(np.roll(mask.astype(np.float64), int(dy * ss), 0), int(dx * ss), 1)
    return np.clip(blur(m, sigma * ss) * forca, 0, 1)


def luz(n, albedo, spec, shin, amb=0.10, fill=0.25, sheen=0.0, f0=0.05, box=6.0, rough=0.10):
    """Lambert + Blinn + reflexo de um softbox em cima-esquerda (o que dá forma a objeto escuro) com Fresnel.
    albedo: H×W×3 (linear)."""
    d1 = np.clip(np.einsum("ijk,k->ij", n, L_KEY), 0, 1)
    d2 = np.clip(np.einsum("ijk,k->ij", n, L_FILL), 0, 1)
    s1 = np.clip(np.einsum("ijk,k->ij", n, unit(L_KEY + VIEW)), 0, 1) ** shin
    nz = np.clip(n[..., 2], 0, 1)
    refl = 2 * nz[..., None] * n - VIEW
    cosb = np.einsum("ijk,k->ij", refl, L_KEY)
    soft = np.clip((cosb - (1 - rough * 2.2)) / (rough * 1.6), 0, 1) ** 2
    sky = np.clip(-refl[..., 1] * 0.5 + 0.5, 0, 1) * np.clip(refl[..., 2], 0, 1)
    fres = f0 + (1 - f0) * (1 - nz) ** 5
    lum = albedo * (amb + d1 * 1.25 + d2 * fill)[..., None]
    return lum + (spec * s1 + fres * (box * soft + 0.10 * sky))[..., None] + (sheen * (1 - nz) ** 2 * (0.3 + d1))[..., None]


def knob(N=288, ss=3):
    """Knob de alumínio usinado: anel de cima torneado (brilho em leque que segue a luz), borda serrilhada,
    chanfro polido e tampa de acrílico preto brilhante no meio (onde o ponteiro é pintado); sombra no painel."""
    S = N * ss
    R = 0.36 * S
    y, x = np.mgrid[0:S, 0:S].astype(np.float64)
    cx = cy = S / 2.0
    dx, dy = x - cx, y - cy
    r = np.sqrt(dx * dx + dy * dy) / R
    th = np.arctan2(dy, dx)

    cap_r, ch_r, knurl_r = 0.62, 0.68, 0.90
    knurl = (r <= 1.0) & (r > knurl_r)
    ring = (r <= knurl_r) & (r > ch_r)
    cham = (r <= ch_r) & (r > cap_r)
    cap = r <= cap_r
    h = np.zeros_like(r)
    # borda serrilhada: desce arredondando até o painel, com os dentes
    t = np.clip((r - knurl_r) / (1.0 - knurl_r), 0, 1)
    h = np.where(knurl, 0.30 * (1 - t ** 2.2) + 0.012 * np.cos(72 * th) * np.sin(np.pi * t), h)
    h = np.where(ring, 0.30 + 0.004 * (knurl_r - r), h)
    tc = (r - cap_r) / (ch_r - cap_r)
    h = np.where(cham, 0.30 - 0.035 * (1 - tc), h)                  # chanfro para dentro
    h = np.where(cap, 0.265 - 0.010 * (1 - (r / cap_r) ** 2), h)     # tampa levemente côncava

    n = normais(h * R, 1.0)
    inside = r <= 1.0
    rng = np.random.default_rng(7)

    # alumínio: riscos concêntricos do torno e o leque de brilho perpendicular à luz
    lathe = blur(rng.standard_normal((S, S)), 0.6 * ss)
    spun = np.cos(r * 520.0 + lathe * 0.8) * 0.5 + 0.5
    lt = np.arctan2(L_KEY[1], L_KEY[0])
    fan = (0.5 + 0.5 * np.cos(2 * (th - lt - np.pi / 2))) ** 6
    alb = np.ones((S, S, 3)) * np.array([0.20, 0.21, 0.23])
    alb = alb * (0.80 + 0.20 * spun)[..., None]
    rgb_alu = luz(n, alb, 0.05, 30, amb=0.30, fill=0.30, f0=0.04, box=0.0, rough=0.28)
    rgb_alu = rgb_alu + (fan * (0.22 + 0.16 * spun))[..., None] * np.array([1.0, 1.0, 1.04])
    # serrilhado: mais escuro (a luz entra menos entre os dentes)
    rgb_kn = luz(n, alb * 0.9, 0.25, 20, amb=0.20, fill=0.3, f0=0.10, box=0.6, rough=0.22)
    # chanfro polido: espelho (céu claro em cima, chão escuro embaixo)
    nz = np.clip(n[..., 2], 0, 1)
    refl_y = 2 * nz * n[..., 1]
    rgb_ch = np.clip(0.35 - refl_y * 1.6, 0.02, 0.9)[..., None] * np.array([0.92, 0.95, 1.0])
    # tampa: acrílico preto brilhante
    cap_alb = np.ones((S, S, 3)) * np.array([0.008, 0.008, 0.010])
    rgb_cap = luz(n, cap_alb, 0.6, 80, amb=0.10, fill=0.2, f0=0.04, box=3.0, rough=0.10)
    env = np.exp(-(((dx / R + 0.20) ** 2) / 0.06 + ((dy / R + 0.30) ** 2) / 0.012)) * 0.05
    rgb_cap = rgb_cap + env[..., None]

    rgb = np.where(cap[..., None], rgb_cap, np.where(cham[..., None], rgb_ch, np.where(ring[..., None], rgb_alu, rgb_kn)))
    ao = 1.0 - 0.35 * np.exp(-(1.0 - np.clip(r, 0, 1)) / 0.03)
    rgb = rgb * ao[..., None]

    mask = inside.astype(np.float64)
    sh1 = sombra(mask, ss, 0.06 * R / ss, 0.12 * R / ss, 0.10 * R / ss, 0.70)
    sh2 = sombra(mask, ss, 0.0, 0.02 * R / ss, 0.03 * R / ss, 0.60)
    shadow = 1 - (1 - sh1) * (1 - sh2)
    a = mask + shadow * (1 - mask)
    col = np.where(a[..., None] > 0, (rgb * mask[..., None]) / np.maximum(a[..., None], 1e-6), 0)
    return para_rgba(col, a, ss)


def rrect_sdf(x, y, w, h, rad):
    """Distância assinada de um retângulo arredondado centrado na origem (negativa dentro)."""
    qx = np.abs(x) - (w / 2 - rad)
    qy = np.abs(y) - (h / 2 - rad)
    return np.sqrt(np.maximum(qx, 0) ** 2 + np.maximum(qy, 0) ** 2) + np.minimum(np.maximum(qx, qy), 0) - rad


def tecla(W=180, H=96, ss=3):
    """Tecla preta acetinada: topo quase plano, borda arredondada, sombra embaixo."""
    SW, SH = W * ss, H * ss
    y, x = np.mgrid[0:SH, 0:SW].astype(np.float64)
    kw, kh = 0.78 * SW, 0.62 * SH
    cx, cy = SW / 2, SH * 0.44
    d = rrect_sdf(x - cx, y - cy, kw, kh, 0.16 * kh)          # < 0 dentro
    bevel = 0.20 * kh
    t = np.clip(-d / bevel, 0, 1)
    h = (1 - (1 - t) ** 2) * bevel * 0.9                      # borda arredondada
    dome = 1 - (((x - cx) / (kw / 2)) ** 2 * 0.5 + ((y - cy) / (kh / 2)) ** 2 * 0.5)
    h = h + np.where(d < 0, dome * 0.03 * kh, 0)
    rng = np.random.default_rng(11)
    h = h + np.where(d < 0, blur(rng.standard_normal((SH, SW)), 1.0 * ss) * 0.08, 0)
    n = normais(h, 1.0)
    inside = (d < 0).astype(np.float64)
    alb = np.zeros((SH, SW, 3))
    alb[:] = np.array([0.022, 0.022, 0.025])
    ao = 1 - 0.35 * np.exp(-np.clip(-d, 0, None) / (0.05 * kh))
    alb *= ao[..., None]
    rgb = luz(n, alb, 0.10, 20, amb=0.16, fill=0.25, sheen=0.06, f0=0.05, box=2.0, rough=0.22)
    sh1 = sombra(inside, ss, 0.0, 0.10 * kh / ss, 0.10 * kh / ss, 0.80)
    sh2 = sombra(inside, ss, 0.0, 0.03 * kh / ss, 0.03 * kh / ss, 0.70)
    shadow = 1 - (1 - sh1) * (1 - sh2)
    a = inside + shadow * (1 - inside)
    col = np.where(a[..., None] > 0, (rgb * inside[..., None]) / np.maximum(a[..., None], 1e-6), 0)
    return para_rgba(col, a, ss)


def parafuso(N=72, ss=4):
    """Parafuso de cabeça abaulada em aço inox, com sextavado interno (Allen), e o furo escuro em volta."""
    S = N * ss
    y, x = np.mgrid[0:S, 0:S].astype(np.float64)
    c = S / 2
    R = 0.36 * S
    dx, dy = x - c, y - c
    r = np.sqrt(dx * dx + dy * dy) / R
    h = np.where(r < 1, np.sqrt(np.clip(1 - r ** 2, 0, 1)) * 0.30 * R, 0)
    ang = np.arctan2(dy, dx)
    hexd = np.max(np.stack([np.abs(dx * np.cos(a) + dy * np.sin(a)) for a in (0, np.pi / 3, 2 * np.pi / 3)]), axis=0) / R
    hole = hexd < 0.36
    h = np.where(hole, h - 0.20 * R, h)
    n = normais(h, 1.0)
    inside = (r < 1).astype(np.float64)
    rng = np.random.default_rng(3)
    brush = blur(rng.standard_normal((S, S)), 0.7 * ss)
    alb = np.zeros((S, S, 3))
    alb[:] = np.array([0.42, 0.43, 0.45])
    alb *= (1 + 0.08 * np.sin(ang * 2 + 0.4) + 0.05 * brush)[..., None]
    alb = np.where(hole[..., None], alb * 0.05, alb)
    rgb = luz(n, alb, 0.5, 40, amb=0.25, fill=0.3, f0=0.5, box=1.2, rough=0.22)
    ring = np.clip((1.12 - r) / 0.06, 0, 1) * (r >= 1)
    sh = sombra(inside, ss, 0.0, 0.05 * R / ss, 0.08 * R / ss, 0.8)
    a = inside + np.maximum(ring * 0.85, sh) * (1 - inside)
    col = np.where(a[..., None] > 0, (rgb * inside[..., None]) / np.maximum(a[..., None], 1e-6), 0)
    return para_rgba(col, a, ss)


def hblur(a, rad):
    """Desfoque só na horizontal (três caixas), com a borda dando a volta."""
    out = a
    for _ in range(3):
        p = np.pad(out, [(0, 0), (rad + 1, rad)], mode="wrap")
        cs = np.cumsum(p, axis=1)
        out = (cs[:, 2 * rad + 1:] - cs[:, :-2 * rad - 1]) / (2 * rad + 1)
    return out


def chapa(W=2880, H=1920, seed=5):
    """Chapa do painel: alumínio escovado na horizontal (riscos finos e compridos), anodizado natural de tom frio,
    mais claro em cima-esquerda e escurecendo para as bordas. Sem os detalhes (serigrafia, telas) — esses são
    desenhados por cima."""
    rng = np.random.default_rng(seed)
    brush = hblur(rng.standard_normal((H, W)), 60) * 6.0 + hblur(rng.standard_normal((H, W)), 220) * 9.0
    brush = brush / np.std(brush)
    mancha = blur(rng.standard_normal((H // 8, W // 8)), 6.0)
    mancha = np.kron(mancha / np.std(mancha), np.ones((8, 8)))[:H, :W]
    alb = np.ones((H, W, 3)) * np.array([0.36, 0.375, 0.40])
    alb *= (1 + 0.045 * brush + 0.02 * mancha)[..., None]
    y, x = np.mgrid[0:H, 0:W].astype(np.float64)
    u, v = x / W, y / H
    grad = 1.10 - 0.16 * v - 0.05 * u + 0.10 * np.exp(-((u - 0.30) ** 2 / 0.10 + (v - 0.10) ** 2 / 0.06))
    vig = 1 - 0.22 * (np.clip(np.abs(u - 0.5) * 2, 0, 1) ** 4 + np.clip(np.abs(v - 0.5) * 2, 0, 1) ** 4)
    # escovado na horizontal estica o reflexo na vertical: faixas largas e suaves
    sheen = 0.06 * np.exp(-((u - 0.62) ** 2) / 0.02) + 0.04 * np.exp(-((u - 0.18) ** 2) / 0.01)
    rgb = alb * (grad * vig)[..., None] + (sheen * (1 + 0.05 * brush))[..., None]
    srgb = np.clip(rgb, 0, 1) ** (1 / 2.2)
    return Image.fromarray((srgb * 255).round().clip(0, 255).astype(np.uint8), "RGB")


def main():
    DESTINO.mkdir(parents=True, exist_ok=True)
    knob().save(DESTINO / "knob.png", optimize=True)
    tecla().save(DESTINO / "key.png", optimize=True)
    parafuso().save(DESTINO / "screw.png", optimize=True)
    chapa().save(DESTINO / "panel.jpg", quality=86, optimize=True, subsampling=0)   # 2× da tela base (1440×960)
    for f in sorted(DESTINO.iterdir()):
        print(f"{f.name}: {f.stat().st_size // 1024} KB")


if __name__ == "__main__":
    main()

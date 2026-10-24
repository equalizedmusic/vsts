#!/usr/bin/env python3
"""Gera as texturas do VACINA (Source/img): knob, tecla, parafuso e a chapa do painel.

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
    """Knob preto de borracha macia: saia cônica serrilhada, tampa levemente abaulada, sombra no painel."""
    S = N * ss
    R = 0.36 * S
    y, x = np.mgrid[0:S, 0:S].astype(np.float64)
    cx = cy = S / 2.0
    dx, dy = x - cx, y - cy
    r = np.sqrt(dx * dx + dy * dy) / R
    th = np.arctan2(dy, dx)

    top_r, sh_r = 0.78, 0.82
    hTop = 0.30
    h = np.zeros_like(r)
    skirt = (r <= 1.0) & (r > sh_r)
    h = np.where(skirt, (1.0 - r) / (1.0 - sh_r) * hTop, h)
    # ombro arredondado entre a saia e a tampa
    sh = (r <= sh_r) & (r > top_r)
    t = (r - top_r) / (sh_r - top_r)
    h = np.where(sh, hTop + 0.012 * (1 - t) ** 2 - 0.004 * t, h)
    top = r <= top_r
    h = np.where(top, hTop + 0.012 + 0.035 * (1 - (r / top_r) ** 2), h)
    # serrilhado da saia (some perto da tampa e da base)
    win = np.clip((r - sh_r) / 0.03, 0, 1) * np.clip((1.0 - r) / 0.03, 0, 1)
    h = h + np.where(skirt, 0.010 * np.cos(44 * th) * win, 0)
    # micro textura (borracha)
    rng = np.random.default_rng(7)
    micro = blur(rng.standard_normal((S, S)), 1.2 * ss) * 0.0009
    h = h + np.where(r <= 1.0, micro, 0)

    n = normais(h * R, 1.0)
    inside = r <= 1.0
    alb = np.zeros((S, S, 3))
    base = np.array([0.011, 0.011, 0.012])
    alb[:] = base
    alb[top] = np.array([0.013, 0.013, 0.014])
    # oclusão perto da base da saia
    ao = 1.0 - 0.45 * np.exp(-(1.0 - np.clip(r, 0, 1)) / 0.035)
    alb = alb * ao[..., None]

    rgb_skirt = luz(n, alb, 0.03, 10, amb=0.10, fill=0.25, sheen=0.04, f0=0.05, box=2.2, rough=0.22)
    rgb_top = luz(n, alb, 0.02, 30, amb=0.10, fill=0.25, sheen=0.02, f0=0.05, box=1.6, rough=0.16)
    rgb = np.where(top[..., None], rgb_top, rgb_skirt)
    # reflexo do ambiente na tampa: faixa clara suave em cima-esquerda
    env = np.exp(-(((dx / R + 0.25) ** 2) / 0.10 + ((dy / R + 0.42) ** 2) / 0.02)) * 0.004
    rgb = rgb + np.where(top, env, 0)[..., None]

    mask = inside.astype(np.float64)
    sh1 = sombra(mask, ss, 0.05 * R / ss, 0.10 * R / ss, 0.10 * R / ss, 0.78)
    sh2 = sombra(mask, ss, 0.0, 0.02 * R / ss, 0.03 * R / ss, 0.65)
    shadow = 1 - (1 - sh1) * (1 - sh2)

    # composição: knob por cima da sombra (sombra preta)
    a_k = mask
    a = a_k + shadow * (1 - a_k)
    col = np.where(a[..., None] > 0, (rgb * a_k[..., None]) / np.maximum(a[..., None], 1e-6), 0)
    return para_rgba(col, a, ss)


def rrect_sdf(x, y, w, h, rad):
    """Distância assinada de um retângulo arredondado centrado na origem (negativa dentro)."""
    qx = np.abs(x) - (w / 2 - rad)
    qy = np.abs(y) - (h / 2 - rad)
    return np.sqrt(np.maximum(qx, 0) ** 2 + np.maximum(qy, 0) ** 2) + np.minimum(np.maximum(qx, qy), 0) - rad


def tecla(W=180, H=96, ss=3):
    """Tecla de borracha cinza (como as do painel): topo quase plano, borda arredondada, sombra embaixo."""
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
    alb[:] = np.array([0.27, 0.27, 0.26])
    ao = 1 - 0.35 * np.exp(-np.clip(-d, 0, None) / (0.05 * kh))
    alb *= ao[..., None]
    rgb = luz(n, alb, 0.05, 12, amb=0.16, fill=0.25, sheen=0.05, f0=0.04, box=0.7, rough=0.30)
    sh1 = sombra(inside, ss, 0.0, 0.10 * kh / ss, 0.10 * kh / ss, 0.80)
    sh2 = sombra(inside, ss, 0.0, 0.03 * kh / ss, 0.03 * kh / ss, 0.70)
    shadow = 1 - (1 - sh1) * (1 - sh2)
    a = inside + shadow * (1 - inside)
    col = np.where(a[..., None] > 0, (rgb * inside[..., None]) / np.maximum(a[..., None], 1e-6), 0)
    return para_rgba(col, a, ss)


def parafuso(N=72, ss=4):
    """Parafuso Phillips de cabeça panela, aço escovado, com a sombra do furo."""
    S = N * ss
    y, x = np.mgrid[0:S, 0:S].astype(np.float64)
    c = S / 2
    R = 0.36 * S
    dx, dy = x - c, y - c
    r = np.sqrt(dx * dx + dy * dy) / R
    h = np.where(r < 1, np.sqrt(np.clip(1 - r ** 2, 0, 1)) * 0.35 * R, 0)
    # fenda em cruz
    w = 0.09 * R
    cruz = (np.abs(dx) < w) & (np.abs(dy) < 0.55 * R) | (np.abs(dy) < w) & (np.abs(dx) < 0.55 * R)
    h = np.where(cruz & (r < 1), h - 0.22 * R * np.clip(1 - np.maximum(np.abs(dx), np.abs(dy)) / (0.55 * R), 0, 1) ** 0.3, h)
    n = normais(h, 1.0)
    inside = (r < 1).astype(np.float64)
    ang = np.arctan2(dy, dx)
    rng = np.random.default_rng(3)
    brush = blur(rng.standard_normal((S, S)), 0.7 * ss)
    alb = np.zeros((S, S, 3))
    alb[:] = np.array([0.075, 0.075, 0.08])
    alb *= (1 + 0.10 * np.sin(ang * 3 + 0.4) + 0.06 * brush)[..., None]
    alb = np.where(cruz[..., None], alb * 0.25, alb)
    rgb = luz(n, alb, 0.25, 40, amb=0.12, fill=0.3, f0=0.25, box=0.9, rough=0.25)
    # furo escuro em volta (rebaixo do painel)
    ring = np.clip((1.12 - r) / 0.06, 0, 1) * (r >= 1)
    sh = sombra(inside, ss, 0.0, 0.05 * R / ss, 0.08 * R / ss, 0.8)
    a = inside + np.maximum(ring * 0.85, sh) * (1 - inside)
    col = np.where(a[..., None] > 0, (rgb * inside[..., None]) / np.maximum(a[..., None], 1e-6), 0)
    return para_rgba(col, a, ss)


def chapa(W=2880, H=1920, seed=5):
    """Chapa do painel: pintura eletrostática grafite (casca de laranja fina iluminada), mais clara em
    cima-esquerda e escurecendo para as bordas. Sem os detalhes (serigrafia, telas) — esses são desenhados por cima."""
    rng = np.random.default_rng(seed)
    peel = blur(rng.standard_normal((H, W)), 2.2) * 1.0 + blur(rng.standard_normal((H, W)), 6.0) * 1.2
    n = normais(peel, 0.9)
    base = np.array([0.0215, 0.0222, 0.0245])
    alb = np.ones((H, W, 3)) * base
    grain = rng.standard_normal((H, W)) * 0.035 + blur(rng.standard_normal((H, W)), 30.0) * 0.6
    alb *= (1 + grain)[..., None]
    rgb = luz(n, alb, 0.0, 10, amb=0.55, fill=0.3, f0=0.03, box=0.08, rough=0.45)
    y, x = np.mgrid[0:H, 0:W].astype(np.float64)
    u, v = x / W, y / H
    grad = 1.10 - 0.18 * v - 0.06 * u + 0.08 * np.exp(-((u - 0.25) ** 2 / 0.08 + (v - 0.05) ** 2 / 0.05))
    vig = 1 - 0.25 * (np.clip(np.abs(u - 0.5) * 2, 0, 1) ** 4 + np.clip(np.abs(v - 0.5) * 2, 0, 1) ** 4)
    rgb = rgb * (grad * vig)[..., None]
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

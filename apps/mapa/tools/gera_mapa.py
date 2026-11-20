"""MAPA 0.3 — música sintética com GABARITO por elemento, para medir a precisão do mapa (onde cada coisa entra e sai).
uso: python gera_mapa.py <saida.wav> <gabarito.json> [bpm=140] [silencio_inicial_em_tempos=0]
Elementos: KICK, BAIXO, CLAP, HATS, PAD, LEAD, VOZ (sintetizada por formantes), SWEEP UP, SWEEP DOWN, IMPACTO.
Gabarito: {"bpm":..,"audioInicioTempos":..,"elementos":{"KICK":[[ini,fim],...] (compassos, 0 = compasso 1, fim exclusivo)}}
"""
import json
import sys
import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, sosfilt, lfilter

out, gab = sys.argv[1], sys.argv[2]
sys.argv = [a for a in sys.argv if a != "--sem-voz"] + (["--sem-voz"] if "--sem-voz" in sys.argv else [])
bpm = float(sys.argv[3]) if len(sys.argv) > 3 and sys.argv[3] != "--sem-voz" else 140.0
sil = float(sys.argv[4]) if len(sys.argv) > 4 and sys.argv[4] != "--sem-voz" else 0.0
SR = 44100
P = 60.0 / bpm
TOTAL = 128
t0 = sil * P
N = int((t0 + TOTAL * 4 * P + 3) * SR)
mix = np.zeros(N)
rng = np.random.default_rng(7)
TON = 9  # A menor
esc = [0, 2, 3, 5, 7, 8, 10]

# arranjo (compassos, fim exclusivo)
G = {
    "KICK": [[4, 28], [32, 64], [80, 124]],
    "BAIXO": [[16, 28], [32, 64], [80, 112]],
    "CLAP": [[20, 28], [32, 64], [80, 112]],
    "HATS": [[8, 32], [32, 64], [80, 120]],
    "PAD": [[0, 16], [64, 80], [116, 128]],
    "LEAD": [[32, 64], [72, 80], [88, 112]],
    "VOZ": [] if "--sem-voz" in sys.argv else [[16, 32], [40, 56], [68, 80], [96, 104]],
    "SWEEP UP": [[28, 32], [76, 80]],
    "SWEEP DOWN": [[32, 34], [64, 66], [80, 82]],
    "IMPACTO": [[32, 33], [80, 81]],
}


def on(el, b):
    return any(a <= b < z for a, z in G[el])


def hz(m):
    return 440 * 2 ** ((m - 69) / 12)


def pos(bar, step=0.0):
    return int((t0 + (bar * 4 + step / 4) * P) * SR)


def add(sig, at, gain=1.0):
    e = min(N, at + len(sig))
    if 0 <= at < N:
        mix[at:e] += gain * sig[: e - at]


def kick():
    n = int(0.22 * SR); t = np.arange(n) / SR
    f = 48 + 120 * np.exp(-t * 32)
    return np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 11)


def hat(aberto=False):
    n = int((0.12 if aberto else 0.035) * SR)
    s = sosfilt(butter(4, 7500, "hp", fs=SR, output="sos"), rng.standard_normal(n))
    return s * np.exp(-np.arange(n) / SR * (25 if aberto else 90)) * 0.5


def clap():
    n = int(0.14 * SR)
    s = sosfilt(butter(2, [900, 3200], "bp", fs=SR, output="sos"), rng.standard_normal(n))
    env = np.exp(-np.arange(n) / SR * 28)
    for k in (0.008, 0.016):  # 3 batidas de mão
        env[int(k * SR):] += 0.6 * np.exp(-np.arange(n - int(k * SR)) / SR * 60)
    return s * env * 0.7


def osc(m, dur, forma="saw", g=1.0, det=0.0):
    n = int(dur * SR); t = np.arange(n) / SR; f = hz(m) * (1 + det)
    if forma == "saw":
        s = 2 * ((t * f) % 1) - 1
    elif forma == "sq":
        s = np.sign(np.sin(2 * np.pi * f * t))
    else:
        s = np.sin(2 * np.pi * f * t)
    env = np.minimum(1, np.minimum(t / 0.004, np.maximum(0, dur - t) / 0.01))
    return s * env * g


lp_baixo = butter(4, 300, "lp", fs=SR, output="sos")
lp_pad = butter(2, 1800, "lp", fs=SR, output="sos")
graus = [0, 5, 6, 4]


def formantes(src, vogal):
    F = {"a": [(800, 80), (1150, 90), (2900, 120)], "e": [(400, 70), (1600, 80), (2700, 100)],
         "o": [(450, 70), (800, 80), (2830, 100)], "i": [(270, 60), (2140, 90), (2950, 100)]}[vogal]
    y = np.zeros_like(src)
    for i, (fc, bw) in enumerate(F):
        r = np.exp(-np.pi * bw / SR); th = 2 * np.pi * fc / SR
        y += lfilter([1 - r], [1, -2 * r * np.cos(th), r * r], src) * (1.0, 0.5, 0.25)[i]
    return y


def voz(m, dur, vogal):
    n = int(dur * SR); t = np.arange(n) / SR
    f = hz(m) * (1 + 0.012 * np.sin(2 * np.pi * 5.5 * t) * np.minimum(1, t / 0.3))
    ph = np.cumsum(f) / SR
    pulso = (2 * (ph % 1) - 1) + 0.02 * rng.standard_normal(n)
    s = formantes(pulso, vogal)
    env = np.minimum(1, np.minimum(t / 0.04, np.maximum(0, dur - t) / 0.08))
    return s * env


for B in range(TOTAL):
    grau = graus[(B // 4) % 4]
    raiz = (TON + esc[grau]) % 12
    if on("KICK", B):
        for s in (0, 4, 8, 12):
            add(kick(), pos(B, s), 0.9)
    if on("BAIXO", B):
        for s in range(16):
            if s % 4 != 0:
                add(sosfilt(lp_baixo, osc(33 + raiz, P / 4 * 0.85)), pos(B, s), 0.55)
    if on("CLAP", B):
        for s in (4, 12):
            add(clap(), pos(B, s), 0.55)
    if on("HATS", B):
        for s in range(16):
            if s % 4 == 2:
                add(hat(), pos(B, s), 0.45)
            elif s % 2 == 1 and B >= 32:
                add(hat(), pos(B, s), 0.2)
    if on("PAD", B):
        acc = [raiz, (TON + esc[(grau + 2) % 7]) % 12, (TON + esc[(grau + 4) % 7]) % 12]
        sig = sum(osc(57 + a, 4 * P, "saw", 0.05, d) for a in acc for d in (-0.004, 0.004))
        env = np.minimum(1, np.arange(len(sig)) / SR / 0.3)
        add(sosfilt(lp_pad, sig * env), pos(B), 1.0)
    if on("LEAD", B):
        mel = [0, 2, 4, 2, 4, 5, 4, 2, 0, 2, 4, 6, 4, 2, 1, 0]
        for s in range(0, 16, 2):
            g = mel[(s // 2 + (B % 2) * 8) % 16]
            add(osc(76 + (TON + esc[g]) % 12, P / 2 * 0.8, "sq", 0.10), pos(B, s))
    if on("VOZ", B):
        # frase de 2 compassos: 3 sílabas e uma pausa curta
        fr = B % 2
        silabas = [(0, 3, "a", 0), (3, 3, "e", 2), (6, 2, "o", 4)] if fr == 0 else [(0, 4, "i", 4), (4, 3, "a", 2)]
        for ini, d, v, g in silabas:
            add(voz(64 + (TON + esc[g]) % 12, d * P * 0.95, v), pos(B, ini * 4), 0.35)
    for a, z in G["SWEEP UP"]:
        if B == a:
            n = int((z - a) * 4 * P * SR); t = np.arange(n) / n
            ruido = rng.standard_normal(n)
            y = np.zeros(n); blk = 2048
            for i in range(0, n, blk):
                fc = 400 * (9000 / 400) ** (t[i] ** 1.5)
                y[i:i + blk] = sosfilt(butter(2, [fc * 0.7, min(fc * 1.4, 20000)], "bp", fs=SR, output="sos"), ruido[i:i + blk])
            add(y * (0.05 + 0.35 * t ** 2), pos(B))
    for a, z in G["SWEEP DOWN"]:
        if B == a:
            n = int((z - a) * 4 * P * SR); t = np.arange(n) / n
            ruido = rng.standard_normal(n)
            y = np.zeros(n); blk = 2048
            for i in range(0, n, blk):
                fc = 9000 * (300 / 9000) ** t[i]
                y[i:i + blk] = sosfilt(butter(2, [fc * 0.7, min(fc * 1.4, 20000)], "bp", fs=SR, output="sos"), ruido[i:i + blk])
            add(y * 0.35 * (1 - t) ** 1.5, pos(B))
    for a, z in G["IMPACTO"]:
        if B == a:
            n = int(2.5 * SR); t = np.arange(n) / SR
            crash = sosfilt(butter(2, 3500, "hp", fs=SR, output="sos"), rng.standard_normal(n)) * np.exp(-t * 1.6) * 0.45
            boom = np.sin(2 * np.pi * np.cumsum(40 + 60 * np.exp(-t * 8)) / SR) * np.exp(-t * 2.5) * 0.6
            add(crash + boom, pos(B))

mix /= np.max(np.abs(mix)) * 1.05
wavfile.write(out, SR, np.stack([mix, mix], 1).astype(np.float32))
json.dump({"bpm": bpm, "silencioTempos": sil, "elementos": G}, open(gab, "w"), indent=1)
print("ok", out, round(N / SR, 1), "s")

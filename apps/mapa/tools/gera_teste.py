"""Gera músicas sintéticas com gabarito conhecido para testar a análise do MAPA.
uso: python gera_teste.py <saida.wav> <bpm> <tonica 0-11> <menor 0|1> <silencio_inicial_em_tempos> [estilo psy|house]
Estrutura (compassos): INTRO 16 | BUILD 16 | DROP 32 | BREAK 16 | BUILD 8 | DROP 32 | OUTRO 16
"""
import sys
import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, sosfilt

out, bpm, ton, menor, sil = sys.argv[1], float(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), float(sys.argv[5])
estilo = sys.argv[6] if len(sys.argv) > 6 else "psy"
SR = 44100
P = 60.0 / bpm
partes = [("INTRO", 16), ("BUILD", 16), ("DROP", 32), ("BREAK", 16), ("BUILD", 8), ("DROP", 32), ("OUTRO", 16)]
total = sum(n for _, n in partes)
N = int((sil * P + total * 4 * P + 2) * SR)
mix = np.zeros(N)
rng = np.random.default_rng(1)
esc = [0, 2, 3, 5, 7, 8, 10] if menor else [0, 2, 4, 5, 7, 9, 11]
t0 = sil * P


def hz(m):
    return 440 * 2 ** ((m - 69) / 12)


def pos(bar, step):
    return int((t0 + (bar * 4 + step / 4) * P) * SR)


def add(sig, at, gain=1.0):
    e = min(N, at + len(sig))
    if at < N:
        mix[at:e] += gain * sig[: e - at]


def kick():
    n = int(0.18 * SR); t = np.arange(n) / SR
    f = 50 + 110 * np.exp(-t * 35)
    return np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 14)


def hat():
    n = int(0.035 * SR)
    s = sosfilt(butter(4, 7000, "hp", fs=SR, output="sos"), rng.standard_normal(n))
    return s * np.exp(-np.arange(n) / SR * 90) * 0.5


def clap():
    n = int(0.12 * SR)
    s = sosfilt(butter(2, [1000, 3000], "bp", fs=SR, output="sos"), rng.standard_normal(n))
    return s * np.exp(-np.arange(n) / SR * 30) * 0.9


def nota(m, dur, forma="saw", g=1.0):
    n = int(dur * SR); t = np.arange(n) / SR; f = hz(m)
    if forma == "saw":
        s = 2 * ((t * f) % 1) - 1
    elif forma == "sq":
        s = np.sign(np.sin(2 * np.pi * f * t))
    else:
        s = np.sin(2 * np.pi * f * t)
    env = np.minimum(1, np.minimum(t / 0.005, (dur - t) / 0.01))
    return s * env * g


lp_baixo = butter(4, 400, "lp", fs=SR, output="sos")
graus = [0, 5, 6, 0] if menor else [0, 3, 4, 0]   # i - VI - VII - i  (ou I - IV - V - I)
bar = 0
for nome, n in partes:
    for b in range(n):
        B = bar + b
        grau = graus[(B // 4) % 4]
        raiz = esc[grau] + ton
        tem_kick = nome in ("BUILD", "DROP", "OUTRO") and not (nome == "BUILD" and b >= n - 4 and n > 8) and not (nome == "BUILD" and n == 8)
        if tem_kick:
            steps = [0, 4, 8, 12] if estilo in ("psy", "house") else [0, 3, 6, 10]
            for s in steps:
                add(kick(), pos(B, s), 0.9)
        if nome in ("DROP",):
            for s in range(16):
                if estilo == "psy" and s % 4 != 0:
                    m = 24 + (ton % 12) if s // 4 != 3 else 24 + (ton + esc[2]) % 12
                    add(sosfilt(lp_baixo, nota(m, P / 4 * 0.8)), pos(B, s), 0.5)
                elif estilo == "house" and s % 4 == 2:
                    add(sosfilt(lp_baixo, nota(36 + raiz % 12, P / 2 * 0.9)), pos(B, s), 0.55)
            for s in (4, 12):
                add(clap(), pos(B, s), 0.6)
            mel = [0, 2, 4, 2, 0, 4, 5, 4, 2, 0, 1, 2, 4, 6, 4, 2]
            for s in range(0, 16, 2):
                g = mel[(s + (B % 2) * 8) % 16] % 7
                add(nota(72 + (ton + esc[g]) % 12, P / 2 * 0.9, "sq", 0.12), pos(B, s))
        if nome in ("INTRO", "BUILD", "DROP", "OUTRO"):
            for s in range(16):
                if s % 4 == 2 or (nome == "DROP" and s % 2 == 1):
                    add(hat(), pos(B, s), 0.5 if s % 4 == 2 else 0.25)
        if nome in ("INTRO", "BREAK"):
            for i, k in enumerate((0, 2, 4)):
                m = 48 + (ton + esc[(grau + k) % 7]) % 12 + (12 if i and (ton + esc[(grau + k) % 7]) % 12 < (raiz % 12) else 0)
                add(nota(m, 4 * P, "saw", 0.08) + nota(m, 4 * P, "sin", 0.15), pos(B, 0))
        if nome == "BUILD" and b >= n - 4:
            k = b - (n - 4)
            ln = int(4 * P * SR)
            ruido = sosfilt(butter(2, 2000 + k * 2000, "hp", fs=SR, output="sos"), rng.standard_normal(ln))
            add(ruido * np.linspace(0.05 + 0.1 * k, 0.15 + 0.1 * k, ln), pos(B, 0))
    bar += n

mix /= np.max(np.abs(mix)) * 1.05
wavfile.write(out, SR, np.stack([mix, mix], 1).astype(np.float32))
print("gabarito: bpm", bpm, "tom", ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"][ton], "m" if menor else "",
      "silencio", sil, "tempos -> audioInicio", (4 - sil) % 4 if sil >= 0.25 else 0)
acc = 1
for nome, n in partes:
    print(f"  {nome:6s} {acc}-{acc + n - 1}")
    acc += n

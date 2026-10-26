# Trilha + efeitos dos vídeos de story (30 s, 128 BPM), tudo sintetizado aqui (sem sample de terceiros).
# Os tempos batem com video.html: intro 8 tempos, plugins de 4 tempos, final no tempo 48.
# uso: python audio.py <mix|criativo|synth> <saida.wav>
import sys, wave
import numpy as np

P = sys.argv[1]
STORY = len(sys.argv) > 3 and sys.argv[3] == 'story'   # story de 1 plugin (60 s): mesmos tempos do montaPlugin em video.html
SR, DUR, BPM = 44100, (60.0 if STORY else 30.0), 128
BEAT = 60 / BPM
N = int(SR * DUR)
rng = np.random.default_rng(7)

def nota(n): return 440.0 * 2 ** ((n - 69) / 12)      # MIDI -> Hz
def ts(n): return np.arange(n) / SR
def bus(): return np.zeros((2, N))
def put(dst, sig, t0, gain=1.0, pan=0.0):
    i = int(t0 * SR)
    if i >= N: return
    if sig.ndim == 1: sig = np.vstack([sig * np.sqrt((1 - pan) / 2) * 1.414, sig * np.sqrt((1 + pan) / 2) * 1.414])
    j = min(N, i + sig.shape[1]); k = j - i
    if i < 0: sig, k, i = sig[:, -i:], j, 0
    dst[:, i:j] += gain * sig[:, :k]

def biquad(x, fc, q=0.7, kind='lp'):
    """Biquad com corte variável por amostra (fc pode ser vetor)."""
    fc = np.broadcast_to(np.asarray(fc, float), x.shape)
    w = 2 * np.pi * np.clip(fc, 20, SR * 0.45) / SR
    cw, al = np.cos(w), np.sin(w) / (2 * q)
    if kind == 'lp': b0 = b2 = (1 - cw) / 2; b1 = 1 - cw
    elif kind == 'hp': b0 = b2 = (1 + cw) / 2; b1 = -(1 + cw)
    else: b0, b1, b2 = al, np.zeros_like(al), -al          # bp
    a0, a1, a2 = 1 + al, -2 * cw, 1 - al
    b0, b1, b2, a1, a2 = (v / a0 for v in (b0, b1, b2, a1, a2))
    B0, B1, B2, A1, A2, X = (np.broadcast_to(v, x.shape).tolist() for v in (b0, b1, b2, a1, a2, x))
    y = [0.0] * len(X); x1 = x2 = y1 = y2 = 0.0
    for n, xn in enumerate(X):
        v = B0[n] * xn + B1[n] * x1 + B2[n] * x2 - A1[n] * y1 - A2[n] * y2
        x2, x1, y2, y1 = x1, xn, y1, v; y[n] = v
    return np.array(y)

def reverb(st, seg=2.2, mixv=0.35):
    """Convolução com cauda de ruído decaindo (um IR por canal = estéreo largo)."""
    L = int(seg * SR); out = np.empty_like(st)
    for c in range(2):
        ir = rng.standard_normal(L) * np.exp(-ts(L) * 6.9 / seg)
        ir = np.convolve(ir, np.ones(8) / 8, 'same'); ir /= np.sqrt((ir ** 2).sum())
        n = 1 << int(np.ceil(np.log2(st.shape[1] + L)))
        out[c] = np.fft.irfft(np.fft.rfft(st[c], n) * np.fft.rfft(ir, n), n)[:st.shape[1]]
    return st * (1 - mixv) + out * mixv * 2.2

def saw(f, dur, harm=40, cut=None, det=0.0):
    """Serra por soma de harmônicos; cut(t) = corte do filtro em Hz (vetor), suave de 24 dB/oit."""
    t = ts(int(dur * SR)); s = np.zeros_like(t)
    for d in ([0.0] if not det else [-det, det]):
        ff = f * 2 ** (d / 1200); ph = rng.uniform(0, 2 * np.pi)
        for k in range(1, harm + 1):
            if k * ff > SR * 0.45: break
            g = 1.0 / k if cut is None else 1.0 / k / (1 + (k * ff / cut) ** 4)
            s += g * np.sin(2 * np.pi * k * ff * t + ph * k)
    return s / (1 if not det else 2)

def env(dur, a=0.005, d=0.2, s=0.0, r=0.05, hold=None):
    n = int(dur * SR); t = ts(n); hold = dur - r if hold is None else hold
    e = np.where(t < a, t / a, s + (1 - s) * np.exp(-(t - a) / max(d, 1e-4)))
    rel = np.clip((t - hold) / r, 0, 1); return e * (1 - rel)

# ---------- peças ----------
def kick():
    t = ts(int(0.45 * SR)); f = 45 + 110 * np.exp(-t * 28)
    s = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 7.5)
    s[:120] += rng.standard_normal(120) * np.linspace(0.6, 0, 120)
    return np.tanh(s * 1.6)
def hat(dur=0.05, open_=False):
    n = int((0.22 if open_ else dur) * SR); x = rng.standard_normal(n)
    x = np.diff(np.diff(x, prepend=0), prepend=0)
    return x * np.exp(-ts(n) * (14 if open_ else 70)) * 0.25
def clap():
    n = int(0.3 * SR); x = biquad(rng.standard_normal(n), 1400, 1.2, 'bp'); e = np.zeros(n)
    for k, o in enumerate([0, 0.011, 0.022]): i = int(o * SR); e[i:] += np.exp(-ts(n - i) * (90 if k < 2 else 16))
    return x * e * 1.4
def whoosh(dur=0.5, f0=300, f1=6000, up=True):
    n = int(dur * SR); t = ts(n) / dur
    fc = f0 * (f1 / f0) ** (t if up else 1 - t)
    x = biquad(rng.standard_normal(n), fc, 1.4, 'bp')
    e = np.sin(np.pi * t) ** 1.5
    return np.vstack([x * e * (1 - 0.6 * t), x[::-1] * 0 + x * e * (0.4 + 0.6 * t)]) * 0.9   # passa da esq. p/ dir.
def impacto(forte=1.0):
    t = ts(int(2.5 * SR)); f = 32 + 70 * np.exp(-t * 9)
    boom = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 2.2)
    nz = biquad(rng.standard_normal(len(t)), 2500 * np.exp(-t * 3) + 200, 0.7) * np.exp(-t * 5) * 0.6
    return np.tanh((boom + nz) * 1.5) * forte
def riser(dur):
    n = int(dur * SR); t = ts(n) / dur
    x = biquad(rng.standard_normal(n), 200 * 60 ** t, 2.5, 'bp') * t ** 2
    tone = np.sin(2 * np.pi * np.cumsum(220 * 4 ** t) / SR) * t ** 3 * 0.15
    return x * 0.8 + tone
def blip(f, dur=0.12):
    t = ts(int(dur * SR)); return np.sin(2 * np.pi * f * t + 2 * np.sin(2 * np.pi * f * 2 * t) * np.exp(-t * 30)) * np.exp(-t * 28)
def tick():
    n = int(0.012 * SR); return biquad(rng.standard_normal(n), 5000, 2, 'bp') * np.exp(-ts(n) * 400) * 0.7

# ---------- harmonia: Am - F - C - G (lá menor) ----------
CHORDS = [[57, 60, 64], [53, 57, 60], [48, 52, 55], [55, 59, 62]]
ROOTS = [33, 29, 36, 31]
def acorde(bar): return CHORDS[bar % 4]

T_IN = 8 * BEAT
T_OUT = T_IN + (40 if P != 'synth' else 30) * BEAT      # pacotes: 10 slots de 4 tempos · synth: 4 + 6×6 tempos
T_OUT = 48 * BEAT                                       # os dois caem no tempo 48 (22,5 s)
T_FIM = T_OUT + (10 if P != 'synth' else 8) * BEAT      # entra a marca: para a bateria
SLOT = 4 * BEAT
if STORY and P == 'synth':   # montaPlugin: intro 0-8 · foto 8-16 · passeio 5×16 (16-96) · foto 96-112 · final 112 · marca 120
    T_OUT, T_FIM = 112 * BEAT, 120 * BEAT
elif STORY:                  # montaPacote 60 s: 10 slots de 10 tempos (8-108) · final 108 · marca 118
    SLOT = 10 * BEAT
    T_OUT = T_IN + 10 * SLOT; T_FIM = T_OUT + 10 * BEAT

drums, bass, pad, lead, fx = bus(), bus(), bus(), bus(), bus()
K = kick(); H = hat(); HO = hat(open_=True); CL = clap()
beats = [i for i in range(int(DUR / BEAT) + 1) if T_IN <= i * BEAT < T_FIM - 1e-6]
for i in beats:
    t = i * BEAT
    put(drums, K, t, 0.95)
    put(drums, H, t + BEAT / 2, 0.55, 0.3)
    if P != 'mix': put(drums, H, t + BEAT * 0.75, 0.25, -0.3)
    if i % 2 == 1: put(drums, CL, t, 0.5)
    if i % 8 == 7: put(drums, HO, t + BEAT / 2, 0.35, 0.2)
# sidechain: tudo que é tom abaixa no kick
duck = np.ones(N)
for i in beats:
    a = int(i * BEAT * SR); n = min(N - a, int(BEAT * SR)); duck[a:a + n] = 1 - 0.65 * np.exp(-ts(n) * 9)

# baixo: colcheia no contratempo, raiz do acorde
for i in beats:
    bar = i // 4; f = nota(ROOTS[bar % 4] + 12)
    d = BEAT / 2 * 0.95; cut = 300 + 1800 * np.exp(-ts(int(d * SR)) * 18)
    put(bass, saw(f, d, 30, cut, det=6) * env(d, 0.003, 0.15, 0.4, 0.03), i * BEAT + BEAT / 2, 0.5)

# pad: acorde por compasso, do início ao fim (na intro sobe o filtro)
for bar in range(int(DUR / (4 * BEAT)) + 1):
    t0 = bar * 4 * BEAT; d = 4 * BEAT + 0.4
    if t0 >= DUR: break
    tt = t0 + ts(int(d * SR))
    cut = np.interp(tt, [0, T_IN, T_FIM, DUR], [350, 1600, 2200, 700])
    s = sum(saw(nota(n), d, 30, cut, det=12) for n in acorde(bar))
    put(pad, np.vstack([s, np.roll(s, 400)]) * env(d, 0.25, 9, 1, 0.4), t0, 0.16)

# arpejo: synth = protagonista; criativo = discreto; mix = sem
if P != 'mix':
    passo = BEAT / 4 if P == 'synth' else BEAT / 2
    a0 = T_IN if P == 'synth' else T_IN + 16 * BEAT
    k = 0; t = a0
    while t < T_FIM - 1e-6:
        bar = int(t / (4 * BEAT)); ch = acorde(bar); n = ch[[0, 1, 2, 1, 2, 0, 1, 2][k % 8]] + 12 + (12 if k % 16 >= 8 else 0)
        d = passo * 0.9; cut = 900 + 4500 * np.exp(-ts(int(d * SR)) * 20)
        put(lead, saw(nota(n), d, 30, cut, det=8) * env(d, 0.002, 0.12, 0.0, 0.02), t, 0.22 if P == 'synth' else 0.12, 0.35 * np.sin(k))
        k += 1; t += passo

# ---------- efeitos sincronizados com a animação ----------
put(fx, riser(T_IN), 0, 0.45)
put(fx, impacto(1.0), T_IN, 0.9)
if STORY and P == 'synth':
    for i in range(7):     # letras do nome: pluck subindo o acorde
        n = [57, 60, 64, 69, 72, 76, 81][i]; d = 0.6
        put(fx, saw(nota(n), d, 30, 600 + 5000 * np.exp(-ts(int(d * SR)) * 9), det=10) * env(d, 0.002, 0.25, 0, 0.1), 0.5 + i * BEAT / 2, 0.18, -0.6 + 0.2 * i)
    put(fx, whoosh(0.6, 300, 6000), 6 * BEAT - 0.1, 0.4)        # card da intro entra
    put(fx, whoosh(0.8, 200, 3000), T_IN - 0.1, 0.45)            # foto inteira sobe
    T_FULL, SEG = 16 * BEAT, 16 * BEAT
    put(fx, whoosh(0.8, 400, 5000), T_FULL - 0.8, 0.4)           # zoom no 1º trecho
    for i in range(1, 6):                                         # câmera viaja no fim de cada trecho
        put(fx, whoosh(0.8, 400, 5000, up=i % 2 == 0), T_FULL + i * SEG - 0.8, 0.4)
    for i in range(5):
        put(fx, blip(1320 if i % 2 else 990, 0.09), T_FULL + i * SEG + 0.05, 0.18)   # legenda entra
        for c in range(16): put(fx, tick(), T_FULL + i * SEG + 0.25 + c * 0.85 / 16, 0.22, 0.4)
    for i in range(4): put(fx, blip(880 * 2 ** (i * 4 / 12)), T_FULL + i * BEAT, 0.3)   # chips
    put(fx, riser(4 * BEAT), 60 * BEAT, 0.3); put(fx, impacto(0.6), 64 * BEAT, 0.45)   # virada no meio
elif P == 'synth':
    for i in range(7):     # letras de QUIMERA: pluck subindo o acorde
        n = [57, 60, 64, 69, 72, 76, 81][i]; d = 0.6
        put(fx, saw(nota(n), d, 30, 600 + 5000 * np.exp(-ts(int(d * SR)) * 9), det=10) * env(d, 0.002, 0.25, 0, 0.1), 0.5 + i * BEAT / 2, 0.22, -0.6 + 0.2 * i)
    put(fx, blip(1760, 0.5) * 0.6, 6 * BEAT, 0.5)              # logo
    put(fx, whoosh(0.8, 200, 3000), T_IN - 0.1, 0.45)          # foto inteira sobe
    T_FULL, SEG = T_IN + 4 * BEAT, 6 * BEAT
    for i in range(1, 7):                                       # câmera viaja (últimos 0,6 s de cada trecho)
        put(fx, whoosh(0.7, 400, 5000, up=i % 2 == 0), T_FULL + i * SEG - 0.65 if i < 6 else T_FULL - 0.7, 0.4)
    for i in range(4): put(fx, blip(880 * 2 ** (i * 4 / 12)), T_FULL + i * BEAT, 0.3)   # chips
    for i in range(6):                                          # legenda digitando
        for c in range(14): put(fx, tick(), T_FULL + i * SEG + 0.2 + c * 0.6 / 14, 0.25, 0.4)
else:
    put(fx, blip(660), 0.4, 0.25); put(fx, blip(880), 0.4 + BEAT, 0.25)        # LINHA / DE MIX
    for i in range(10):
        t0 = T_IN + i * SLOT
        put(fx, whoosh(0.45, 300, 7000), t0 - 0.25, 0.55)                       # card sai e o próximo entra
        put(fx, blip(1320 if i % 2 else 990, 0.09), t0 + 0.05, 0.18)            # nome aparece
        for c in range(12): put(fx, tick(), t0 + 0.18 + c * 0.52 / 12, 0.22, 0.4)  # descrição digitando
        put(fx, whoosh(0.35, 2000, 400, up=False), t0 + 0.3, 0.2)                # detalhe abre
        if STORY:                                                                # zoom desce de faixa (topo -> meio -> base)
            for f in range(2): put(fx, whoosh(0.3, 3000, 600, up=False), t0 + 0.3 + (f + 0.82) / 3 * (SLOT - 0.55), 0.14)
    for i in range(10): put(fx, blip(523 * 2 ** ([0, 3, 7, 10, 12, 15, 19, 22, 24, 27][i] / 12), 0.1), T_OUT + 0.35 + i * BEAT / 4, 0.22, -0.5 + i * 0.1)
put(fx, impacto(0.7), T_OUT, 0.6)
put(fx, riser(2 * BEAT), T_FIM - 2 * BEAT, 0.4)
put(fx, impacto(1.0), T_FIM, 0.9)

# ---------- mixagem ----------
musica = bass + pad + lead
musica *= duck
mixv = drums * 0.9 + reverb(musica, 1.8, 0.25) + reverb(fx, 2.5, 0.3)
mixv = np.vstack([biquad(mixv[0], 30, 0.7, 'hp'), biquad(mixv[1], 30, 0.7, 'hp')])
mixv = np.tanh(mixv * 1.1)
fade = np.minimum(np.clip(ts(N) / 0.05, 0, 1), np.clip((DUR - ts(N)) / 1.2, 0, 1))
mixv *= fade
mixv *= 10 ** (-1 / 20) / np.abs(mixv).max()
pcm = (mixv.T * 32767).astype('<i2')
with wave.open(sys.argv[2], 'wb') as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR); w.writeframes(pcm.tobytes())
print(P, 'ok', sys.argv[2], 'pico', round(float(np.abs(mixv).max()), 3), 'rms dBFS', round(20 * np.log10(np.sqrt((mixv ** 2).mean())), 1))

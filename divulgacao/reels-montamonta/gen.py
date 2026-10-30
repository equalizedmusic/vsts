# Reels MONTA-MONTA (60 s, 1080x1920): monta a linha do tempo a partir das falas (voz/sNN.wav),
# grava timeline.js (lido pelo reels.html) e sintetiza a trilha + os sons de cada passo, com a narração por cima.
# uso: python gen.py   ->  timeline.js, trilha.wav
import json, wave
import numpy as np

SR, DUR, BPM = 44100, 60.0, 120
BEAT = 60 / BPM
N = int(SR * DUR)
rng = np.random.default_rng(11)
R = json.load(open('roteiro.json', encoding='utf-8'))

def le(f):
    w = wave.open(f); x = np.frombuffer(w.readframes(w.getnframes()), np.int16) / 32768.0
    return x.astype(float)

# ---------- linha do tempo ----------
voz = {s['id']: le(f"voz/{s['id']}.wav") for s in R}
t, segs = 0.5, []
for s in R:
    d = len(voz[s['id']]) / SR
    if s['id'] == 's10': t += 0.25
    segs.append({'id': s['id'], 't0': round(t, 3), 't1': round(t + d, 3), 'en': s['en']})
    t += d + 0.62
S = {s['id']: s for s in segs}
print('fim da fala', segs[-1]['t1'])

def st(i): return S[f's{i:02d}']['t0']
snap = {'VCO': st(3) + 1.7, 'ENVAMP': st(4) + 1.1, 'VCF': st(5) + 1.0, 'ENVMOD': st(6) + 1.0,
        'MIXER': st(7) + 0.7, 'VCO2': st(7) + 1.5}
cut = {'a': snap['VCF'] + 0.3, 'b': st(6) - 0.1}          # varredura do CUTOFF no passo 3

def cutoff_u(tt):  # 1 -> 0 -> 1 (claro, escuro, claro)
    u = 0.5 + 0.5 * np.cos(2 * np.pi * (tt - cut['a']) / (cut['b'] - cut['a']))
    return np.where((tt >= cut['a']) & (tt <= cut['b']), u, 1.0)

# notas: (início, duração, midi, modo) — modo diz o que está montado
notes = []
def seq(t0, t1, step, pat, gate, modo):
    k, tt = 0, t0
    while tt < t1 - 0.05:
        n = pat[k % len(pat)]
        if n: notes.append({'t': round(tt, 3), 'd': round(min(gate, t1 - tt), 3), 'n': n, 'm': modo})
        k += 1; tt += step
notes.append({'t': round(snap['VCO'] + 0.15, 3), 'd': round(st(4) - snap['VCO'] - 0.25, 3), 'n': 48, 'm': 1})   # sem ENV AMP: toca sem parar
seq(snap['ENVAMP'] + 0.2, st(5), BEAT, [48, 55, 51, 53], 0.32, 2)
seq(snap['VCF'] + 0.2, st(6), BEAT / 2, [48, 48, 60, 48, 51, 48, 55, 53], 0.2, 3)
seq(snap['ENVMOD'] + 0.2, st(7), BEAT / 2, [36, 48, 36, 51, 36, 55, 36, 58], 0.18, 4)
seq(snap['VCO2'] + 0.2, st(8), BEAT / 2, [36, 48, 36, 51, 36, 55, 36, 58], 0.18, 5)
RIFF = [36, 36, 48, 36, 39, 36, 46, 36, 36, 36, 48, 36, 43, 36, 41, 39]
seq(st(8), st(10) - 0.3, BEAT / 4 * 2, RIFF, 0.16, 6)
seq(st(10), DUR - 1.2, BEAT / 4 * 2, RIFF, 0.15, 7)

json.dump({'DUR': DUR, 'FPS': 30, 'segs': segs, 'snap': snap, 'cut': cut, 'notes': notes, 'beat': BEAT, 'beatStart': st(10)},
          open('timeline.json', 'w'), indent=0)
open('timeline.js', 'w').write('window.TL = ' + json.dumps({'DUR': DUR, 'FPS': 30, 'segs': segs, 'snap': snap, 'cut': cut,
                                                             'notes': notes, 'beat': BEAT, 'beatStart': st(10)}) + ';\n')

# ---------- síntese ----------
def mtof(n): return 440.0 * 2 ** ((n - 69) / 12)
def put(dst, sig, t0, g=1.0, pan=0.0):
    i = int(t0 * SR)
    if i >= N: return
    if sig.ndim == 1: sig = np.vstack([sig * np.sqrt(1 - pan), sig * np.sqrt(1 + pan)])
    j = min(N, i + sig.shape[1]); dst[:, i:j] += g * sig[:, :j - i]

def saw(f, n, ph=0.0):
    t = np.arange(n) / SR; s = np.zeros(n)
    for k in range(1, 200):
        if k * f > SR * 0.45: break
        s += np.sin(2 * np.pi * k * f * t + ph * k) / k
    return s * 0.6

def ladder(x, fc, res=0.3):
    """Ladder de 4 polos (TPT simplificado) com corte por amostra."""
    fc = np.broadcast_to(np.clip(fc, 30, SR * 0.4), x.shape)
    g = np.tan(np.pi * fc / SR); G = (g / (1 + g)).tolist(); X = x.tolist()
    s1 = s2 = s3 = s4 = 0.0; y = [0.0] * len(X); k = res * 3.6
    for i, xn in enumerate(X):
        a = G[i]; u = np.tanh(xn - k * s4) if False else (xn - k * s4)
        u = u / (1 + abs(u) * 0.15)
        v = (u - s1) * a; y1 = v + s1; s1 = y1 + v
        v = (y1 - s2) * a; y2 = v + s2; s2 = y2 + v
        v = (y2 - s3) * a; y3 = v + s3; s3 = y3 + v
        v = (y3 - s4) * a; y4 = v + s4; s4 = y4 + v
        y[i] = y4
    return np.array(y) * (1 + k * 0.5)

def adsr(n, gate, a, d, s, r):
    t = np.arange(n) / SR
    e = np.where(t < a, t / a, s + (1 - s) * np.exp(-(t - a) / d))
    rel = np.clip((t - gate) / r, 0, 1)
    eg = e[min(n - 1, int(gate * SR))] if gate * SR < n else e[-1]
    return np.where(t < gate, e, eg * (1 - rel))

demo = np.zeros((2, N))
for nt in notes:
    m, f = nt['m'], mtof(nt['n'])
    rel = 0.25 if m >= 2 else 0.05
    n = int((nt['d'] + rel) * SR); tt = nt['t'] + np.arange(n) / SR
    x = saw(f, n, rng.uniform(0, 6))
    if m >= 5: x = 0.6 * x + 0.6 * saw(f * 2 ** (12 / 1200), n, rng.uniform(0, 6))
    if m == 1: env = np.minimum(1, np.arange(n) / (0.01 * SR)) * np.clip((n - np.arange(n)) / (0.04 * SR), 0, 1)
    else: env = adsr(n, nt['d'], 0.008, 0.25 if m == 2 else 0.12, 0.6 if m == 2 else 0.35, rel)
    if m == 3: fc = 140 * 2 ** (cutoff_u(tt) * 6.4)
    elif m >= 4:
        fe = adsr(n, nt['d'], 0.003, 0.11 if m < 6 else 0.09, 0.0, 0.1)
        fc = 260 + (3800 if m < 6 else 2600) * fe
    else: fc = None
    if fc is not None: x = ladder(x, fc, 0.45 if m >= 4 else 0.3)
    g = {1: 0.33, 2: 0.42, 3: 0.5, 4: 0.55, 5: 0.5, 6: 0.42, 7: 0.36}[m]
    put(demo, x * env, nt['t'], g)

# estalo de encaixe
sfx = np.zeros((2, N))
def clique():
    n = int(0.12 * SR); t = np.arange(n) / SR
    thump = np.sin(2 * np.pi * (90 + 160 * np.exp(-t * 60)) * t) * np.exp(-t * 35)
    tick = rng.standard_normal(n) * np.exp(-t * 300)
    return 0.7 * thump + 0.35 * np.diff(tick, prepend=0)
for k, v in snap.items(): put(sfx, clique(), v, 0.55)

# batida (fim): bumbo, hat, palma
beat = np.zeros((2, N))
def kick():
    n = int(0.4 * SR); t = np.arange(n) / SR
    return np.tanh(1.5 * np.sin(2 * np.pi * np.cumsum(45 + 120 * np.exp(-t * 30)) / SR) * np.exp(-t * 8))
def hat():
    n = int(0.06 * SR); x = np.diff(rng.standard_normal(n), prepend=0); return x * np.exp(-np.arange(n) / SR * 60) * 0.2
def clap():
    n = int(0.25 * SR); x = np.diff(rng.standard_normal(n), prepend=0); e = np.zeros(n)
    for o in (0, 0.01, 0.02): i = int(o * SR); e[i:] += np.exp(-np.arange(n - i) / SR * 40)
    return x * e * 0.18
t0 = st(10); b = 0
while t0 + b * BEAT < DUR - 1.0:
    tb = t0 + b * BEAT
    put(beat, kick(), tb, 0.6)
    put(beat, hat(), tb + BEAT / 2, 0.5, 0.3)
    if b % 2: put(beat, clap(), tb, 0.6, -0.1)
    b += 1
# leve antes do fim: hats nos passos 8–9
tb = st(8)
while tb < st(10) - 0.3: put(beat, hat(), tb + BEAT / 2, 0.35, 0.3); tb += BEAT

# pad de abertura (até o passo 1)
pad = np.zeros((2, N)); n = int((st(3) + 0.8) * SR); t = np.arange(n) / SR
for f, p in ((mtof(48), -0.4), (mtof(55), 0.4), (mtof(63), 0.0), (mtof(67), 0.2)):
    x = ladder(saw(f, n, rng.uniform(0, 6)) + saw(f * 1.004, n, rng.uniform(0, 6)), 500 + 300 * np.sin(t * 0.8), 0.2)
    put(pad, x * np.minimum(1, t / 1.5) * np.clip((st(3) + 0.8 - t) / 0.8, 0, 1), 0, 0.12, p)

# eco/reverb curtinho no synth
def eco(x, dt=0.375, fb=0.32, mix=0.22):
    y = x.copy(); d = int(dt * SR)
    for k in range(1, 5):
        sh = np.zeros_like(x); sh[:, d * k:] = x[:, :-d * k] * (fb ** k); y += mix * sh[::-1] if k % 2 else mix * sh
    return y
demo = eco(demo)

# narração
nar = np.zeros(N)
for s in segs:
    v = voz[s['id']]; i = int(s['t0'] * SR); nar[i:i + len(v)] += v[:N - i]
nar /= np.abs(nar).max() / 0.85
# duck da trilha sob a fala (envelope suave)
envv = np.convolve(np.abs(nar) > 0.02, np.ones(int(0.25 * SR)) / (0.25 * SR), 'same')
duck = 1 - 0.55 * np.clip(envv * 3, 0, 1)
musica = (demo + beat + pad) * duck + sfx
musica *= 0.55 / max(1e-9, np.abs(musica).max())
mix = musica + np.vstack([nar, nar]) * 0.95
fade = np.clip((DUR - np.arange(N) / SR) / 1.0, 0, 1)
mix *= fade
mix /= max(1.0, np.abs(mix).max() / 0.97)
w = wave.open('trilha.wav', 'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
w.writeframes((mix.T * 32767).astype(np.int16).tobytes()); w.close()
print('ok trilha.wav', len(notes), 'notas')

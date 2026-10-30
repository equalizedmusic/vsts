# MESA — "revelação" de foto sobre o render da amostra: o que a lente e o sensor fazem e o 3D não faz.
# Uso: python pos_foto.py entrada.png saida.png
#   - aberração cromática leve nas bordas (vermelho e azul não caem no mesmo ponto)
#   - vinheta suave, curva de contraste de foto, granulado de sensor (ISO baixo)
import sys
import numpy as np
from PIL import Image, ImageFilter

src, dst = sys.argv[1], sys.argv[2]
im = Image.open(src).convert('RGB'); W, H = im.size
r, g, b = im.split()

def escala(ch, f):
    w2, h2 = int(round(W * f)), int(round(H * f))
    c = ch.resize((w2, h2), Image.LANCZOS)
    x0, y0 = (w2 - W) // 2, (h2 - H) // 2
    return c.crop((x0, y0, x0 + W, y0 + H))

r = escala(r, 1.0012); b = escala(b, 0.9990)
a = np.dstack([np.asarray(r), np.asarray(g), np.asarray(b)]).astype(np.float32) / 255.0

yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
d = np.sqrt(((xx - W / 2) / (W / 2)) ** 2 + ((yy - H / 2) / (H / 2)) ** 2) / np.sqrt(2)
a *= (1.0 - 0.22 * d ** 2.2)[..., None]

a = a + 0.4 * (a - a ** 2) * (2 * a - 1)                       # leve S (contraste de foto)
a = np.clip(a * np.array([1.012, 1.0, 0.985]), 0, 1)            # um tico mais quente

rng = np.random.default_rng(7)
n = rng.normal(0, 1, (H, W)).astype(np.float32)
n = np.asarray(Image.fromarray(((n * 32) + 128).clip(0, 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.6))).astype(np.float32)
n = (n - 128) / 32.0
lum = a.mean(axis=2, keepdims=True)
a = a + n[..., None] * 0.012 * (0.4 + (1 - lum))               # grão aparece mais nas sombras
Image.fromarray((np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8)).save(dst)
print('ok', dst, W, H)

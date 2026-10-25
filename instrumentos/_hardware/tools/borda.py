# Some a sombra na borda das peças renderizadas: a sombra do softbox chega até a beira do quadro e,
# no painel claro, o recorte aparecia como um quadrado cinza em volta do knob. A transparência cai a zero
# perto da borda (faixa = fração do lado). Uso: python borda.py <pasta> (altera os PNG no lugar)
import sys, os
import numpy as np
from PIL import Image
FAIXA = {'knob': 0.14, 'lampada': 0.12, 'led': 0.11, 'tecla': 0.08}
pasta = sys.argv[1]
for f in sorted(os.listdir(pasta)):
    tipo = next((t for t in FAIXA if f.startswith(t)), None)
    if not tipo or not f.endswith('.png') or '_luz' in f: continue
    im = np.array(Image.open(os.path.join(pasta, f)).convert('RGBA')).astype(np.float32)
    h, w = im.shape[:2]; fx = FAIXA[tipo] * min(w, h)
    y, x = np.mgrid[0:h, 0:w]
    d = np.minimum.reduce([x, y, w - 1 - x, h - 1 - y]).astype(np.float32)
    t = np.clip(d / fx, 0, 1); m = t * t * (3 - 2 * t)
    im[..., 3] *= m
    Image.fromarray(im.round().astype(np.uint8)).save(os.path.join(pasta, f))
    print(f, round(fx, 1))

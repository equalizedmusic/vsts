# Refaz a sombra das peças renderizadas. A sombra do Blender (shadow catcher) é longa e batia na borda do
# quadro: no painel ela aparecia como um canto quadrado embaixo/à direita de cada knob. Aqui a peça é
# separada da sombra (no shadow catcher a sombra sai com RGB = 0) e ganha uma sombra nova, redonda e macia,
# que termina antes da borda. Substitui o antigo borda.py.
# Uso: python sombra.py <arquivo.png ou pasta> ...   (altera no lugar só as peças com sombra: knobs, alavanca, LED e lâmpada)
import sys, os
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

# (deslocamento, desfoque, opacidade) em fração do lado menor: sombra projetada + sombra de contato
CAMADAS = [((0.022, 0.032), 0.030, 0.50), ((0.006, 0.009), 0.008, 0.45)]
PECAS = ('knob_', 'rate_', 'mix_', 'alav_', 'led_off', 'lampada_off')

def corpo(p):
    """máscara da peça: pixels com cor (a sombra é preta pura), com os buracos internos preenchidos"""
    m = ((p[..., :3].max(-1) >= 6) & (p[..., 3] > 0)).astype(np.uint8) * 255
    fora = Image.fromarray(255 - m).convert('L')
    h, w = m.shape
    big = Image.new('L', (w + 2, h + 2), 255); big.paste(fora, (1, 1))
    ImageDraw.floodfill(big, (0, 0), 128)
    fundo = np.array(big)[1:-1, 1:-1] == 128          # só o que encosta na borda é fundo de verdade
    return ~fundo

def refaz(caminho):
    im = Image.open(caminho).convert('RGBA')
    p = np.array(im)
    dentro = corpo(p)
    a = p[..., 3].astype(np.float32) * dentro           # opacidade só da peça
    peca = p.copy(); peca[..., 3] = a.round().astype(np.uint8)
    h, w = a.shape; lado = min(w, h)
    sombra = np.zeros_like(a)
    base = Image.fromarray(a.round().astype(np.uint8))
    for (dx, dy), blur, op in CAMADAS:
        s = Image.new('L', base.size, 0); s.paste(base, (round(dx * lado), round(dy * lado)))
        s = s.filter(ImageFilter.GaussianBlur(blur * lado))
        sombra = 1 - (1 - sombra) * (1 - np.array(s, np.float32) / 255 * op)
    camada = np.zeros_like(p); camada[..., 3] = (sombra * 255).round().astype(np.uint8)
    out = Image.fromarray(camada); out.alpha_composite(Image.fromarray(peca))
    o = np.array(out)
    borda = max(o[0, :, 3].max(), o[-1, :, 3].max(), o[:, 0, 3].max(), o[:, -1, 3].max())
    out.save(caminho)
    print(os.path.basename(caminho), 'alfa máx. na borda:', borda)

for arg in sys.argv[1:]:
    arqs = [os.path.join(arg, f) for f in sorted(os.listdir(arg))] if os.path.isdir(arg) else [arg]
    for f in arqs:
        if f.endswith('.png') and os.path.basename(f).startswith(PECAS):
            refaz(f)

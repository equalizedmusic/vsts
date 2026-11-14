# Imagens dos devices Max for Live (VASSOURA, RODO) para o site — roda sozinho no build.mjs (se houver Python + Pillow).
#
# Fonte: D:\Apps\granular-vst\tmp\shots-m4l\ (fora do git; prints da janela e do device tirados do Live/simulador):
#   vassoura-janela.png  -> img/vassoura.webp        (capa da VASSOURA = a janela)  + img/vassoura-janela.webp
#   vassoura-device.png  -> img/vassoura-device.webp (o device na faixa, só com o botão OPEN)
#   rodo-janela.png      -> img/rodo.webp            (capa do RODO = a janela)      + img/rodo-janela.webp
#   rodo-device.png      -> img/rodo-device.webp
# Sem o print do RODO, usa a prévia do simulador (vsts/tmp/teste-rodo/out/2-applying.png) só para a capa.
# img/vassourarodo.webp (capa do combo) = as duas janelas sobrepostas, no molde do maestromagos.webp (1600x1000).
# Só regrava o .webp quando o .png é mais novo (o build não muda o site à toa).
import os
import sys
from PIL import Image

RAIZ = os.path.dirname(os.path.abspath(__file__))
IMG = os.path.join(RAIZ, 'img')
SHOTS = os.environ.get('SHOTS_M4L', r'D:\Apps\granular-vst\tmp\shots-m4l')
RESERVA = {'rodo-janela': r'D:\Apps\granular-vst\vsts\tmp\teste-rodo\out\2-applying.png'}
mudou = False


def novo(origem, destino):
    return os.path.exists(origem) and (not os.path.exists(destino) or os.path.getmtime(origem) > os.path.getmtime(destino))


def grava(im, destino, largura_max):
    global mudou
    im = im.convert('RGB')
    if im.width > largura_max:
        im = im.resize((largura_max, round(im.height * largura_max / im.width)), Image.LANCZOS)
    im.save(destino, 'WEBP', quality=86, method=6)
    mudou = True
    print('m4l-imagens:', os.path.basename(destino), im.size)


def fonte(nome):
    p = os.path.join(SHOTS, nome + '.png')
    if os.path.exists(p):
        return p
    r = RESERVA.get(nome)
    return r if r and os.path.exists(r) else None


for dev in ('vassoura', 'rodo'):
    jan = fonte(dev + '-janela')
    if jan:
        # a prévia de reserva vira só a capa; a foto "janela" da galeria só sai de um print de verdade
        real = os.path.exists(os.path.join(SHOTS, dev + '-janela.png'))
        for destino in (dev + '.webp',) + ((dev + '-janela.webp',) if real else ()):
            d = os.path.join(IMG, destino)
            if novo(jan, d):
                grava(Image.open(jan), d, 1600)
    dv = fonte(dev + '-device')
    if dv and novo(dv, os.path.join(IMG, dev + '-device.webp')):
        grava(Image.open(dv), os.path.join(IMG, dev + '-device.webp'), 1200)

# capa do combo: VASSOURA atrás (alto/esquerda), RODO na frente (baixo/direita), sombra, fundo escuro do site
cv, cr, cc = (os.path.join(IMG, n) for n in ('vassoura.webp', 'rodo.webp', 'vassourarodo.webp'))
if os.path.exists(cv) and os.path.exists(cr) and (not os.path.exists(cc) or max(os.path.getmtime(cv), os.path.getmtime(cr)) > os.path.getmtime(cc)):
    W, H = 1600, 1000
    fundo = Image.new('RGB', (W, H), (13, 11, 20))
    a = Image.open(cv).convert('RGB')
    b = Image.open(cr).convert('RGB')
    la = 1040
    a = a.resize((la, round(a.height * la / a.width)), Image.LANCZOS)
    b = b.resize((la, round(b.height * la / b.width)), Image.LANCZOS)
    sombra = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    from PIL import ImageDraw, ImageFilter
    ds = ImageDraw.Draw(sombra)
    pa, pb = (40, 40), (W - 40 - la, H - 40 - b.height)
    ds.rectangle([pb[0] - 6, pb[1] + 10, pb[0] + la + 18, pb[1] + b.height + 26], fill=(0, 0, 0, 170))
    sombra = sombra.filter(ImageFilter.GaussianBlur(18))
    fundo.paste(a, pa)
    fundo = Image.alpha_composite(fundo.convert('RGBA'), sombra).convert('RGB')
    fundo.paste(b, pb)
    grava(fundo, cc, 1600)

if not mudou:
    print('m4l-imagens: nada novo')
sys.exit(0)

# SERIE KNOB - prévia fotorrealista (Blender/Cycles)
# uso: blender -b -P pedal.py -- <saida.png> <pedal> [valor 0-10] [amostras] [escala%]
import bpy, bmesh, math, os, sys
from mathutils import Vector

argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
# texturas (nogueira: cor e rugosidade; HDRI de estúdio; Poly Haven, CC0) ficam em texturas/, ao lado do script.
# Sem elas, a madeira sai lisa e o fundo do mundo fica cinza.
DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'texturas') + os.sep
SAIDA = argv[0] if argv else os.path.join(os.getcwd(), 'fole.png')
PEDAL = argv[1] if len(argv) > 1 else 'fole'
VALOR_ARG = float(argv[2]) if len(argv) > 2 and argv[2] != '-' else None
AMOSTRAS = int(argv[3]) if len(argv) > 3 else 96
ESCALA = int(argv[4]) if len(argv) > 4 else 100
ACAB = argv[5] if len(argv) > 5 else 'aco'  # chapa igual em todos: 'aco' (escovado) ou 'preto'
def imagem(nome):
    f = DIR + nome
    return bpy.data.images.load(f) if os.path.exists(f) else None

def hexcor(h): return tuple((int(h[i:i + 2], 16) / 255) ** 2.2 for i in (0, 2, 4))

# formas do visor SHAPE (t de 0 a 2 compassos, retorno 0..1) — mesmas da prévia 2D
def _fole(t): b = (t * 4) % 1; return 1 - 0.92 * math.exp(-b * 7) * (1 - math.exp(-b * 60))
def _picador(t):
    pat = [1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1]; s = int(t * 16) % 16
    return 0.95 if pat[s] and (t * 16) % 1 < 0.78 else 0.08
def _vaivem(t): x = (t * 2) % 1; return 0.5 + 0.42 * ((x * 4 - 1) if x < 0.5 else (3 - x * 4))
def _repique(t):
    ln = 0.25 if t < 0.5 else 0.125 if t < 1 else 0.0625 if t < 1.5 else 0.03125
    return 0.1 + 0.85 * math.exp(-((t % ln) / ln) * 3)
def _eco(t): k = int(t / 0.1875); u = (t % 0.1875) / 0.1875; return 0.05 + 0.9 * 0.78 ** k * math.exp(-u * 9)
def _avesso(t):
    s = int(t * 8); u = (t * 8) % 1
    return 0.08 + 0.85 * u ** 1.6 if s % 2 else 0.08 + 0.85 * math.exp(-u * 3.5)
def _freio(t): b = t % 1; return 0.9 if b < 0.75 else 0.9 * (1 - (b - 0.75) / 0.25) ** 1.8
def _escada(t):
    st = [0, 3, 5, 7, 10, 12, 15, 17, 12, 10, 7, 5, 3, 0, -2, 0]; return 0.08 + (st[int(t * 8) % 16] + 2) / 19 * 0.84
def _mastiga(t):
    bits = 3 if (t * 8) % 1 < 0.5 else 12; return 0.5 + 0.42 * round(math.sin(t * 38) * bits) / bits

# emblemas do centro do knob: linhas em coordenadas -1..1
def _seno(y0, amp, ciclos): return [(-0.9 + 1.8 * k / 40, y0 + amp * math.sin(2 * math.pi * ciclos * k / 40)) for k in range(41)]
def _arco(cx, cy, r, a0, a1, n=24): return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * k / n)), cy + r * math.sin(math.radians(a0 + (a1 - a0) * k / n))) for k in range(n + 1)]
EMBLEMAS = {
    # sanfona: duas tampas e as dobras do fole entre elas
    'fole': [[(-0.9, -0.8), (-0.9, 0.8)], [(0.9, -0.8), (0.9, 0.8)],
             [(-0.9 + 0.3 * k, 0.45 if k % 2 == 0 else 0.7) for k in range(7)],
             [(-0.9 + 0.3 * k, -0.45 if k % 2 == 0 else -0.7) for k in range(7)]]
             + [[(-0.9 + 0.3 * k, 0.45 if k % 2 == 0 else 0.7), (-0.9 + 0.3 * k, -0.45 if k % 2 == 0 else -0.7)] for k in range(1, 6)],
    'picador': [[(-0.95, -0.4), (-0.6, -0.4), (-0.6, 0.4), (-0.2, 0.4), (-0.2, -0.4), (0.2, -0.4), (0.2, 0.4), (0.6, 0.4), (0.6, -0.4), (0.95, -0.4)]],
    'mare': [_seno(0.35, 0.22, 1.5), _seno(-0.35, 0.22, 1.5)],
    'vaivem': [[(-0.9, 0), (0.9, 0)], [(-0.5, 0.38), (-0.9, 0), (-0.5, -0.38)], [(0.5, 0.38), (0.9, 0), (0.5, -0.38)]],
    'repique': [[(x, -0.7), (x, -0.7 + h)] for x, h in ((-0.8, 1.5), (-0.4, 1.15), (0.0, 0.85), (0.4, 0.6), (0.8, 0.4))],
    'eco': [_arco(-0.75, 0, r, -42, 42) for r in (0.45, 0.95, 1.45)] + [_arco(-0.75, 0, 0.12, 0, 360, 16)],
    'avesso': [_arco(0, 0, 0.72, 110, -200), [(-0.45, 0.85), (-0.25, 0.68), (-0.5, 0.45)]],
    'freio': [[(-0.9 + 1.8 * k / 30, 0.65 - 1.35 * (k / 30) ** 2.6) for k in range(31)], [(-0.9, -0.75), (0.9, -0.75)]],
    'escada': [[(-0.9, -0.7), (-0.45, -0.7), (-0.45, -0.25), (0, -0.25), (0, 0.2), (0.45, 0.2), (0.45, 0.65), (0.9, 0.65)]],
    'mastiga': [[(-0.9 + 0.36 * k, 0.15 if k % 2 == 0 else 0.62) for k in range(6)],
                [(-0.9 + 0.36 * k, -0.15 if k % 2 == 0 else -0.62) for k in range(6)]],
}

# por pedal: nome, subtítulo, painel, lâmpada, valor do knob, rate, shape, trigger (INT/MIDI/AUDIO) e origem
PEDAIS = {
    'fole':    ('FOLE',    'SIDECHAIN SINCRONIZADO', 'a03834', 'ff6b14', 6, '1/4',    'CLÁSSICO',     'AUDIO', 'SC: canal 3 · kick', _fole),
    'picador': ('PICADOR', 'TRANCE GATE',            '1e2a10', 'a6ff2e', 6, '1/16',   'TRANCE 2',     'INT',   'tempo do DAW', _picador),
    'mare':    ('MARÉ',    'FILTRO EM MOVIMENTO',    '0c2a52', '3fc8ff', 6.5, '2 COMP', 'SENO',       'INT',   'tempo do DAW', lambda t: 0.5 - 0.42 * math.cos(t * math.pi)),
    'vaivem':  ('VAIVÉM',  'AUTO-PAN NO TEMPO',      '4a0f30', 'ff3fb4', 8, '1/2',    'TRIÂNGULO',    'INT',   'tempo do DAW', _vaivem),
    'repique': ('REPIQUE', 'STUTTER · BEAT REPEAT',  '1a1a1c', 'ff8c1a', 4, '1/16',   'ROLL',         'MIDI',  'MIDI: canal 2 · C1', _repique),
    'eco':     ('ECO',     'DELAY DE FITA',          '4a3810', 'ffd23a', 5.5, '3/16', 'PING-PONG',    'INT',   'tempo do DAW', _eco),
    'avesso':  ('AVESSO',  'REVERSO NO TEMPO',       '2a1450', 'a66bff', 5, '1/8',    'ALTERNADO',    'INT',   'tempo do DAW', _avesso),
    'freio':   ('FREIO',   'TAPE STOP · TAPE START', '2a3038', 'e8f4ff', 3, '1 COMP', 'FIM DO COMP.', 'MIDI',  'MIDI: canal 1 · C3', _freio),
    'escada':  ('ESCADA',  'SEQUENCIADOR DE PITCH',  '0c3a32', '2effc0', 7, '1/8',    'SOBE',         'INT',   'tempo do DAW', _escada),
    'mastiga': ('MASTIGA', 'BITCRUSH RITMADO',       '141c4a', '5a74ff', 8.5, '1/8',  'QUADRADO',     'AUDIO', 'SC: canal 5 · snare', _mastiga),
}
NOME, SUB, _pn, _lp, _val, RATE_TXT, SHAPE_TXT, TRIG, ORIGEM, FORMA_FN = PEDAIS[PEDAL]
COR_LAMP = hexcor(_lp)  # só o LED muda de pedal para pedal
VALOR = VALOR_ARG if VALOR_ARG is not None else _val

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
# Segoe UI do Windows; fora dele, a fonte padrão do Blender
def fonte(nome):
    f = os.path.join(os.environ.get('WINDIR', 'C:/Windows'), 'Fonts', nome)
    return bpy.data.fonts.load(f) if os.path.exists(f) else bpy.data.fonts.load('<builtin>')
FONTE = fonte('segoeuisl.ttf')
FONTE_B = fonte('segoeuib.ttf')
FONTE_M = fonte('seguisb.ttf')  # semibold: legível nas letras pequenas

# ---------------- materiais ----------------
def principled(nome, base, metal=0.0, rough=0.5, coat=0.0, coat_rough=0.05, emis=None, forca=0.0):
    m = bpy.data.materials.new(nome); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*base, 1)
    b.inputs['Metallic'].default_value = metal
    b.inputs['Roughness'].default_value = rough
    b.inputs['Coat Weight'].default_value = coat
    b.inputs['Coat Roughness'].default_value = coat_rough
    if emis:
        b.inputs['Emission Color'].default_value = (*emis, 1)
        b.inputs['Emission Strength'].default_value = forca
    return m

def escovado(m, escala=(1.5, 500, 1.5), forca=0.12, radial=False):
    """metal escovado: riscos finos (lineares ou concêntricos) no bump e na rugosidade"""
    nt = m.node_tree; n = nt.nodes; l = nt.links
    b = n['Principled BSDF']
    tc = n.new('ShaderNodeTexCoord')
    if radial:
        t = n.new('ShaderNodeTexWave'); t.wave_type = 'RINGS'
        t.inputs['Scale'].default_value = 220; t.inputs['Distortion'].default_value = 1.5
        t.inputs['Detail'].default_value = 6
        l.new(tc.outputs['Object'], t.inputs['Vector'])
    else:
        mp = n.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = escala
        t = n.new('ShaderNodeTexNoise'); t.inputs['Scale'].default_value = 3; t.inputs['Detail'].default_value = 12
        l.new(tc.outputs['Object'], mp.inputs['Vector']); l.new(mp.outputs['Vector'], t.inputs['Vector'])
    bp = n.new('ShaderNodeBump'); bp.inputs['Strength'].default_value = forca; bp.inputs['Distance'].default_value = 0.01
    l.new(t.outputs['Fac'], bp.inputs['Height']); l.new(bp.outputs['Normal'], b.inputs['Normal'])
    mr = n.new('ShaderNodeMapRange')
    r0 = b.inputs['Roughness'].default_value
    mr.inputs['To Min'].default_value = r0 * 0.7; mr.inputs['To Max'].default_value = r0 * 1.3
    l.new(t.outputs['Fac'], mr.inputs['Value']); l.new(mr.outputs['Result'], b.inputs['Roughness'])
    return m

def madeira(nome, eixo):
    """nogueira escaneada (Poly Haven, CC0), veio ao longo da peça, escurecida e envernizada"""
    m = principled(nome, (0.2, 0.08, 0.03), rough=0.4, coat=0.8, coat_rough=0.1)
    nt = m.node_tree; n = nt.nodes; l = nt.links; b = n['Principled BSDF']
    tc = n.new('ShaderNodeTexCoord'); mp = n.new('ShaderNodeMapping')
    # escala maior que antes: o veio aparece em vez de virar marrom liso
    mp.inputs['Scale'].default_value = (0.1, 0.1, 0.1)
    mp.inputs['Location'].default_value = (0.37 if eixo == 'x' else 0.0, 0.21, 0)
    if eixo == 'x': mp.inputs['Rotation'].default_value = (0, 0, math.radians(90))
    l.new(tc.outputs['Object'], mp.inputs['Vector'])
    img_cor, img_rug = imagem('nogueira_cor.jpg'), imagem('nogueira_rug.jpg')
    if not (img_cor and img_rug): return m
    cor = n.new('ShaderNodeTexImage'); cor.image = img_cor
    rug = n.new('ShaderNodeTexImage'); rug.image = img_rug
    rug.image.colorspace_settings.name = 'Non-Color'
    for t in (cor, rug): l.new(mp.outputs['Vector'], t.inputs['Vector'])
    mx = n.new('ShaderNodeMix'); mx.data_type = 'RGBA'; mx.blend_type = 'MULTIPLY'
    mx.inputs['Factor'].default_value = 1.0; mx.inputs['B'].default_value = (0.5, 0.4, 0.32, 1)
    # contraste do veio: realça o desenho da nogueira
    bc = n.new('ShaderNodeBrightContrast'); bc.inputs['Contrast'].default_value = 0.25
    hs = n.new('ShaderNodeHueSaturation'); hs.inputs['Saturation'].default_value = 0.7
    l.new(cor.outputs['Color'], hs.inputs['Color']); l.new(hs.outputs['Color'], bc.inputs['Color']); l.new(bc.outputs['Color'], mx.inputs['A'])
    l.new(mx.outputs['Result'], b.inputs['Base Color'])
    l.new(rug.outputs['Color'], b.inputs['Roughness'])
    bp = n.new('ShaderNodeBump'); bp.inputs['Strength'].default_value = 0.12
    l.new(cor.outputs['Color'], bp.inputs['Height']); l.new(bp.outputs['Normal'], b.inputs['Normal'])
    return m

if ACAB == 'aco':
    M_PAINEL = escovado(principled('painel', (0.93, 0.93, 0.94), metal=1, rough=0.45), escala=(1.5, 700, 1.5), forca=0.18)
else:
    M_PAINEL = escovado(principled('painel', (0.025, 0.025, 0.027), metal=1, rough=0.36), forca=0.1)
M_ALU = principled('alu', (0.91, 0.91, 0.92), metal=1, rough=0.2)
M_ALU_LISO = principled('alu_liso', (0.9, 0.9, 0.91), metal=1, rough=0.16)
M_CROMO = principled('cromo', (0.95, 0.95, 0.95), metal=1, rough=0.06)
M_TAMPA = escovado(principled('tampa', (0.012, 0.012, 0.013), rough=0.28, coat=1, coat_rough=0.04), radial=True, forca=0.03)
M_TINTA = principled('tinta', (0.86, 0.8, 0.66), rough=0.55)
# serigrafia da chapa: preta no aço, creme no preto
M_SERIG = principled('serig', (0.012, 0.012, 0.012), rough=0.6) if ACAB == 'aco' else M_TINTA
M_PRETO = principled('preto', (0.02, 0.02, 0.022), metal=0.6, rough=0.4)
M_VIDRO = principled('vidro', (0.004, 0.004, 0.005), rough=0.75)
M_LAMP_ON = principled('lamp_on', COR_LAMP, rough=0.05, coat=1, emis=COR_LAMP, forca=2.0)
M_LAMP_OFF = principled('lamp_off', tuple(v * 0.06 for v in COR_LAMP), rough=0.05, coat=1, coat_rough=0.02)
M_TELA = principled('tela', COR_LAMP, emis=COR_LAMP, forca=3.5)
M_FENDA = principled('fenda', (0.01, 0.01, 0.01), rough=0.8)

# ---------------- utilitários ----------------
def obj(nome, me, mat=None):
    o = bpy.data.objects.new(nome, me); sc.collection.objects.link(o)
    if mat: o.data.materials.append(mat)
    return o

def suave(o, bev=0.0, seg=3):
    for p in o.data.polygons: p.use_smooth = True
    if bev:
        md = o.modifiers.new('bev', 'BEVEL'); md.width = bev; md.segments = seg
        md.limit_method = 'ANGLE'; md.angle_limit = math.radians(30); md.harden_normals = True
    return o

def perfil(nome, raio_fn, n, z0, z1, mat, bev=0.05):
    """sólido de revolução de topo plano com raio variável no ângulo (canelado)"""
    bm = bmesh.new(); bx, tp = [], []
    for i in range(n):
        a = 2 * math.pi * i / n; r = raio_fn(a)
        bx.append(bm.verts.new((r * math.cos(a), r * math.sin(a), z0)))
        tp.append(bm.verts.new((r * math.cos(a), r * math.sin(a), z1)))
    for i in range(n):
        j = (i + 1) % n; bm.faces.new((bx[i], bx[j], tp[j], tp[i]))
    tampas = [bm.faces.new(tp), bm.faces.new(list(reversed(bx)))]
    bmesh.ops.poke(bm, faces=tampas)  # leque em vez de n-gono: some a emenda no topo
    me = bpy.data.meshes.new(nome); bm.to_mesh(me); bm.free()
    return suave(obj(nome, me, mat), bev)

def cilindro(nome, r, z0, z1, mat, n=96, bev=0.04):
    return perfil(nome, lambda a: r, n, z0, z1, mat, bev)

def caixa(nome, x0, y0, x1, y1, z0, z1, mat, bev=0.0):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1)
    for v in bm.verts:
        v.co = Vector(((x0 + x1) / 2 + v.co.x * (x1 - x0), (y0 + y1) / 2 + v.co.y * (y1 - y0), (z0 + z1) / 2 + v.co.z * (z1 - z0)))
    me = bpy.data.meshes.new(nome); bm.to_mesh(me); bm.free()
    return suave(obj(nome, me, mat), bev, 4)

def texto(s, x, y, tam, mat=M_TINTA, z=0.0, rot=0.0, alin='CENTER', esp=1.0, fonte=None):
    cu = bpy.data.curves.new('t', 'FONT'); cu.body = s; cu.font = fonte or FONTE
    cu.size = tam; cu.align_x = alin; cu.align_y = 'CENTER'; cu.extrude = 0.008; cu.space_character = esp
    o = obj('t_' + s, cu, mat); o.location = (x, y, z + 0.008 + (0.03 if y < -10 else 0)); o.rotation_euler.z = rot
    return o

def esfera(nome, r, x, y, z, mat, achat=1.0):
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=48, v_segments=24, radius=r)
    me = bpy.data.meshes.new(nome); bm.to_mesh(me); bm.free()
    o = suave(obj(nome, me, mat)); o.location = (x, y, z); o.scale.z = achat
    return o

def apontar(o, ang):  # gira em torno do próprio eixo z
    o.rotation_euler.z = ang

# ---------------- painel + caixa de madeira ----------------
PW, PH = 15.0, 20.0
caixa('painel', -PW, -PH, PW, PH, -0.6, 0.0, M_PAINEL, bev=0.03)
F = 2.6  # largura da moldura
caixa('mad_esq', -PW - F, -PH - F, -PW, PH + F, -2.0, 1.1, madeira('mad_v1', 'y'), bev=0.35)
caixa('mad_dir', PW, -PH - F, PW + F, PH + F, -2.0, 1.1, madeira('mad_v2', 'y'), bev=0.35)
caixa('mad_sup', -PW, PH, PW, PH + F, -2.0, 1.1, madeira('mad_h1', 'x'), bev=0.35)
caixa('mad_inf', -PW, -PH - F, PW, -PH, -2.0, 1.1, madeira('mad_h2', 'x'), bev=0.35)

# parafusos de cabeça abaulada com fenda Phillips
for sx in (-1, 1):
    for sy in (-1, 1):
        x, y = sx * (PW - 1.1), sy * (PH - 1.1)
        esfera('paraf', 0.5, x, y, -0.15, M_CROMO, achat=0.45)
        for a in (0.3, 0.3 + math.pi / 2):
            f = caixa('fenda', -0.38, -0.05, 0.38, 0.05, 0.0, 0.1, M_FENDA)
            f.location = (x, y, 0.02); f.rotation_euler.z = a

# ---------------- knob grandão ----------------
KX, KY = 0.0, 3.7
ang_val = math.radians(135 - 27 * VALOR)

def emblema(linhas, tam, z, mat):
    """emblema em relevo (traços arredondados) no centro da tampa"""
    cu = bpy.data.curves.new('emb', 'CURVE'); cu.dimensions = '3D'
    cu.bevel_depth = tam * 0.085; cu.bevel_resolution = 3; cu.use_fill_caps = True
    for ln in linhas:
        sp = cu.splines.new('POLY'); sp.points.add(len(ln) - 1)
        for p, (x, y) in zip(sp.points, ln): p.co = (x * tam, y * tam, z, 1)
    return obj('emblema', cu, mat)

def knob(cx, cy, R, H, ang, flautas=18, escala=1.0, emb=None):
    pai = bpy.data.objects.new('knob', None); sc.collection.objects.link(pai)
    pai.location = (cx, cy, 0); pai.rotation_euler.z = ang; pai.scale = (escala,) * 3
    pecas = []
    # saia usinada com traço indicador gravado
    pecas.append(cilindro('saia', R * 1.22, 0.0, 0.45, M_ALU, bev=0.06))
    pecas.append(caixa('traco_saia', -0.09, R * 1.0, 0.09, R * 1.2, 0.44, 0.47, M_FENDA))
    # corpo canelado (flautas arredondadas, estilo knob de bancada)
    prof = R * 0.045
    pecas.append(perfil('corpo', lambda a: R - prof * (0.5 - 0.5 * math.cos(flautas * a)),
                        flautas * 12, 0.45, H, M_ALU, bev=0.16))
    # tampa preta embutida com linha creme
    pecas.append(cilindro('tampa', R * 0.6, H - 0.15, H + 0.02, M_TAMPA, bev=0.05))
    if emb:  # knob grande: indicador vai para a borda e o emblema ocupa o centro
        pecas.append(caixa('linha', -0.1, R * 0.43, 0.1, R * 0.57, H, H + 0.045, M_TINTA, bev=0.02))
        pecas.append(emblema(emb, R * 0.38, H + 0.04, M_ALU_LISO))
    else:
        pecas.append(caixa('linha', -0.1, R * 0.18, 0.1, R * 0.55, H, H + 0.045, M_TINTA, bev=0.02))
    for p in pecas: p.parent = pai
    return pai

knob(KX, KY, 6.1, 4.2, ang_val, emb=EMBLEMAS[PEDAL])

# lâmpadas tipo "jewel" no lugar de anel de LED
RL = 9.7
for i in range(11):
    a = math.radians(135 - 27 * i)
    x, y = KX - RL * math.sin(a), KY + RL * math.cos(a)
    ligada = i <= VALOR + 1e-6
    cilindro('bezel', 0.62, 0.0, 0.28, M_CROMO, n=48, bev=0.08).location = (x, y, 0)
    esfera('lente', 0.44, x, y, 0.26, M_LAMP_ON if ligada else M_LAMP_OFF, achat=0.62)
    if ligada:
        l = bpy.data.lights.new('brilho', 'POINT'); l.energy = 14; l.color = COR_LAMP; l.shadow_soft_size = 0.4
        lo = bpy.data.objects.new('brilho', l); sc.collection.objects.link(lo); lo.location = (x, y, 0.9)
    rn = RL + 1.75
    texto(str(i), KX - rn * math.sin(a), KY + rn * math.cos(a), 1.1, M_SERIG, fonte=FONTE_B)

# ---------------- textos ----------------
texto('série knob', 0, 18.1, 1.6, M_SERIG, esp=1.05, fonte=FONTE_B)
caixa('filete', -9.5, 16.75, 9.5, 16.8, 0.0, 0.01, M_SERIG)
texto(NOME, 0, -7.0, 2.7, M_SERIG, esp=1.3, fonte=FONTE_B)
texto(SUB, 0, -8.85, 0.85, M_SERIG, esp=1.3, fonte=FONTE_M)

# ---------------- faixa de controles ----------------
caixa('faixa', -13.2, -18.8, 13.2, -10.0, -0.3, 0.02, M_PRETO, bev=0.15)
# parafusos da faixa
for x in (-12.5, 12.5):
    for y in (-18.1, -10.7):
        esfera('pf', 0.22, x, y, 0.0, M_CROMO, achat=0.5)

LY = -11.3  # linha dos rótulos
# TRIGGER: chave alavanca de 3 posições
TX = -8.6
texto('TRIGGER', TX, LY, 0.8, esp=1.2, fonte=FONTE_B)
cilindro('porca', 0.78, 0.02, 0.32, M_CROMO, n=6, bev=0.05).location = (TX, -14.3, 0)
cilindro('bucha', 0.48, 0.32, 0.75, M_CROMO, n=48, bev=0.03).location = (TX, -14.3, 0)
alav = bpy.data.objects.new('alavanca', None); sc.collection.objects.link(alav)
alav.location = (TX, -14.3, 0.7)
if TRIG == 'AUDIO': alav.rotation_euler.y = math.radians(32)
elif TRIG == 'INT': alav.rotation_euler.y = math.radians(-32)
else: alav.rotation_euler.x = math.radians(-32)  # MIDI: para cima
bm = bmesh.new(); bmesh.ops.create_cone(bm, cap_ends=True, segments=40, radius1=0.26, radius2=0.16, depth=2.4)
me = bpy.data.meshes.new('bat'); bm.to_mesh(me); bm.free()
bat = suave(obj('bat', me, M_CROMO)); bat.location = (0, 0, 1.2); bat.parent = alav
ponta = esfera('ponta', 0.3, 0, 0, 2.4, M_CROMO); ponta.parent = alav
for s, x, y in (('INT', TX - 2.6, -14.3), ('MIDI', TX, -12.45), ('ÁUDIO', TX + 2.9, -14.3)):
    texto(s, x, y, 0.62, esp=1.1, fonte=FONTE_M)
texto(ORIGEM, TX, -17.05, 0.7, esp=1.05, fonte=FONTE_M)

# RATE: knob pequeno
RX = -1.9
texto('RATE', RX, LY, 0.8, esp=1.2, fonte=FONTE_B)
knob(RX, -14.4, 1.45, 1.6, math.radians(135 - 27 * 4), flautas=14)
for i in range(7):
    tk = caixa('tick', -0.04, 2.15, 0.04, 2.4, 0, 0.01, M_TINTA)
    tk.location = (RX, -14.4, 0); tk.rotation_euler.z = math.radians(135 - 45 * i)
texto(RATE_TXT + ' · 124 BPM', RX, -17.05, 0.7, esp=1.05, fonte=FONTE_M)

# SHAPE: visor de vidro com a curva acesa
SX = 5.4
texto('SHAPE', SX, LY, 0.8, esp=1.2, fonte=FONTE_B)
caixa('moldura_visor', SX - 3.1, -16.0, SX + 3.1, -12.3, 0.0, 0.12, M_CROMO, bev=0.06)
caixa('visor', SX - 2.85, -15.75, SX + 2.85, -12.55, 0.0, 0.14, M_VIDRO)
cu = bpy.data.curves.new('curva', 'CURVE'); cu.dimensions = '3D'; cu.bevel_depth = 0.02
sp = cu.splines.new('POLY'); pts = []
N = 400  # 2 compassos, mesma forma da prévia 2D
for k in range(N + 1):
    pts.append((SX - 2.6 + 5.2 * k / N, -15.4 + 2.5 * FORMA_FN(2 * k / N * 0.9999)))
sp.points.add(len(pts) - 1)
for p, (x, y) in zip(sp.points, pts): p.co = (x, y, 0.16, 1)
obj('curva', cu, M_TELA)
texto(SHAPE_TXT, SX, -17.05, 0.7, esp=1.05, fonte=FONTE_M)

# MIX
MX = 11.0
texto('MIX', MX, LY, 0.8, esp=1.2, fonte=FONTE_B)
knob(MX, -14.4, 1.25, 1.4, math.radians(135), flautas=12)
texto('100%', MX, -17.05, 0.7, esp=1.05, fonte=FONTE_M)

caixa('fundo', -60, -60, 60, 60, -2.2, -2.1, principled('fundo', (0.015, 0.014, 0.013), rough=0.9))
# ---------------- luz, câmera, render ----------------
w = bpy.data.worlds.new('mundo'); sc.world = w; w.use_nodes = True
bg = w.node_tree.nodes['Background']; bg.inputs['Strength'].default_value = 0.9
hdr = imagem('studio.hdr')
if hdr:
    env = w.node_tree.nodes.new('ShaderNodeTexEnvironment'); env.image = hdr
    mp = w.node_tree.nodes.new('ShaderNodeMapping'); mp.inputs['Rotation'].default_value = (0, 0, math.radians(200))
    tc = w.node_tree.nodes.new('ShaderNodeTexCoord')
    w.node_tree.links.new(tc.outputs['Generated'], mp.inputs['Vector']); w.node_tree.links.new(mp.outputs['Vector'], env.inputs['Vector'])
    w.node_tree.links.new(env.outputs['Color'], bg.inputs['Color'])

sol = bpy.data.lights.new('sol', 'SUN'); sol.energy = 2.6; sol.angle = math.radians(6)
so = bpy.data.objects.new('sol', sol); sc.collection.objects.link(so)
so.rotation_euler = (math.radians(38), 0, math.radians(-40))  # vem de cima-esquerda

# câmera em perspectiva levemente inclinada: mostra a lateral canelada do knob
cam = bpy.data.cameras.new('cam'); cam.lens = 100; cam.sensor_fit = 'VERTICAL'; cam.sensor_height = 36
INC, DIST = math.radians(11), 146
co = bpy.data.objects.new('cam', cam); sc.collection.objects.link(co)
co.location = (0, -DIST * math.sin(INC), DIST * math.cos(INC)); co.rotation_euler = (INC, 0, 0); sc.camera = co

r = sc.render; r.engine = 'CYCLES'
r.resolution_x, r.resolution_y = 1000, int(1000 * (PH + F) / (PW + F)); r.resolution_percentage = ESCALA
sc.cycles.device = 'CPU'; sc.cycles.samples = AMOSTRAS; sc.cycles.use_adaptive_sampling = True
sc.cycles.adaptive_threshold = 0.03; sc.cycles.use_denoising = True; sc.cycles.denoiser = 'OPENIMAGEDENOISE'
sc.cycles.max_bounces = 6
sc.view_settings.view_transform = 'AgX'; sc.view_settings.look = 'AgX - Medium High Contrast'
r.filepath = SAIDA
bpy.ops.render.render(write_still=True)

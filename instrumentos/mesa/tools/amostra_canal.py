# MESA — AMOSTRA de canal mais realista (para comparar com foto de console real antes de refazer as peças do plugin).
# Uso: blender -b --factory-startup --python amostra_canal.py -- --out arquivo.png [--samples 128] [--pct 100] [--tilt 18]
# Diferenças para o render_mesa.py (peças atuais do plugin):
#   - câmera em PERSPECTIVA levemente inclinada (operador sentado), não ortográfica de cima
#   - luz de estúdio com softboxes (sombra de contato de verdade, nada feito em 2D)
#   - teclas com altura (capa sobre moldura), LEDs em domo com lente, legenda serigrafada com espessura
#   - plástico e pintura com variação de brilho (uso, poeira), brilho dos LEDs no compositor
# Reaproveita toda a modelagem do render_mesa.py (executa o arquivo até a lista de peças).
import bpy, bmesh, math, os, sys, random
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
src = open(os.path.join(HERE, 'render_mesa.py'), encoding='utf-8').read()
src = src.split('# ---------------------------------------------------------------- lista de peças')[0]
src = src.replace("os.path.dirname(os.path.abspath(__file__))", repr(HERE))
exec(compile(src, 'render_mesa.py', 'exec'), globals())

OUT = arg('--out', os.path.join(HERE, 'amostra.png'))
PCT = int(arg('--pct', '100'))
TILT = float(arg('--tilt', '18'))
random.seed(11)

# ---------------------------------------------------------------- materiais: uso e variação de brilho
def grime(m, scale=40.0, lo=0.75, hi=1.35, smudge=True):
    """Rugosidade irregular (marca de dedo, poeira) por cima do material existente."""
    nt = m.node_tree; p = nt.nodes['Principled BSDF']
    base = p.inputs['Roughness'].default_value
    if p.inputs['Roughness'].is_linked: return
    tc = nt.nodes.new('ShaderNodeTexCoord')
    n = nt.nodes.new('ShaderNodeTexNoise'); n.inputs['Scale'].default_value = scale; n.inputs['Detail'].default_value = 6; n.inputs['Roughness'].default_value = 0.65
    nt.links.new(tc.outputs['Object'], n.inputs['Vector'])
    r = nt.nodes.new('ShaderNodeMapRange'); r.inputs['From Min'].default_value = 0.35; r.inputs['From Max'].default_value = 0.65
    r.inputs['To Min'].default_value = base * lo; r.inputs['To Max'].default_value = min(1.0, base * hi)
    nt.links.new(n.outputs['Fac'], r.inputs['Value']); nt.links.new(r.outputs['Result'], p.inputs['Roughness'])

def fosco(m, rough, spec=0.3, grain=0.06):
    """Plástico injetado de console: sem verniz, brilho baixo e espalhado, granulado fino do molde."""
    nt = m.node_tree; p = nt.nodes['Principled BSDF']
    p.inputs['Coat Weight'].default_value = 0.0
    p.inputs['Roughness'].default_value = rough
    p.inputs['Specular IOR Level'].default_value = spec
    if not p.inputs['Normal'].is_linked:
        tc = nt.nodes.new('ShaderNodeTexCoord'); n = nt.nodes.new('ShaderNodeTexNoise')
        n.inputs['Scale'].default_value = 900.0; n.inputs['Detail'].default_value = 2
        b = nt.nodes.new('ShaderNodeBump'); b.inputs['Strength'].default_value = grain; b.inputs['Distance'].default_value = 0.002
        nt.links.new(tc.outputs['Object'], n.inputs['Vector'])
        nt.links.new(n.outputs['Fac'], b.inputs['Height']); nt.links.new(b.outputs['Normal'], p.inputs['Normal'])

fosco(M['black'], 0.66); fosco(M['blackm'], 0.72); fosco(M['grey'], 0.6, 0.32); fosco(M['red'], 0.58, 0.32)
for c in CAP.values(): fosco(c, 0.55, 0.32, 0.04)

for k in ('black', 'blackm', 'grey', 'red', 'plate', 'chrome', 'needle'):
    grime(M[k], 25.0)
for c in CAP.values(): grime(c, 30.0, 0.9, 1.2)
for k in ('ssl', 'neve', 'api', 'bridge'):  # pintura: já tem casca-de-laranja; soma desgaste leve na rugosidade
    p = M[k].node_tree.nodes['Principled BSDF']; p.inputs['Coat Weight'].default_value = 0.08; p.inputs['Coat Roughness'].default_value = 0.35
M['legend'].node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = 0.42
M['glass'].node_tree.nodes['Principled BSDF'].inputs['Alpha'].default_value = 0.08

_text = text
def text(*a, **k):  # serigrafia tem espessura e borda: pega luz rasante
    o = _text(*a, **k); o.data.extrude = 0.004; o.data.bevel_depth = 0.0; return o

# ---------------------------------------------------------------- tecla com altura e LED em domo
def keycap(x, y, colr, on, label=None, w=0.62):
    shared_box(('kbz', w), x, y, SURF, w + 0.16, w + 0.16, 0.1, M['blackm'], 0.03)        # moldura no painel
    shared_box(('kwell', w), x, y, SURF + 0.1, w + 0.04, w + 0.04, 0.005, M['slot'], 0.0)  # fresta escura
    zc = SURF + 0.06 + (0.0 if on else 0.08)                                                # apertada fica mais baixa
    shared_box(('kbody', w), x, y, zc, w, w, 0.42, M['black'], 0.05)
    shared_box(('klens', w), x, y, zc + 0.42, w - 0.06, w - 0.06, 0.1, lens(colr, on), 0.045)
    if label:
        text(label, x, y, zc + 0.522, 0.2 if len(label) > 2 else 0.26, M['plate'] if on else mat('lente_txt', '#9a978e', 0.6), space=1.0)

def dome_mesh(name, r):
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=32, v_segments=16, radius=r)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < -1e-4], context='VERTS')
    for v in bm.verts: v.co.z *= 0.75
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free(); return finish(me, 1.2)

def led_dome(x, y, colr, on, r=0.11):
    obj(mesh(('ledbz', r), lambda: cone_mesh('ledbz', r + 0.07, r + 0.05, 0.09, seg=32, bev=0.02)), M['chrome'], (x, y, SURF))
    obj(mesh(('ledd', r), lambda: dome_mesh('ledd', r)), led(colr, on), (x, y, SURF + 0.09))

def vu_lit(cx, cy, z, w, h, val):
    vu(cx, cy, z, w, h, val)
    ld = bpy.data.lights.new('vulamp', 'POINT'); ld.energy = 0.6; ld.color = (1.0, 0.82, 0.55); ld.shadow_soft_size = 0.4
    o = bpy.data.objects.new('vulamp', ld); col.objects.link(o); o.location = (cx, cy - h * 0.35, z + 0.42)

# ---------------------------------------------------------------- canal completo (chapa + peças, como o plugin monta)
def pre_knobs(ox, fam, vals):
    for (k, (x, y, d)), v in zip(L.PRE_KNOBS.items(), vals):
        X, Y = ox + x, y
        if fam == 'api': knob_api(X, Y, SURF, v, d)
        elif fam == 'neve':
            if k == 'gain': knob_marconi(X, Y, SURF, v, d, red=True)
            else: knob_marconi(X, Y, SURF, v, d, cap='org' if k == 'drive' else None)
        else: knob_ssl(X, Y, SURF, v, {'gain': 'gry', 'drive': 'org', 'hpf': 'blk'}[k], d)

def fad_lit(ox, level, fpos, name, dbtxt):
    y0, y1 = L.SEC['fad']
    base(ox, (y0 + y1) / 2, L.SW, y0 - y1)
    module(ox, y0, y1, 'ssl')
    sx = ox + L.FAD_X; yt, yb = L.FAD_YT, L.FAD_YB; Ln = yt - yb
    shared_box('slot', sx, (yt + yb) / 2, SURF - 0.05, 0.24, Ln, 0.056, M['slot'], 0.0)
    for t, fr in L.FAD_MARKS:
        yy = yt - fr * Ln
        shared_box(('ftk', t == '0'), sx - 0.62, yy, SURF, 0.42 if t == '0' else 0.26, 0.035, 0.004, M['legend'], 0.0)
        text(t, sx - 0.95, yy, SURF + 0.003, 0.24, M['legend'], ax='RIGHT')
    for i, mx in enumerate(L.METER_X):
        ledcol(ox + mx, yb, SURF, L.METER_N, Ln / (L.METER_N - 1), level - 0.04 * i, L.METER_W)
    yy = yt - fpos * Ln   # capa do fader (mesma peça do plugin)
    shared_box('fstem', sx, yy, SURF, 0.16, 0.5, 0.4, M['chrome'], 0.0)
    shared_box('fcap', sx, yy, SURF + 0.35, L.FAD_CAP_W, L.FAD_CAP_H, 0.85, M['black'], 0.22)
    shared_box('fcapl', sx, yy, SURF + 1.199, 1.0, 0.09, 0.005, M['white'], 0.0)
    for k in (-1, 1): shared_box('fcapr', sx, yy + k * 0.45, SURF + 1.199, 0.9, 0.05, 0.004, M['blackm'], 0.0)
    x, y, w, h = L.TAPE
    shared_box('tape', ox + x, y, SURF, w, h, 0.012, M['tape'], 0.0, rotz=random.uniform(-0.03, 0.03))
    text(name, ox + x + 0.15, y - 0.02, SURF + 0.03, 0.55, M['ink'], font=FHAND)
    text(dbtxt, ox + L.FAD_DB[0], L.FAD_DB[1], SURF + 0.003, 0.2, M['legend_y'])

def strip(ox, pre_m, eq_m, name, st):
    fam_p = L.PRE_FAM[pre_m]; fam_e = L.EQ_FAM[eq_m]
    # ponte de VU
    box(ox, (L.Y_BR_TOP + L.Y_BR_BOT) / 2, 0, L.SW, L.Y_BR_TOP - L.Y_BR_BOT, 1.4, M['bridge'], 0.04)
    v = L.VU_CH; vu_lit(ox + v['x'], v['y'], 1.4, v['w'], v['h'], st['vu'])
    for (x, y), c, on in ((L.SIG, '#36d16a', True), (L.PEAK, '#ff3a2a', st['peak'])):
        obj(mesh('ledbz2', lambda: cone_mesh('ledbz2', 0.17, 0.15, 0.08, seg=32, bev=0.02)), M['chrome'], (ox + x, y, 1.4))
        obj(mesh('ledd2', lambda: dome_mesh('ledd2', 0.11)), led(c, on), (ox + x, y, 1.48))
    text(str(st['num']), ox, L.NUM_Y, 1.403, 0.32, M['legend'])
    # pré
    pre(pre_m)(ox, 0)
    pre_knobs(ox, fam_p, st['pre'])
    keycap(ox + L.PRE_PAD[0], L.PRE_PAD[1], BCOLS['amb'], st['pad'], None, 0.46)
    # EQ
    eq(eq_m)(ox, 0)
    for k, (pid, a, b, cap) in enumerate(L.EQ_ROWS):
        yy = L.eq_row_y(k); g, f = st['eq'][k]
        if fam_e == 'neve':
            knob_marconi(ox + L.EQ_GAIN_X, yy, SURF, g, L.EQ_GAIN_D, cap=cap); knob_marconi(ox + L.EQ_FREQ_X, yy, SURF, f, L.EQ_FREQ_D, cap=cap)
        else:
            knob_ssl(ox + L.EQ_GAIN_X, yy, SURF, g, cap, L.EQ_GAIN_D); knob_ssl(ox + L.EQ_FREQ_X, yy, SURF, f, cap, L.EQ_FREQ_D)
    keycap(ox + L.EQ_BTN[0], L.EQ_BTN[1], BCOLS['grn'], True, 'EQ')
    keycap(ox + L.DYNPRE_BTN[0], L.DYNPRE_BTN[1], BCOLS['amb'], st['dynpre'], 'PRÉ')
    # dinâmica
    dyn('VCA', ('THR', 'RAT', 'GATE'))(ox, 0)
    for x, v, c in zip(L.DYN_X, st['dyn'], ('gry', 'gry', 'grn')): knob_ssl(ox + x, L.DYN_KNOBS_Y, SURF, v, c, L.DYN_D)
    for k, (x, y) in enumerate(L.GR_LEDS):
        if k < st['gr']: shared_box('grled', ox + x, y, SURF + 0.002, L.GR_LED_W, L.GR_LED_H, 0.061, led('#ffb347' if k < 4 else '#ff3a2a', True), 0.02)
    # saída
    out(ox, 0)
    for k, (x, y, w) in enumerate(L.G_BTNS): keycap(ox + x, y, BCOLS['grn'], st['g'] == k + 1, f'G{k + 1}', w * 0.9)
    knob_ssl(ox + L.PAN[0], L.PAN[1], SURF, st['pan'], 'wht', L.PAN[2])
    keycap(ox + L.MUTE[0], L.MUTE[1], BCOLS['red'], st['mute'], 'M')
    keycap(ox + L.SOLO[0], L.SOLO[1], BCOLS['yel'], False, 'S')
    fad_lit(ox, st['met'], st['fad'], name, st['db'])

BCOLS = dict(amb='#ffb347', grn='#3fbf6a', red='#e8473c', yel='#f2d24b', blu='#3d8bff')
CHS = [
    ('BRIT 73', 'BRIT 81', 'KICK',  dict(num=1, vu=0.72, peak=True,  pre=(0.62, 0.35, 0.2), pad=False, eq=[(0.55, 0.4), (0.45, 0.6), (0.38, 0.3), (0.66, 0.25)], dynpre=False, dyn=(0.4, 0.55, 0.2), gr=3, g=1, pan=0.5, mute=False, met=0.74, fad=0.24, db='-0.4')),
    ('LIMPO',   'E-4K',    'SNARE', dict(num=2, vu=0.58, peak=False, pre=(0.55, 0.2, 0.45), pad=False, eq=[(0.62, 0.7), (0.58, 0.45), (0.42, 0.55), (0.5, 0.5)], dynpre=True, dyn=(0.35, 0.6, 0.45), gr=2, g=1, pan=0.56, mute=False, met=0.62, fad=0.3, db='-2.1')),
    ('API 312', 'E-4K',    'BASS',  dict(num=3, vu=0.5, peak=False, pre=(0.5, 0.6, 0.3), pad=True, eq=[(0.5, 0.5), (0.4, 0.35), (0.6, 0.2), (0.57, 0.3)], dynpre=False, dyn=(0.5, 0.45, 0.1), gr=1, g=2, pan=0.5, mute=False, met=0.55, fad=0.27, db='-1.0')),
    ('BRIT 73', 'BRIT 81', 'VOX',   dict(num=4, vu=0.64, peak=False, pre=(0.7, 0.45, 0.4), pad=False, eq=[(0.6, 0.8), (0.55, 0.6), (0.4, 0.35), (0.45, 0.3)], dynpre=False, dyn=(0.3, 0.7, 0.3), gr=4, g=0, pan=0.5, mute=False, met=0.68, fad=0.2, db='+0.8')),
    ('LIMPO',   'E-4K',    'GTR',   dict(num=5, vu=0.3, peak=False, pre=(0.45, 0.3, 0.5), pad=False, eq=[(0.5, 0.5), (0.5, 0.5), (0.5, 0.5), (0.5, 0.5)], dynpre=False, dyn=(0.5, 0.5, 0.5), gr=0, g=3, pan=0.3, mute=True, met=0.0, fad=0.34, db='-4.5')),
]
X0 = L.WOOD_W / 2 + L.EDGE + L.SW / 2        # 1º canal logo depois da madeira
for i, (p_, e_, nm, st) in enumerate(CHS): strip(X0 + i * L.SW, p_, e_, nm, st)
wood_side(0.0, L.Y_TOP)
XR = X0 + len(CHS) * L.SW
box(XR / 2, L.Y_BR_TOP + L.TOP_FRAME / 2, -1.0, XR + 4, L.TOP_FRAME, 3.2, wood_mat('madeira_h', (0.015, 0.15, 0.15), 'Y'), 0.5)
w_arm = XR + 4
box(XR / 2, -45.0 - L.ARM / 2, -1.6, w_arm, L.ARM + 2, 0.6, M['base'], 0.0)
box(XR / 2, -45.0 - 0.4 - 4.4 / 2, -1.0, w_arm, 4.4, 3.0, M['leather'], 1.1)
box(XR / 2, -22, -4.0, 200, 200, 0.5, mat('mesa_fundo', '#151517', 0.7), 0.0)   # tampo/sala escuro em volta

# ---------------------------------------------------------------- REALISMO: marcas de uso de verdade
# (poeira assentada nas frestas, micro-risco de limpeza, mancha de dedo, serigrafia lascada, estria no knob, madeira a óleo)
def _src(nt, p, nome):
    inp = p.inputs[nome]
    if inp.is_linked: return inp.links[0].from_socket
    if nome == 'Base Color':
        n = nt.nodes.new('ShaderNodeRGB'); n.outputs[0].default_value = inp.default_value; return n.outputs[0]
    n = nt.nodes.new('ShaderNodeValue'); n.outputs[0].default_value = inp.default_value; return n.outputs[0]

def _m(nt, op, a, b=0.0, clamp=False):
    n = nt.nodes.new('ShaderNodeMath'); n.operation = op; n.use_clamp = clamp
    for i, v in enumerate((a, b)):
        if isinstance(v, (int, float)): n.inputs[i].default_value = v
        else: nt.links.new(v, n.inputs[i])
    return n.outputs[0]

def _ruido(nt, vec, escala, det=4, lo=0.0, hi=1.0, a=0.0, b=1.0):
    n = nt.nodes.new('ShaderNodeTexNoise'); n.inputs['Scale'].default_value = escala; n.inputs['Detail'].default_value = det
    nt.links.new(vec, n.inputs['Vector'])
    r = nt.nodes.new('ShaderNodeMapRange'); r.inputs['From Min'].default_value = lo; r.inputs['From Max'].default_value = hi
    r.inputs['To Min'].default_value = a; r.inputs['To Max'].default_value = b; r.clamp = True
    nt.links.new(n.outputs['Fac'], r.inputs['Value']); return r.outputs['Result']

def _risco(nt, vec, ang, escala):
    mp = nt.nodes.new('ShaderNodeMapping'); mp.inputs['Rotation'].default_value = (0, 0, ang)
    mp.inputs['Scale'].default_value = (escala * 0.025, escala, escala * 0.025)   # esticado: vira traço fino
    nt.links.new(vec, mp.inputs['Vector'])
    return _ruido(nt, mp.outputs['Vector'], 1.0, 3, 0.70, 0.76, 0.0, 1.0)

def _mix(nt, fac, a, cor):
    n = nt.nodes.new('ShaderNodeMix'); n.data_type = 'RGBA'; n.blend_type = 'MIX'
    nt.links.new(fac, n.inputs[0]); nt.links.new(a, n.inputs[6]); n.inputs[7].default_value = hexc(cor)
    return n.outputs[2]

def uso(m, poeira=0.3, risco=0.2, mancha=0.12, dist=0.5, var=0.04, cor_poeira='#6f6a62', cor_risco='#8f8a82'):
    nt = m.node_tree; p = nt.nodes['Principled BSDF']
    tc = nt.nodes.new('ShaderNodeTexCoord'); oc = tc.outputs['Object']
    col0 = _src(nt, p, 'Base Color'); r0 = _src(nt, p, 'Roughness')
    ao = nt.nodes.new('ShaderNodeAmbientOcclusion'); ao.samples = 8; ao.inputs['Distance'].default_value = dist
    fresta = _m(nt, 'SUBTRACT', 1.0, ao.outputs['AO'], True)
    irregular = _ruido(nt, oc, 5.0, 8, 0.42, 0.68, 0.2, 1.0)                     # poeira não é véu uniforme
    pont = _ruido(nt, oc, 260.0, 2, 0.74, 0.8, 0.0, 0.5)                         # pontinhos de pó soltos
    pfac = _m(nt, 'MULTIPLY', _m(nt, 'ADD', _m(nt, 'MULTIPLY', fresta, irregular), _m(nt, 'MULTIPLY', pont, irregular)), poeira, True)
    rfac = _m(nt, 'MULTIPLY', _m(nt, 'MAXIMUM', _risco(nt, oc, 0.35, 9.0), _risco(nt, oc, -1.2, 7.0)), risco, True)
    cor = _mix(nt, rfac, _mix(nt, pfac, col0, cor_poeira), cor_risco)
    nt.links.new(cor, p.inputs['Base Color'])
    gord = _ruido(nt, oc, 2.2, 6, 0.38, 0.66, -mancha, mancha)                   # mancha de dedo/gordura
    oi = nt.nodes.new('ShaderNodeObjectInfo')                                    # cada peça é uma peça (lote, desgaste)
    rv = _m(nt, 'MULTIPLY', _m(nt, 'SUBTRACT', oi.outputs['Random'], 0.5), var * 2)
    r = _m(nt, 'ADD', r0, gord); r = _m(nt, 'ADD', r, rv); r = _m(nt, 'ADD', r, _m(nt, 'MULTIPLY', rfac, 0.6))
    r = _m(nt, 'ADD', r, _m(nt, 'MULTIPLY', pfac, 0.6), True)
    nt.links.new(r, p.inputs['Roughness'])

def lascado(m, cor=None, escala=55.0, quanto=0.1):
    """serigrafia gasta: falhas onde a tinta saiu (some e mostra a chapa) + leve amarelado."""
    nt = m.node_tree; p = nt.nodes['Principled BSDF']
    if cor: p.inputs['Base Color'].default_value = hexc(cor)
    tc = nt.nodes.new('ShaderNodeTexCoord')
    a = _ruido(nt, tc.outputs['Object'], escala, 6, 0.78 - quanto * 0.5, 0.80 - quanto * 0.5, 1.0, 0.0)   # só os picos do ruído viram falha
    nt.links.new(a, p.inputs['Alpha'])

def estrias(m, n=48, forca=0.35):
    """estria vertical de pega na lateral dos knobs (só onde a face é lateral)."""
    nt = m.node_tree; p = nt.nodes['Principled BSDF']
    tc = nt.nodes.new('ShaderNodeTexCoord')
    sx = nt.nodes.new('ShaderNodeSeparateXYZ'); nt.links.new(tc.outputs['Object'], sx.inputs[0])
    sn = nt.nodes.new('ShaderNodeSeparateXYZ'); nt.links.new(tc.outputs['Normal'], sn.inputs[0])
    ang = _m(nt, 'ARCTAN2', sx.outputs['Y'], sx.outputs['X'])
    onda = _m(nt, 'ABSOLUTE', _m(nt, 'SINE', _m(nt, 'MULTIPLY', ang, n / 2)))
    lado = _m(nt, 'MULTIPLY', _m(nt, 'SUBTRACT', 0.55, _m(nt, 'ABSOLUTE', sn.outputs['Z'])), 4.0, True)
    b = nt.nodes.new('ShaderNodeBump'); b.inputs['Strength'].default_value = forca; b.inputs['Distance'].default_value = 0.01
    nt.links.new(_m(nt, 'MULTIPLY', onda, lado), b.inputs['Height'])
    if p.inputs['Normal'].is_linked: nt.links.new(p.inputs['Normal'].links[0].from_socket, b.inputs['Normal'])
    nt.links.new(b.outputs['Normal'], p.inputs['Normal'])

def oleo(m):
    """madeira a óleo (não verniz de piano): pouco coat, poro aberto, desgaste de braço."""
    nt = m.node_tree; p = nt.nodes['Principled BSDF']
    p.inputs['Coat Weight'].default_value = 0.12; p.inputs['Coat Roughness'].default_value = 0.4; p.inputs['Roughness'].default_value = 0.55
    tc = nt.nodes.new('ShaderNodeTexCoord'); mp = nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = (60, 2.5, 60) if 'h' not in m.name[-2:] else (2.5, 60, 60)
    nt.links.new(tc.outputs['Object'], mp.inputs['Vector'])
    poro = _ruido(nt, mp.outputs['Vector'], 1.0, 4, 0.6, 0.75, 0.0, 1.0)
    b = nt.nodes.new('ShaderNodeBump'); b.inputs['Strength'].default_value = 0.08; b.inputs['Distance'].default_value = 0.01; b.invert = True
    nt.links.new(poro, b.inputs['Height'])
    if p.inputs['Normal'].is_linked: nt.links.new(p.inputs['Normal'].links[0].from_socket, b.inputs['Normal'])
    nt.links.new(b.outputs['Normal'], p.inputs['Normal'])

estrias(M['black'])
for mm in list(bpy.data.materials):
    nm_ = mm.name
    if nm_.startswith('madeira'): oleo(mm); uso(mm, 0.05, 0.12, 0.06, 1.0, 0.02, '#3a2414', '#5a3a22')
    elif nm_.startswith(('painel_', 'ponte', 'base')): uso(mm, 0.2, 0.14, 0.04, 0.6, 0.02)
    elif nm_.startswith(('plast_', 'marconi', 'vermelho', 'cap_')): uso(mm, 0.22, 0.08, 0.08, 0.35, 0.06)
    elif nm_.startswith(('cromo', 'couro', 'fenda')): uso(mm, 0.18, 0.15, 0.06, 0.4, 0.03)
    elif nm_.startswith('placa'): uso(mm, 0.08, 0.06, 0.04, 0.3, 0.02)
    elif nm_.startswith('fita_crepe'): uso(mm, 0.15, 0.0, 0.05, 0.3, 0.05, '#9a8f74')
lascado(M['legend'], '#e0dacb', 55.0, 0.12); lascado(M['legend_y'], '#e9b94c', 55.0, 0.1)
lascado(M['white'], None, 40.0, 0.08)

# ---------------------------------------------------------------- luz de estúdio (softboxes) + HDRI fraco
w = bpy.data.worlds.new('w'); sc.world = w; w.use_nodes = True; nt = w.node_tree
env = nt.nodes.new('ShaderNodeTexEnvironment'); env.image = bpy.data.images.load(HDRI)
mp = nt.nodes.new('ShaderNodeMapping'); tc = nt.nodes.new('ShaderNodeTexCoord'); mp.inputs['Rotation'].default_value = (0, 0, math.radians(120))
nt.links.new(tc.outputs['Generated'], mp.inputs['Vector']); nt.links.new(mp.outputs['Vector'], env.inputs['Vector'])
nt.links.new(env.outputs['Color'], nt.nodes['Background'].inputs['Color']); nt.nodes['Background'].inputs['Strength'].default_value = 0.4

CX, CY = X0 + 1.6 * L.SW, -21.5
def area(name, loc, size, power, color=(1, 0.96, 0.9), shape='RECTANGLE', sy=None):
    ld = bpy.data.lights.new(name, 'AREA'); ld.shape = shape; ld.size = size; ld.size_y = sy or size; ld.energy = power; ld.color = color
    o = bpy.data.objects.new(name, ld); col.objects.link(o); o.location = loc
    o.rotation_euler = (Vector((CX, CY, 0)) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
area('key', (CX - 35, CY + 55, 95), 28, 1.7e5, sy=60)                    # softbox alto atrás/esquerda: sombra curta para baixo-direita
area('fill', (CX + 55, CY - 70, 60), 70, 2.2e4, (0.86, 0.9, 1.0))         # rebatedor frio pelo lado do operador
area('rim', (CX + 10, CY + 110, 22), 30, 4.0e4, sy=8)                     # tira atrás: borda dos knobs acende
area('strip', (CX - 70, CY - 10, 25), 8, 1.4e4, (1.0, 0.92, 0.82), sy=60)  # lateral rasante: relevo da pintura e serigrafia

# ---------------------------------------------------------------- câmera do operador, levemente inclinada
cam = bpy.data.cameras.new('cam'); co = bpy.data.objects.new('cam', cam); col.objects.link(co); sc.camera = co
cam.sensor_width = 36; cam.sensor_fit = 'AUTO'; cam.lens = 90
XL_, XR_ = -L.WOOD_W / 2 - 0.2, XR - 0.9                                    # o 5º canal sai pela borda (é uma console maior)
YT_, YB_ = L.Y_BR_TOP + L.TOP_FRAME + 0.3, -45.0 - L.ARM + 0.6
W_, H_ = XR_ - XL_, (YT_ - YB_) * math.cos(math.radians(TILT)) + 2.0
dist = max(H_, W_) * cam.lens / 36 * 1.0
t = math.radians(TILT); tgt = Vector(((XL_ + XR_) / 2, (YT_ + YB_) / 2, 1.0))
co.location = tgt + Vector((0, -math.sin(t), math.cos(t))) * dist
co.rotation_euler = (tgt - co.location).to_track_quat('-Z', 'Y').to_euler()
cam.clip_end = dist * 4
cam.dof.use_dof = True; cam.dof.focus_distance = dist; cam.dof.aperture_fstop = float(arg('--fstop', '6.3'))   # foco cai um pouco nas pontas, como foto
RX = int(arg("--rx", "1500")); sc.render.resolution_x, sc.render.resolution_y = RX, int(RX * H_ / W_)

sc.render.engine = 'CYCLES'; cy = sc.cycles
cy.device = 'CPU'; cy.samples = SAMPLES; cy.use_adaptive_sampling = True; cy.adaptive_threshold = 0.012
cy.use_denoising = True; cy.denoiser = 'OPENIMAGEDENOISE'
cy.max_bounces = 8; cy.glossy_bounces = 4; cy.transmission_bounces = 4; cy.caustics_reflective = False; cy.caustics_refractive = False
sc.render.resolution_percentage = PCT
sc.view_settings.view_transform = 'Khronos PBR Neutral'; sc.view_settings.look = 'None'

# brilho dos LEDs/lentes acesos (compositor)
sc.use_nodes = True; ct = sc.node_tree
rl = ct.nodes['Render Layers']; cmp_ = ct.nodes['Composite']
gl = ct.nodes.new('CompositorNodeGlare'); gl.glare_type = 'FOG_GLOW'
try: gl.quality = 'HIGH'
except Exception: pass
for nm, v in (('threshold', 0.95), ('size', 7)):
    if hasattr(gl, nm): setattr(gl, nm, v)
for nm, v in (('Threshold', 0.95), ('Size', 0.45), ('Strength', 0.6)):
    if nm in gl.inputs: gl.inputs[nm].default_value = v
ct.links.new(rl.outputs['Image'], gl.inputs['Image']); ct.links.new(gl.outputs['Image'], cmp_.inputs['Image'])

sc.render.image_settings.file_format = 'PNG'; sc.render.filepath = OUT
print('OBJETOS', len(bpy.data.objects), 'RES', sc.render.resolution_x, sc.render.resolution_y, 'TILT', TILT)
bpy.ops.render.render(write_still=True)
print('RENDER OK', OUT)

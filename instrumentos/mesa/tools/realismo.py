# MESA — acabamento "de equipamento real" aprovado na amostra de 30/10 (tools/amostra_canal.py -> real.png).
# Não roda sozinho: é executado (exec) depois da modelagem do render_mesa.py, no mesmo namespace.
#   materiais_base()  plástico fosco sem verniz (granulado de molde), serigrafia lascada, estria no knob preto
#   materiais_uso()   poeira assentada nas frestas, micro-risco, mancha de dedo, madeira a óleo — chamar DEPOIS de
#                     montar a cena (pega também os materiais criados no caminho, como a madeira de cima)
#   keycap(), led_dome(), vu_lit()  peças com altura (tecla sobre moldura, LED em domo com aro, VU com lâmpada)
import bpy, bmesh, math
from mathutils import Vector

# ---------------------------------------------------------------- plástico e pintura
def grime(m, scale=40.0, lo=0.75, hi=1.35, smudge=True):
    """Rugosidade irregular (marca de dedo, poeira) por cima do material existente."""
    nt = m.node_tree; p = nt.nodes['Principled BSDF']
    base_ = p.inputs['Roughness'].default_value
    if p.inputs['Roughness'].is_linked: return
    tc = nt.nodes.new('ShaderNodeTexCoord')
    n = nt.nodes.new('ShaderNodeTexNoise'); n.inputs['Scale'].default_value = scale; n.inputs['Detail'].default_value = 6; n.inputs['Roughness'].default_value = 0.65
    nt.links.new(tc.outputs['Object'], n.inputs['Vector'])
    r = nt.nodes.new('ShaderNodeMapRange'); r.inputs['From Min'].default_value = 0.35; r.inputs['From Max'].default_value = 0.65
    r.inputs['To Min'].default_value = base_ * lo; r.inputs['To Max'].default_value = min(1.0, base_ * hi)
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

# ---------------------------------------------------------------- marcas de uso de verdade
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

def materiais_base():
    fosco(M['black'], 0.66); fosco(M['blackm'], 0.72); fosco(M['grey'], 0.6, 0.32); fosco(M['red'], 0.58, 0.32)
    for c in CAP.values(): fosco(c, 0.55, 0.32, 0.04)
    for k in ('black', 'blackm', 'grey', 'red', 'plate', 'chrome', 'needle'):
        grime(M[k], 25.0)
    for c in CAP.values(): grime(c, 30.0, 0.9, 1.2)
    for k in ('ssl', 'neve', 'api', 'bridge'):  # pintura: já tem casca-de-laranja; leve desgaste na rugosidade
        p = M[k].node_tree.nodes['Principled BSDF']; p.inputs['Coat Weight'].default_value = 0.08; p.inputs['Coat Roughness'].default_value = 0.35
    M['legend'].node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = 0.42
    M['glass'].node_tree.nodes['Principled BSDF'].inputs['Alpha'].default_value = 0.08
    estrias(M['black'])
    lascado(M['legend'], '#e0dacb', 55.0, 0.12); lascado(M['legend_y'], '#e9b94c', 55.0, 0.1)
    lascado(M['white'], None, 40.0, 0.08)

_USADOS = set()
def materiais_uso():
    for mm in list(bpy.data.materials):
        nm_ = mm.name
        if nm_ in _USADOS: continue
        _USADOS.add(nm_)
        if nm_.startswith('madeira'): oleo(mm); uso(mm, 0.05, 0.12, 0.06, 1.0, 0.02, '#3a2414', '#5a3a22')
        elif nm_.startswith(('painel_', 'ponte', 'base')): uso(mm, 0.2, 0.14, 0.04, 0.6, 0.02)
        elif nm_.startswith(('plast_', 'marconi', 'vermelho', 'cap_')): uso(mm, 0.22, 0.08, 0.08, 0.35, 0.06)
        elif nm_.startswith(('cromo', 'couro', 'fenda')): uso(mm, 0.18, 0.15, 0.06, 0.4, 0.03)
        elif nm_.startswith('placa'): uso(mm, 0.08, 0.06, 0.04, 0.3, 0.02)
        elif nm_.startswith('fita_crepe'): uso(mm, 0.15, 0.0, 0.05, 0.3, 0.05, '#9a8f74')

_text0 = text
def text(*a, **k):  # serigrafia tem espessura e borda: pega luz rasante
    o = _text0(*a, **k); o.data.extrude = 0.004; o.data.bevel_depth = 0.0; return o

# ---------------------------------------------------------------- tecla com altura e LED em domo
KEY_UP, KEY_DOWN = 0.14, 0.06          # altura da base da tecla acima da chapa: solta / apertada
KEY_TOP = 0.522                         # do pé da tecla ao topo da lente

def keycap(x, y, colr, on, label=None, w=0.62, z0=None):
    z0 = SURF if z0 is None else z0
    shared_box(('kbz', w), x, y, z0, w + 0.16, w + 0.16, 0.1, M['blackm'], 0.03)        # moldura no painel
    shared_box(('kwell', w), x, y, z0 + 0.1, w + 0.04, w + 0.04, 0.005, M['slot'], 0.0)  # fresta escura
    zc = z0 + (KEY_DOWN if on else KEY_UP)                                                # apertada fica mais baixa
    shared_box(('kbody', w), x, y, zc, w, w, 0.42, M['black'], 0.05)
    shared_box(('klens', w), x, y, zc + 0.42, w - 0.06, w - 0.06, 0.1, lens(colr, on), 0.045)
    if label:
        text(label, x, y, zc + KEY_TOP, 0.2 if len(label) > 2 else 0.26, M['plate'] if on else mat('lente_txt', '#9a978e', 0.6), space=1.0)

def dome_mesh(name, r):
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=32, v_segments=16, radius=r)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < -1e-4], context='VERTS')
    for v in bm.verts: v.co.z *= 0.75
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free(); return finish(me, 1.2)

def led_dome(x, y, z, colr, on, r=0.11):
    obj(mesh(('ledbz', r), lambda: cone_mesh('ledbz', r + 0.06, r + 0.04, 0.08, seg=32, bev=0.02)), M['chrome'], (x, y, z))
    obj(mesh(('ledd', r), lambda: dome_mesh('ledd', r)), led(colr, on), (x, y, z + 0.08))

def vu_lamp(cx, cy, z, h):
    ld = bpy.data.lights.new('vulamp', 'POINT'); ld.energy = 0.6 * (h / 2.5) ** 2; ld.color = (1.0, 0.82, 0.55); ld.shadow_soft_size = 0.4 * h / 2.5
    o = bpy.data.objects.new('vulamp', ld); col.objects.link(o); o.location = (cx, cy - h * 0.35, z + 0.42)

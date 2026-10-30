# MESA — peças do plugin renderizadas no Blender (4.5, Cycles CPU), com a mesma modelagem do render aprovado.
# Uso: blender -b --factory-startup --python render_mesa.py -- --part chapas|pecas --out <pasta> [--nome x] [--ppc 64]
#        [--samples 64] [--so prefixo,prefixo] [--sem prefixo] [--tilt 18]
#   chapas: os módulos sem knob/botão (opacos), a madeira e o couro          -> <nome>.png + <nome>.json
#   pecas : knobs (48 posições), teclas soltas/apertadas, LEDs acesos, faders (alfa + sombra) -> idem
# Câmera ortográfica inclinada (ver "lista de peças"): 1 cm = PPC pixels; luz igual em todo lugar, para a chapa e
# a peça casarem em qualquer posição da tela. O acabamento de equipamento real vem do realismo.py.
import bpy, bmesh, math, sys, random, os, json
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import layout as L

argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
def arg(n, d):
    return argv[argv.index(n) + 1] if n in argv else d
PART, OUTDIR, SAMPLES = arg('--part', 'chapas'), arg('--out', '.'), int(arg('--samples', '64'))
ONLY = [s for s in arg('--so', '').split(',') if s]
PPC = float(arg('--ppc', '40'))
HDRI = 'D:/Apps/_tools/hdri/studio.hdr'
FONT_UI = 'C:/Windows/Fonts/arialbd.ttf'
FONT_LOGO = 'C:/Windows/Fonts/framd.ttf'
FONT_HAND = 'C:/Windows/Fonts/segoepr.ttf'
random.seed(7)

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
sc.unit_settings.scale_length = 0.01
col = sc.collection
FUI = bpy.data.fonts.load(FONT_UI)
FHAND = bpy.data.fonts.load(FONT_HAND)
FLOGO = bpy.data.fonts.load(FONT_LOGO)

# ---------------------------------------------------------------- materiais
def hexc(h, a=1.0):
    h = h.lstrip('#'); c = [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c] + [a]

MATS = {}
def mat(name, color, rough=0.5, metal=0.0, emit=None, estr=0.0, bump=None, coat=0.0, alpha=1.0, spec=0.5):
    if name in MATS: return MATS[name]
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; p = nt.nodes['Principled BSDF']
    p.inputs['Base Color'].default_value = hexc(color)
    p.inputs['Roughness'].default_value = rough
    p.inputs['Metallic'].default_value = metal
    p.inputs['Specular IOR Level'].default_value = spec
    p.inputs['Coat Weight'].default_value = coat
    p.inputs['Alpha'].default_value = alpha
    if emit:
        p.inputs['Emission Color'].default_value = hexc(emit)
        p.inputs['Emission Strength'].default_value = estr
    if bump:  # (escala do ruído, força) — pintura casca-de-laranja, martelado, couro
        sc_, st = bump
        tc = nt.nodes.new('ShaderNodeTexCoord'); n = nt.nodes.new('ShaderNodeTexNoise')
        n.inputs['Scale'].default_value = sc_; n.inputs['Detail'].default_value = 8
        b = nt.nodes.new('ShaderNodeBump'); b.inputs['Strength'].default_value = st; b.inputs['Distance'].default_value = 0.02
        nt.links.new(tc.outputs['Object'], n.inputs['Vector'])
        nt.links.new(n.outputs['Fac'], b.inputs['Height']); nt.links.new(b.outputs['Normal'], p.inputs['Normal'])
        # variação sutil de rugosidade
        r = nt.nodes.new('ShaderNodeMapRange'); r.inputs['To Min'].default_value = rough * 0.85; r.inputs['To Max'].default_value = min(1, rough * 1.15)
        nt.links.new(n.outputs['Fac'], r.inputs['Value']); nt.links.new(r.outputs['Result'], p.inputs['Roughness'])
    MATS[name] = m
    return m

def wood_mat(name='madeira', esc=(0.15, 0.015, 0.15), dire='X'):  # veio ao longo do eixo com escala pequena
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; p = nt.nodes['Principled BSDF']
    tc = nt.nodes.new('ShaderNodeTexCoord'); mp = nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = esc
    w = nt.nodes.new('ShaderNodeTexWave'); w.inputs['Scale'].default_value = 2.2; w.inputs['Distortion'].default_value = 9; w.inputs['Detail'].default_value = 6
    w.wave_type = 'BANDS'; w.bands_direction = dire
    ramp = nt.nodes.new('ShaderNodeValToRGB')
    ramp.color_ramp.elements[0].color = hexc('#2a160b'); ramp.color_ramp.elements[1].color = hexc('#6b3b1c')
    nt.links.new(tc.outputs['Object'], mp.inputs['Vector']); nt.links.new(mp.outputs['Vector'], w.inputs['Vector'])
    nt.links.new(w.outputs['Fac'], ramp.inputs['Fac']); nt.links.new(ramp.outputs['Color'], p.inputs['Base Color'])
    p.inputs['Roughness'].default_value = 0.32; p.inputs['Coat Weight'].default_value = 0.6; p.inputs['Coat Roughness'].default_value = 0.12
    b = nt.nodes.new('ShaderNodeBump'); b.inputs['Strength'].default_value = 0.08
    nt.links.new(w.outputs['Fac'], b.inputs['Height']); nt.links.new(b.outputs['Normal'], p.inputs['Normal'])
    return m

M = dict(
    base=mat('base', '#0b0b0c', 0.6),
    ssl=mat('painel_ssl', '#3b3e44', 0.5, 0.25, bump=(180, 0.12)),
    neve=mat('painel_neve', '#4a5f70', 0.48, 0.2, bump=(160, 0.12)),
    api=mat('painel_api', '#1d2026', 0.42, 0.3, bump=(200, 0.08)),
    valv=mat('painel_610', '#4a4d51', 0.55, 0.35, bump=(35, 0.45)),
    pult=mat('painel_pult', '#55697a', 0.5, 0.35, bump=(35, 0.45)),
    midas=mat('painel_midas', '#1c2945', 0.45, 0.25, bump=(180, 0.1)),
    bridge=mat('ponte', '#18191c', 0.55, 0.15, bump=(150, 0.1)),
    legend=mat('legenda', '#ece8dc', 0.6),
    legend_y=mat('legenda_am', '#f2c14e', 0.55),
    plate=mat('placa', '#0c0c0d', 0.3),
    black=mat('plast_preto', '#0e0e0f', 0.32, coat=0.2),
    blackm=mat('plast_preto_fosco', '#121213', 0.55),
    grey=mat('marconi_cinza', '#8b9096', 0.33, 0.1, coat=0.3),
    red=mat('vermelho_gain', '#a3191a', 0.22, coat=0.5),
    white=mat('linha_branca', '#f2f0ea', 0.4),
    chrome=mat('cromo', '#d0d0d0', 0.22, 1.0),
    slot=mat('fenda', '#030303', 0.6),
    leather=mat('couro', '#121212', 0.62, bump=(9, 0.35)),
    tape=mat('fita_crepe', '#e3d7b6', 0.85, bump=(60, 0.2)),
    ink=mat('caneta', '#16182a', 0.7),
    vuface=mat('vu_face', '#f0e1b0', 0.7, emit='#ffd98a', estr=1.1),
    vuink=mat('vu_tinta', '#2a2214', 0.7),
    vured=mat('vu_verm', '#b3261e', 0.6),
    needle=mat('agulha', '#101010', 0.4),
    glass=mat('vidro', '#000000', 0.04, alpha=0.12, spec=1.0),
    wood=wood_mat(),
)
CAP = {k: mat('cap_' + k, c, 0.22, coat=0.6) for k, c in dict(
    red='#c3261e', grn='#23874a', blu='#2457b5', brn='#6a4325', gry='#c9c9c4', yel='#e0b12a', wht='#e8e6df', blk='#1b1b1c', org='#e07a1c').items()}
def dark(col, f):  # cor do plástico/lente apagado: mesma tinta, bem mais escura
    h = col.lstrip('#'); return '#' + ''.join(f'{int(int(h[i:i + 2], 16) * f):02x}' for i in (0, 2, 4))
def lens(col, on):
    return mat(f'lente_{col}_on', col, 0.15, emit=col, estr=1.6, coat=0.8) if on else mat(f'lente_{col}_off', dark(col, 0.32), 0.2, coat=0.8)
def led(col, on):
    return mat(f'led_{col}_on', col, 0.2, emit=col, estr=2.2, coat=0.5) if on else mat(f'led_{col}_off', dark(col, 0.16), 0.25, coat=0.6)

# ---------------------------------------------------------------- geometria base (malhas compartilhadas)
def finish(me, sharp=0.55):
    for f in me.polygons: f.use_smooth = True
    me.set_sharp_from_angle(angle=sharp)
    return me

def cone_mesh(name, r_bot, r_top, h, seg=64, bev=0.04, bseg=3):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=seg, radius1=r_bot, radius2=r_top, depth=h)
    bmesh.ops.translate(bm, vec=(0, 0, h / 2), verts=bm.verts)
    if bev > 0:
        es = [e for e in bm.edges if abs(e.verts[0].co.z - e.verts[1].co.z) < 1e-5 and len(e.link_faces) == 2]
        bmesh.ops.bevel(bm, geom=es, offset=bev, segments=bseg, affect='EDGES', profile=0.5)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    return finish(me)

def box_mesh(name, sx, sy, sz, bev=0.05, bseg=3, taper=None):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1)
    for v in bm.verts:
        v.co.x *= sx; v.co.y *= sy; v.co.z = (v.co.z + 0.5) * sz
        if taper and v.co.y > 0: v.co.x *= taper
    if bev > 0:
        bmesh.ops.bevel(bm, geom=list(bm.edges), offset=min(bev, sx / 2.1, sy / 2.1, sz / 2.1), segments=bseg, affect='EDGES', profile=0.5)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    return finish(me)

MESH = {}
def mesh(key, fn):
    if key not in MESH: MESH[key] = fn()
    return MESH[key]

def obj(me, m, loc, rotz=0.0, scale=1.0):
    o = bpy.data.objects.new(me.name, me); col.objects.link(o)
    o.location = loc; o.rotation_euler = (0, 0, rotz); o.scale = (scale, scale, scale)
    if not o.data.materials: o.data.materials.append(m)
    else: o.material_slots[0].link = 'OBJECT'; o.material_slots[0].material = m
    return o

def box(x, y, z, sx, sy, sz, m, bev=0.05, rotz=0.0):
    me = box_mesh(f'box{len(bpy.data.meshes)}', sx, sy, sz, bev)
    me.materials.append(m)
    o = bpy.data.objects.new('box', me); col.objects.link(o); o.location = (x, y, z); o.rotation_euler = (0, 0, rotz)
    return o

def shared_box(key, x, y, z, sx, sy, sz, m, bev=0.03, rotz=0.0):
    me = mesh(('box', key, sx, sy, sz, bev), lambda: box_mesh(f'sb_{key}', sx, sy, sz, bev))
    return obj(me, m, (x, y, z), rotz)

def text(s, x, y, z, size, m, font=None, ax='CENTER', ay='CENTER', rotz=0.0, space=1.0, bold=0.0):
    cu = bpy.data.curves.new('t', 'FONT'); cu.body = s; cu.font = font or FUI; cu.size = size
    cu.align_x = ax; cu.align_y = ay; cu.space_character = space; cu.offset = bold
    o = bpy.data.objects.new('t', cu); col.objects.link(o); o.location = (x, y, z); o.rotation_euler = (0, 0, rotz)
    cu.materials.append(m)
    return o

def rot_of(v):  # 0..1 -> rotação (ponteiro em +Y = meio-dia), curso de 300°
    return -math.radians(-150 + v * 300)

# ---------------------------------------------------------------- knobs fiéis por família
def knob_ssl(x, y, z, v, cap='red', d=1.5):
    """Botão de console britânica: corpo preto, saia com marca, tampa colorida brilhante e traço branco."""
    s = d / 1.5; r = rot_of(v)
    obj(mesh('ssl_skirt', lambda: cone_mesh('ssl_skirt', 0.8, 0.76, 0.14, bev=0.03)), M['black'], (x, y, z), r, s)
    obj(mesh('ssl_body', lambda: cone_mesh('ssl_body', 0.6, 0.53, 0.95, bev=0.07)), M['black'], (x, y, z + 0.14 * s), r, s)
    obj(mesh('ssl_cap', lambda: cone_mesh('ssl_cap', 0.47, 0.45, 0.07, bev=0.025)), CAP[cap], (x, y, z + 1.06 * s), r, s)
    o = obj(mesh('ssl_line', lambda: box_mesh('ssl_line', 0.07, 0.42, 0.02, 0.01)), M['white'], (x, y, z + 1.12 * s), r, s)
    o2 = obj(mesh('ssl_skline', lambda: box_mesh('ssl_skline', 0.06, 0.16, 0.01, 0.005)), M['white'], (x, y, z + 0.14 * s), r, s)
    o2.location += Vector((-math.sin(r) * 0.69 * s, math.cos(r) * 0.69 * s, 0))
    o.location = Vector((x - math.sin(r) * 0.22 * s, y + math.cos(r) * 0.22 * s, z + 1.12 * s))

def knob_marconi(x, y, z, v, d=1.5, red=False, cap=None):
    """Botão estilo módulo britânico clássico: cinza abaulado; o de ganho tem saia vermelha grande."""
    s = d / 1.5; r = rot_of(v)
    if red:
        obj(mesh('mar_red', lambda: cone_mesh('mar_red', 1.0, 0.9, 0.42, seg=48, bev=0.1)), M['red'], (x, y, z), r, s)
        z2 = z + 0.42 * s; sb = 0.8
    else:
        z2 = z; sb = 1.0
    obj(mesh('mar_body', lambda: cone_mesh('mar_body', 0.66, 0.5, 0.8, bev=0.16, bseg=5)), M['grey'], (x, y, z2), r, s * sb)
    if cap:
        obj(mesh('mar_cap', lambda: cone_mesh('mar_cap', 0.2, 0.2, 0.04, bev=0.01)), CAP[cap], (x, y, z2 + 0.8 * s * sb), r, s * sb)
    o = obj(mesh('mar_line', lambda: box_mesh('mar_line', 0.06, 0.3, 0.02, 0.008)), M['white'], (0, 0, 0), r, s * sb)
    o.location = Vector((x - math.sin(r) * 0.36 * s * sb, y + math.cos(r) * 0.36 * s * sb, z2 + 0.79 * s * sb))

def knob_chicken(x, y, z, v, d=1.5):
    """Bico-de-galinha de baquelite (pré valvulado / EQ passivo)."""
    s = d / 1.5; r = rot_of(v)
    obj(mesh('ch_base', lambda: cone_mesh('ch_base', 0.62, 0.56, 0.32, bev=0.05)), M['black'], (x, y, z), r, s)
    o = obj(mesh('ch_ptr', lambda: box_mesh('ch_ptr', 0.5, 1.7, 0.6, 0.12, 4, taper=0.38)), M['black'], (x, y, z + 0.3 * s), r, s)
    o = obj(mesh('ch_line', lambda: box_mesh('ch_line', 0.06, 0.55, 0.02, 0.008)), M['white'], (0, 0, 0), r, s)
    o.location = Vector((x - math.sin(r) * 0.5 * s, y + math.cos(r) * 0.5 * s, z + 0.905 * s))

def knob_api(x, y, z, v, d=1.5):
    """Botão preto fino com saia prateada (pré americano)."""
    s = d / 1.5; r = rot_of(v)
    obj(mesh('api_skirt', lambda: cone_mesh('api_skirt', 0.74, 0.7, 0.12, bev=0.03)), M['chrome'], (x, y, z), r, s)
    obj(mesh('api_body', lambda: cone_mesh('api_body', 0.52, 0.48, 0.85, seg=24, bev=0.05)), M['black'], (x, y, z + 0.12 * s), r, s)
    o = obj(mesh('api_line', lambda: box_mesh('api_line', 0.06, 0.44, 0.02, 0.008)), M['white'], (0, 0, 0), r, s)
    o.location = Vector((x - math.sin(r) * 0.22 * s, y + math.cos(r) * 0.22 * s, z + 0.975 * s))

def knob_midas(x, y, z, v, cap='red', d=1.5):
    """Botão colorido inteiriço com topo cinza (console de PA britânica)."""
    s = d / 1.5; r = rot_of(v)
    obj(mesh('mid_body', lambda: cone_mesh('mid_body', 0.62, 0.5, 0.85, seg=40, bev=0.06)), CAP[cap], (x, y, z), r, s)
    obj(mesh('mid_top', lambda: cone_mesh('mid_top', 0.42, 0.42, 0.05, bev=0.02)), M['blackm'], (x, y, z + 0.85 * s), r, s)
    o = obj(mesh('mid_line', lambda: box_mesh('mid_line', 0.06, 0.38, 0.02, 0.008)), M['white'], (0, 0, 0), r, s)
    o.location = Vector((x - math.sin(r) * 0.2 * s, y + math.cos(r) * 0.2 * s, z + 0.9 * s))

def scale_marks(x, y, z, d, n=11, m=None):
    """Marcas de escala impressas no painel em volta do knob."""
    R = d * 0.5 + 0.16
    for i in range(n):
        a = math.radians(-150 + i * 300 / (n - 1))
        shared_box('tick', x + math.sin(a) * R, y + math.cos(a) * R, z, 0.035, 0.14, 0.004, m or M['legend'], 0.0, rotz=-a)

def button(x, y, z, colr, on, label=None, w=0.72):
    shared_box(('btnb', w), x, y, z, w, w, 0.22, M['black'], 0.06)
    shared_box(('btnl', w), x, y, z + 0.22, w - 0.12, w - 0.12, 0.07, lens(colr, on), 0.03)
    if label: text(label, x, y, z + 0.30, 0.22 if len(label) > 2 else 0.28, M['plate'] if on else mat('lente_txt', '#2a2a2a', 0.5), space=1.0)

def screw(x, y, z):
    obj(mesh('screw', lambda: cone_mesh('screw', 0.14, 0.12, 0.06, seg=24, bev=0.03)), M['chrome'], (x, y, z), random.random() * 3)
    r = random.random() * 3
    shared_box('slotx', x, y, z + 0.055, 0.2, 0.03, 0.02, M['slot'], 0.0, rotz=r)
    shared_box('slotx', x, y, z + 0.055, 0.2, 0.03, 0.02, M['slot'], 0.0, rotz=r + 1.5708)

def plate(x, y, z, s, w=2.9):
    """Plaquinha preta com o nome do modelo (é o seletor de modelo no plugin)."""
    shared_box(('plate', w), x, y, z, w, 0.62, 0.08, M['plate'], 0.04)
    text(s, x, y, z + 0.085, 0.44, M['legend_y'], space=1.1)

def vu(cx, cy, z, w, h, val, label='VU', big=False):
    shared_box(('vub', w, h), cx, cy, z, w + 0.5, h + 0.5, 0.35, M['black'], 0.12)
    zf = z + 0.36
    shared_box(('vuf', w, h), cx, cy, z + 0.3, w, h, 0.06, M['vuface'], 0.0)
    px, py = cx, cy - h * 0.62; R = h * 0.98
    pos = [(-20, -47), (-10, -30), (-7, -21), (-5, -13), (-3, -3), (-2, 3), (-1, 9.5), (0, 17), (1, 25), (2, 33), (3, 45)]
    for db, a in pos:
        ar = math.radians(a); ln = 0.2 * h / 2.6 * (1.4 if db in (-20, -10, -5, 0, 3) else 1)
        mm = M['vured'] if db > 0 else M['vuink']
        shared_box(('vutk', w, ln), px + math.sin(ar) * (R + ln / 2), py + math.cos(ar) * (R + ln / 2), zf, 0.035 * h / 2.6 * 1.3, ln, 0.004, mm, 0.0, rotz=-ar)
        if db in (-20, -10, -7, -5, -3, 0, 1, 2, 3):
            t = str(abs(db)) if db < 0 else ('+' + str(db) if db else '0')
            text(t, px + math.sin(ar) * (R + ln + 0.2 * h / 2.6), py + math.cos(ar) * (R + ln + 0.2 * h / 2.6), zf, 0.2 * h / 2.6, mm, rotz=-ar * 0.6)
    for i in range(60):  # arco
        a1 = -47 + i * (92 / 60); ar = math.radians(a1 + 92 / 120)
        mm = M['vured'] if a1 >= 17 else M['vuink']
        th = (0.07 if a1 >= 17 else 0.03) * h / 2.6
        shared_box(('vuarc', w, th), px + math.sin(ar) * R, py + math.cos(ar) * R, zf, R * math.radians(92 / 60) * 1.05, th, 0.004, mm, 0.0, rotz=-ar)
    text(label, cx, cy - h * 0.12, zf, 0.42 * h / 2.6, M['vuink'], space=1.2)
    text('−', px - w * 0.36, py + R * 0.5, zf, 0.3 * h / 2.6, M['vuink'])
    text('+', px + w * 0.36, py + R * 0.5, zf, 0.3 * h / 2.6, M['vured'])
    # sombra/carcaça da agulha
    shared_box(('vush', w), cx, cy - h * 0.43, z + 0.3, w, h * 0.16, 0.12, M['blackm'], 0.02)
    a = math.radians(-47 + val * 92); L = R * 1.06
    n = shared_box(('needle', L), 0, 0, zf + 0.08, 0.03 * h / 2.6 * 1.2, L, 0.01, M['needle'], 0.0, rotz=-a)
    n.location = (px + math.sin(a) * L / 2, py + math.cos(a) * L / 2, zf + 0.08)
    shared_box(('vug', w, h), cx, cy, z + 0.48, w, h, 0.02, M['glass'], 0.0)

def ledcol(x, y_bot, z, n, step, level, w=0.36, hh=None):
    hh = hh or step * 0.62
    for i in range(n):
        f = i / (n - 1); c = '#36d16a' if f < 0.68 else ('#f2c12e' if f < 0.86 else '#ff3a2a')
        shared_box(('led', w, hh), x, y_bot + i * step, z, w, hh, 0.07, led(c, f <= level), 0.02)

# ================================================================ peças do plugin
SURF = L.SURF

VU_R, VU_ANG = 0.78, 38.0 / 47.0   # raio da escala (× h) e o arco encolhido para caber inteiro no mostrador

def vu_face(cx, cy, z, w, h, label='VU'):
    """VU sem agulha e sem vidro: a agulha é desenhada pelo plugin (ver Layout.h: pivô em cy - 0.62 h, raio VU_R h,
    ângulos da tabela × VU_ANG). A escala e os números ficam dentro do mostrador, como num VU de verdade."""
    shared_box(('vub', w, h), cx, cy, z, w + 0.5, h + 0.5, 0.35, M['black'], 0.12)
    zf = z + 0.36
    shared_box(('vuf', w, h), cx, cy, z + 0.3, w, h, 0.06, M['vuface'], 0.0)
    px, py = cx, cy - h * 0.62; R = h * VU_R
    pos = [(-20, -47), (-10, -30), (-7, -21), (-5, -13), (-3, -3), (-2, 3), (-1, 9.5), (0, 17), (1, 25), (2, 33), (3, 45)]
    for db, a in pos:
        ar = math.radians(a * VU_ANG); ln = 0.2 * h / 2.6 * (1.4 if db in (-20, -10, -5, 0, 3) else 1)
        mm = M['vured'] if db > 0 else M['vuink']
        shared_box(('vutk', w, ln), px + math.sin(ar) * (R + ln / 2), py + math.cos(ar) * (R + ln / 2), zf, 0.035 * h / 2.6 * 1.3, ln, 0.004, mm, 0.0, rotz=-ar)
        if db in (-20, -10, -7, -5, -3, 0, 1, 2, 3):
            t = str(abs(db)) if db < 0 else ('+' + str(db) if db else '0')
            rl = R + 0.2 * h / 2.6 * 1.4 + 0.05 * h
            text(t, px + math.sin(ar) * rl, py + math.cos(ar) * rl, zf, 0.18 * h / 2.6, mm, rotz=-ar * 0.6)
    for i in range(60):
        a1 = -47 + i * (92 / 60); ar = math.radians((a1 + 92 / 120) * VU_ANG)
        mm = M['vured'] if a1 >= 17 else M['vuink']
        th = (0.07 if a1 >= 17 else 0.03) * h / 2.6
        shared_box(('vuarc', w, th), px + math.sin(ar) * R, py + math.cos(ar) * R, zf, R * math.radians(92 / 60 * VU_ANG) * 1.05, th, 0.004, mm, 0.0, rotz=-ar)
    text(label, cx, cy - h * 0.2, zf, 0.4 * h / 2.6, M['vuink'], space=1.2)
    text('−', px - w * 0.36, py + R * 0.42, zf, 0.3 * h / 2.6, M['vuink'])
    text('+', px + w * 0.36, py + R * 0.42, zf, 0.3 * h / 2.6, M['vured'])
    shared_box(('vush', w), cx, cy - h * 0.43, z + 0.3, w, h * 0.16, 0.12, M['blackm'], 0.02)

def base(cx, cy, w, h):
    box(cx, cy, -0.6, w + 0.02, h + 0.02, 0.6, M['base'], 0.0)

def module(cx, y0, y1, panel, w=L.SW):
    h = y0 - y1; cy = (y0 + y1) / 2
    shared_box(('mod', w, h, panel), cx, cy, 0, w - L.GAP, h - L.GAP, SURF, M[panel], 0.035)
    for sx in (-1, 1):
        screw(cx + sx * (w / 2 - 0.32), y0 - 0.3, SURF)
        screw(cx + sx * (w / 2 - 0.32), y1 + 0.3, SURF)

def lab(s, x, y, size=0.27, m=None):
    text(s, x, y, SURF + 0.003, size * 1.3, m or M['legend'], space=1.05)

def marks(x, y, d):
    scale_marks(x, y, SURF + 0.003, d)

# --- cada peça: função(ox, oy) que monta com o centro do módulo em x = ox e o y do layout deslocado de oy
def ch_bridge(ox, oy):
    box(ox, oy + (L.Y_BR_TOP + L.Y_BR_BOT) / 2, 0, L.SW, L.Y_BR_TOP - L.Y_BR_BOT, 1.4, M['bridge'], 0.0)
    v = L.VU_CH; vu_face(ox + v['x'], oy + v['y'], 1.4, v['w'], v['h'])
    shared_box('peak', ox + L.PEAK[0], oy + L.PEAK[1], 1.4, 0.22, 0.22, 0.06, led('#ff3a2a', False), 0.03)
    shared_box('sig', ox + L.SIG[0], oy + L.SIG[1], 1.4, 0.22, 0.22, 0.06, led('#36d16a', False), 0.03)

def pre(model):
    def f(ox, oy):
        y0, y1 = L.SEC['pre']; fam = L.PRE_FAM[model]
        base(ox, oy + (y0 + y1) / 2, L.SW, y0 - y1)
        module(ox, oy + y0, oy + y1, L.PANEL[fam])
        plate(ox, oy + L.PRE_NAME_Y, SURF, model)
        for k, (x, y, d) in L.PRE_KNOBS.items():
            marks(ox + x, oy + y, 2.25 if (k == 'gain' and fam == 'neve') else d * 1.08)
        for s, x, y, z in L.PRE_LABELS: lab(s, ox + x, oy + y, z)
    return f

def eq(model):
    def f(ox, oy):
        y0, y1 = L.SEC['eq']; fam = L.EQ_FAM[model]
        base(ox, oy + (y0 + y1) / 2, L.SW, y0 - y1)
        module(ox, oy + y0, oy + y1, L.PANEL[fam])
        plate(ox, oy + L.EQ_NAME_Y, SURF, model)
        for k, (pid, a, b, cap) in enumerate(L.EQ_ROWS):
            yy = oy + L.eq_row_y(k)
            marks(ox + L.EQ_GAIN_X, yy, L.EQ_GAIN_D * 1.08); marks(ox + L.EQ_FREQ_X, yy, L.EQ_FREQ_D * 1.08)
            lab(a, ox + L.EQ_GAIN_X, yy - 0.98, 0.22); lab(b, ox + L.EQ_FREQ_X, yy - 0.98, 0.22)
    return f

def dyn(name, labels):
    def f(ox, oy):
        y0, y1 = L.SEC['dyn']
        base(ox, oy + (y0 + y1) / 2, L.SW, y0 - y1)
        module(ox, oy + y0, oy + y1, 'ssl'); plate(ox, oy + L.DYN_NAME_Y, SURF, name)
        for k, l in enumerate(labels):
            marks(ox + L.DYN_X[k], oy + L.DYN_KNOBS_Y, 1.1); lab(l, ox + L.DYN_X[k], oy + L.DYN_KNOBS_Y - 0.9, 0.2)
        for k, (x, y) in enumerate(L.GR_LEDS):
            shared_box('grled', ox + x, oy + y, SURF, L.GR_LED_W, L.GR_LED_H, 0.06, led('#ffb347' if k < 4 else '#ff3a2a', False), 0.02)
    return f

def out(ox, oy):
    y0, y1 = L.SEC['out']
    base(ox, oy + (y0 + y1) / 2, L.SW, y0 - y1)
    module(ox, oy + y0, oy + y1, 'ssl')
    x, y, d = L.PAN; marks(ox + x, oy + y, 1.4)
    lab('L', ox - 1.15, oy + y, 0.22); lab('R', ox + 1.15, oy + y, 0.22); lab('PAN', ox, oy + y - 1.0, 0.2)

def ledcol_off(x, y_bot, n, step, w):
    hh = step * 0.62
    for i in range(n):
        f = i / (n - 1); c = '#36d16a' if f < 0.68 else ('#f2c12e' if f < 0.86 else '#ff3a2a')
        shared_box(('led', w, hh), x, y_bot + i * step, SURF, w, hh, 0.07, led(c, False), 0.02)

def fad(ox, oy):
    y0, y1 = L.SEC['fad']
    base(ox, oy + (y0 + y1) / 2, L.SW, y0 - y1)
    module(ox, oy + y0, oy + y1, 'ssl')
    sx = ox + L.FAD_X; yt, yb = oy + L.FAD_YT, oy + L.FAD_YB; Ln = yt - yb
    shared_box('slot', sx, (yt + yb) / 2, SURF, 0.24, Ln, 0.006, M['slot'], 0.0)
    for t, fr in L.FAD_MARKS:
        yy = yt - fr * Ln
        shared_box(('ftk', t == '0'), sx - 0.62, yy, SURF, 0.42 if t == '0' else 0.26, 0.035, 0.004, M['legend'], 0.0)
        text(t, sx - 0.95, yy, SURF + 0.003, 0.24, M['legend'], ax='RIGHT')
    for mx in L.METER_X: ledcol_off(ox + mx, yb, L.METER_N, Ln / (L.METER_N - 1), L.METER_W)
    x, y, w, h = L.TAPE
    shared_box('tape', ox + x, oy + y, SURF, w, h, 0.012, M['tape'], 0.0)

def blank(ox, oy):
    y0, y1 = L.SEC['pre'][0], L.SEC['fad'][1]
    base(ox, oy + (y0 + y1) / 2, L.SW, y0 - y1)
    for s in ('pre', 'eq', 'dyn', 'out', 'fad'):
        a, b = L.SEC[s]; module(ox, oy + a, oy + b, 'ssl')

def m_bridge(ox, oy):
    box(ox, oy + (L.Y_BR_TOP + L.Y_BR_BOT) / 2, 0, L.MW, L.Y_BR_TOP - L.Y_BR_BOT, 1.4, M['bridge'], 0.0)
    for (x, y, w, h), lb in zip(L.MVU, ('L  VU', 'R  VU')): vu_face(ox + x, oy + y, 1.4, w, h, lb)

def m_logo(ox, oy):
    y0, y1 = L.MSEC['logo']
    base(ox, oy + (y0 + y1) / 2, L.MW, y0 - y1)
    module(ox, oy + y0, oy + y1, 'ssl', L.MW)
    text('MESA', ox - 5.9, oy - 6.55, SURF + 0.003, 2.0, M['legend'], font=FLOGO, ax='LEFT', space=1.15)
    text('CONSOLE ANALÓGICA', ox - 5.85, oy - 7.95, SURF + 0.003, 0.36, M['legend'], ax='LEFT', space=1.3)
    text('INSIDE STUDIO SP', ox - 5.85, oy - 8.5, SURF + 0.003, 0.36, M['legend_y'], ax='LEFT', space=1.3)
    shared_box('logoline', ox + 0.4, oy - 6.55, SURF, 0.04, 2.2, 0.004, M['legend'], 0.0)
    for (x, y, w), l in zip(L.MODE_BTNS, ('CANAL', 'GRUPO', 'MASTER')): lab(l, ox + x, oy + y - 0.8, 0.22)
    for (x, y, w), l in zip(L.OPT_BTNS, ('VU', 'PPM', 'ECO', 'HQ')): lab(l, ox + x, oy + y - 0.7, 0.2)
    lab('MEDIÇÃO', ox + 2.3, oy - 10.75, 0.18); lab('PROCESSO', ox + 5.8, oy - 10.75, 0.18)

def m_cola(ox, oy):
    y0, y1 = L.MSEC['cola']
    base(ox, oy + (y0 + y1) / 2, L.MW, y0 - y1)
    module(ox, oy + y0, oy + y1, 'ssl', L.MW)
    plate(ox, oy - 12.1, SURF, 'COLA · BUS 4K', 5.0)
    x, y, w, h = L.COLA_VU; vu_face(ox + x, oy + y, SURF, w, h, 'GR')
    nomes = dict(thr='THRESHOLD', atk='ATTACK', rel='RELEASE', mkp='MAKEUP', rat='RATIO', mix='MIX')
    escalas = dict(atk=['.1', '.3', '1', '3', '10', '30'], rel=['.1', '.3', '.6', '1.2', 'AUTO'], rat=['1.5', '2', '4', '10'])
    for pid, x, y, cap in L.COLA_KNOBS:
        marks(ox + x, oy + y, 2.0); lab(nomes[pid], ox + x, oy + y - 1.5, 0.26)
        for k, t in enumerate(escalas.get(pid, [])):
            n = len(escalas[pid]); a = math.radians(-150 + 300 * k / (n - 1))
            lab(t, ox + x + math.sin(a) * 1.62, oy + y + math.cos(a) * 1.62, 0.17)
    lab('COMP', ox + L.COLA_IN[0] + 1.1, oy + L.COLA_IN[1], 0.2)
    lab('SC 90', ox + L.COLA_HPF[0] + 1.3, oy + L.COLA_HPF[1], 0.2)

def m_out(ox, oy):
    y0, y1 = L.MSEC['out']
    base(ox, oy + (y0 + y1) / 2, L.MW, y0 - y1)
    module(ox, oy + y0, oy + y1, 'ssl', L.MW)
    plate(ox, oy - 27.0, SURF, 'SAÍDA STEREO', 5.0)
    sx = ox + L.MFAD_X; yt, yb = oy + L.MFAD_YT, oy + L.MFAD_YB; Ln = yt - yb
    shared_box('mslot', sx, (yt + yb) / 2, SURF, 0.28, Ln, 0.006, M['slot'], 0.0)
    for t, fr in L.FAD_MARKS:
        yy = yt - fr * Ln
        shared_box(('mftk', t == '0'), sx - 0.9, yy, SURF, 0.6 if t == '0' else 0.36, 0.04, 0.004, M['legend'], 0.0)
        text(t, sx - 1.35, yy, SURF + 0.003, 0.3, M['legend'], ax='RIGHT')
    for mx in L.MMETER_X: ledcol_off(ox + mx, yb, L.MMETER_N, Ln / (L.MMETER_N - 1), L.MMETER_W)
    lab('L', ox + 1.4, oy - 43.3, 0.24); lab('R', ox + 2.2, oy - 43.3, 0.24)
    for (x, y), v in zip(L.MREAD, ('FADER', 'PICO dBFS', 'GR dB')): lab(v, ox + x, oy + y + 0.6, 0.2)
    x, y, w, h = L.MTAPE
    shared_box('mtape', ox + x, oy + y, SURF, w, h, 0.012, M['tape'], 0.0)

def wood_side(ox, oy):  # (ox, oy) = centro da peça
    box(ox, oy - L.TOTAL_H / 2 - 1.0, -1.0, L.WOOD_W, L.TOTAL_H + 2.0, 4.2, M['wood'], 0.5)

def top_frame(ox, oy):
    box(ox, oy - L.TOP_FRAME, -1.0, L.TOTAL_W + 2.0, L.TOP_FRAME, 3.2, wood_mat('madeira_h', (0.015, 0.15, 0.15), 'Y'), 0.5)

def leather(ox, oy):
    w = L.TOTAL_W - 2 * L.WOOD_W + 2.0
    box(ox, oy - L.ARM / 2, -1.6, w, L.ARM + 2, 0.6, M['base'], 0.0)
    box(ox, oy - 0.4 - 4.4 / 2, -1.0, w, 4.4, 3.0, M['leather'], 1.1)

# ---------------------------------------------------------------- lista de peças (0.2: acabamento real + vista inclinada)
# Câmera ORTOGRÁFICA inclinada TILT graus (o operador olha de frente e de cima): a projeção é a mesma em qualquer
# ponto da cena, então cada peça renderizada sozinha casa com a chapa em qualquer posição da tela.
#   tela (cm, para cima) = y·cos(T) + (z − za)·sin(T);  tela x = x
# Cada célula tem uma âncora (x, y, za) do layout; o atlas guarda o retângulo e onde a âncora cai dentro dele.
# Luz: sol principal (com sombra, macio) + rebatedor, contra e rasante sem sombra + HDRI fraco — tudo invariável
# por translação. Peças (knobs, teclas, LEDs acesos, faders) com fundo transparente sobre um "shadow catcher":
# a sombra de contato vem do 3D, não do 2D.
exec(compile(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'realismo.py'), encoding='utf-8').read(), 'realismo.py', 'exec'), globals())
materiais_base()

TILT = float(arg('--tilt', '18')); TR = math.radians(TILT); CT, ST = math.cos(TR), math.sin(TR)
BR_Z = 1.4                       # altura da ponte de VUs
FRAME_Z, FLOOR_Z = 3.2, -1.0     # topo da madeira / pé da frente do descanso de braço
SEC = L.SEC

def hidden(o):                   # só faz sombra (a peça vizinha que não está nesta célula)
    o.visible_camera = False; o.visible_glossy = False; o.visible_diffuse = False
    return o

def caster_bridge(ox, oy):       # frente da ponte de VUs, para a sombra cair no topo do módulo PRÉ
    hidden(box(ox, oy + L.Y_BR_BOT + 0.5, 0, L.SW + 0.2, 1.0, BR_Z, M['bridge'], 0.0))

def caster_frame(ox, oy, w):     # frente da madeira de cima, para a sombra cair no topo da ponte
    hidden(box(ox, oy + L.Y_BR_TOP + 0.5, 0, w + 0.2, 1.0, FRAME_Z, M['base'], 0.0))

# --- chapas (opacas): função(ox, oy) monta com o centro em x = ox e o y do layout deslocado de oy
def c_bridge(ox, oy):
    box(ox, oy + (L.Y_BR_TOP + L.Y_BR_BOT) / 2, 0, L.SW, L.Y_BR_TOP - L.Y_BR_BOT, BR_Z, M['bridge'], 0.04)
    box(ox, oy + (L.Y_BR_BOT + SEC['pre'][0]) / 2, -0.6, L.SW, L.Y_BR_BOT - SEC['pre'][0], 0.6, M['base'], 0.0)
    v = L.VU_CH; vu_face(ox + v['x'], oy + v['y'], BR_Z, v['w'], v['h']); vu_lamp(ox + v['x'], oy + v['y'], BR_Z, v['h'])
    for (x, y), c in ((L.SIG, '#36d16a'), (L.PEAK, '#ff3a2a')): led_dome(ox + x, oy + y, BR_Z, c, False)
    caster_frame(ox, oy, L.SW)

def c_mod(fn, with_bridge=False):
    def f(ox, oy):
        fn(ox, oy)
        if with_bridge: caster_bridge(ox, oy)
    return f

def c_div(ox, oy, w=1.0):        # divisória lisa entre canais/grupos/MASTER (também a borda junto da madeira)
    box(ox, oy + (L.Y_BR_TOP + L.Y_BR_BOT) / 2, 0, w, L.Y_BR_TOP - L.Y_BR_BOT, BR_Z, M['bridge'], 0.04)
    y0, y1 = SEC['pre'][0], SEC['fad'][1]
    box(ox, oy + (L.Y_BR_BOT + y0) / 2, -0.6, w, L.Y_BR_BOT - y0, 0.6, M['base'], 0.0)
    base(ox, oy + (y0 + y1) / 2, w, y0 - y1)
    shared_box(('div', w), ox, oy + (y0 + y1) / 2, 0, w - L.GAP, y0 - y1 - L.GAP, SURF, M['ssl'], 0.035)
    caster_frame(ox, oy, w)

def c_mbridge(ox, oy):
    box(ox, oy + (L.Y_BR_TOP + L.Y_BR_BOT) / 2, 0, L.MW, L.Y_BR_TOP - L.Y_BR_BOT, BR_Z, M['bridge'], 0.04)
    box(ox, oy + (L.Y_BR_BOT + SEC['pre'][0]) / 2, -0.6, L.MW, L.Y_BR_BOT - SEC['pre'][0], 0.6, M['base'], 0.0)
    for (x, y, w, h), lb in zip(L.MVU, ('L  VU', 'R  VU')):
        vu_face(ox + x, oy + y, BR_Z, w, h, lb); vu_lamp(ox + x, oy + y, BR_Z, h)
    caster_frame(ox, oy, L.MW)

def c_mcola(ox, oy):
    m_cola(ox, oy)
    x, y, w, h = L.COLA_VU; vu_lamp(ox + x, oy + y, SURF, h)

def c_top(ox, oy):
    box(ox, oy + L.Y_BR_TOP + L.TOP_FRAME / 2, -1.0, L.TOTAL_W, L.TOP_FRAME, FRAME_Z + 1.0, wood_mat('madeira_h', (0.015, 0.15, 0.15), 'Y'), 0.15)

def c_wood(ox, oy):
    yb = L.Y_BOT - 0.2
    box(ox, oy + (L.Y_TOP + yb) / 2, -1.0, L.WOOD_W, L.Y_TOP - yb, FRAME_Z + 1.0, M['wood'], 0.3)

def c_leather(ox, oy):
    w = L.TOTAL_W - 2 * L.WOOD_W
    y0 = SEC['fad'][1]; yb = L.Y_BOT - 0.2
    box(ox, oy + y0 - 0.25, -1.0, w, 0.5, 0.7, M['base'], 0.0)                         # fresta entre o painel e o braço
    box(ox, oy + (y0 + yb) / 2, -1.6, w, y0 - yb, 0.6, M['base'], 0.0)
    box(ox, oy + (y0 - 0.4 + yb) / 2, FLOOR_Z, w, y0 - 0.4 - yb, 2.0 - FLOOR_Z, M['leather'], 1.1)

# célula: (nome, x0, x1, yb, yt, zlo, zhi, za, função(ax, ay))
#   [x0, x1] × [yb, yt] = pegada no chão em volta da âncora; zlo/zhi = altura da borda de baixo/de cima da célula
#   (a borda de cima de uma célula de chapa casa com a borda de baixo da célula de trás)
def chapa_mod(nome, w, y0, y1, fn, ztop=None):
    return (nome, -w / 2, w / 2, y1 - y0, 0.0, SURF, ztop or SURF, SURF, lambda ax, ay: fn(ax, ay - y0))

CHAPAS = [chapa_mod('ch_bridge', L.SW, L.Y_BR_TOP, SEC['pre'][0], c_bridge, BR_Z)]
CHAPAS += [chapa_mod('pre_' + m, L.SW, *SEC['pre'], c_mod(pre(m), True)) for m in L.PRE_FAM]
CHAPAS += [chapa_mod('eq_' + m, L.SW, *SEC['eq'], eq(m)) for m in L.EQ_FAM]
CHAPAS += [chapa_mod('dyn_VCA', L.SW, *SEC['dyn'], dyn('VCA', ('THR', 'RAT', 'GATE'))),
           chapa_mod('dyn_COLA 4K', L.SW, *SEC['dyn'], dyn('COLA 4K', ('THR', 'RAT', 'MIX'))),
           chapa_mod('out', L.SW, *SEC['out'], out),
           chapa_mod('fad', L.SW, *SEC['fad'], fad),
           chapa_mod('blank', L.SW, SEC['pre'][0], SEC['fad'][1], c_mod(blank, True)),
           chapa_mod('div', 1.0, L.Y_BR_TOP, SEC['fad'][1], c_div, BR_Z),
           chapa_mod('m_bridge', L.MW, L.Y_BR_TOP, SEC['pre'][0], c_mbridge, BR_Z),
           chapa_mod('m_logo', L.MW, *L.MSEC['logo'], c_mod(m_logo, True)),
           chapa_mod('m_cola', L.MW, *L.MSEC['cola'], c_mcola),
           chapa_mod('m_out', L.MW, *L.MSEC['out'], m_out),
           ('top', -L.TOTAL_W / 2, L.TOTAL_W / 2, 0.0, L.TOP_FRAME, BR_Z, FRAME_Z, SURF, lambda ax, ay: c_top(ax, ay - L.Y_BR_TOP)),
           ('wood', -L.WOOD_W / 2, L.WOOD_W / 2, L.Y_BOT - 0.2 - L.Y_TOP, 0.0, FLOOR_Z, FRAME_Z, SURF, lambda ax, ay: c_wood(ax, ay - L.Y_TOP)),
           ('leather', -(L.TOTAL_W - 2 * L.WOOD_W) / 2, (L.TOTAL_W - 2 * L.WOOD_W) / 2, L.Y_BOT - 0.2 - SEC['fad'][1], 0.0, FLOOR_Z, SURF, SURF,
            lambda ax, ay: c_leather(ax, ay - SEC['fad'][1]))]

# --- peças (fundo transparente + sombra): âncora = centro da peça no pé (za)
KCAPS = ['red', 'grn', 'blu', 'brn', 'gry', 'wht', 'blk', 'org']
FRAMES = int(arg('--frames', '48'))
KNOBS = [('k_ssl_' + c, (lambda c: lambda x, y, v: knob_ssl(x, y, SURF, v, c, 1.5))(c)) for c in KCAPS]
KNOBS += [('k_api', lambda x, y, v: knob_api(x, y, SURF, v, 1.5)),
          ('k_mar', lambda x, y, v: knob_marconi(x, y, SURF, v, 1.5)),
          ('k_mar_gain', lambda x, y, v: knob_marconi(x, y, SURF, v, 1.5, red=True))]
KNOBS += [('k_mar_' + c, (lambda c: lambda x, y, v: knob_marconi(x, y, SURF, v, 1.5, cap=c))(c)) for c in ('red', 'grn', 'blu', 'brn', 'org')]
BCOLS = dict(amb='#ffb347', grn='#3fbf6a', red='#e8473c', yel='#f2d24b', blu='#3d8bff')
# teclas: (nome, cor, legenda na lente, largura da tecla) — o plugin desenha na escala 1 (cm de verdade)
KEYS = [('pad', 'amb', None, 0.46), ('eq', 'grn', 'EQ', 0.62), ('pre', 'amb', 'PRÉ', 0.62),
        ('g1', 'grn', 'G1', 0.594), ('g2', 'grn', 'G2', 0.594), ('g3', 'grn', 'G3', 0.594),
        ('m', 'red', 'M', 0.62), ('s', 'yel', 'S', 0.62), ('mode', 'amb', None, 0.75),
        ('optb', 'blu', None, 0.62), ('optg', 'grn', None, 0.62), ('in', 'amb', 'IN', 0.66), ('hpf', 'blu', 'HPF', 0.66)]
LCOLS = dict(g='#36d16a', y='#f2c12e', r='#ff3a2a', a='#ffb347')
CH_STEP = (L.FAD_YT - L.FAD_YB) / (L.METER_N - 1)
M_STEP = (L.MFAD_YT - L.MFAD_YB) / (L.MMETER_N - 1)
LEDS = [('l_m_' + c, L.METER_W, CH_STEP * 0.62) for c in 'gyr'] + [('l_mm_' + c, L.MMETER_W, M_STEP * 0.62) for c in 'gyr'] \
     + [('l_gr_' + c, L.GR_LED_W, L.GR_LED_H) for c in 'ar']

def fcap(x, y):
    shared_box('fstem', x, y, SURF, 0.16, 0.5, 0.4, M['chrome'], 0.0)
    shared_box('fcap', x, y, SURF + 0.35, L.FAD_CAP_W, L.FAD_CAP_H, 0.85, M['black'], 0.22)
    shared_box('fcapl', x, y, SURF + 1.199, 1.0, 0.09, 0.005, M['white'], 0.0)
    for k in (-1, 1): shared_box('fcapr', x, y + k * 0.45, SURF + 1.199, 0.9, 0.05, 0.004, M['blackm'], 0.0)
def mfcap(x, y):
    shared_box('mfstem', x, y, SURF, 0.18, 0.6, 0.4, M['chrome'], 0.0)
    shared_box('mfcap', x, y, SURF + 0.35, L.MFAD_CAP_W, L.MFAD_CAP_H, 0.95, M['red'], 0.25)
    shared_box('mfcapl', x, y, SURF + 1.299, 1.3, 0.1, 0.005, M['white'], 0.0)

GLOW = 0.3   # margem para o brilho que o recorta.py põe em volta do que está aceso
PECAS = []
for n, f in KNOBS:
    big = n == 'k_mar_gain'
    for i in range(FRAMES):
        PECAS.append((f'{n}_{i:02d}', -1.25 if big else -1.1, 1.4 if big else 1.25, -1.75 if big else -1.6, 1.15 if big else 1.0,
                      SURF, SURF + 1.25, SURF, (lambda f, i: lambda x, y: f(x, y, i / (FRAMES - 1)))(f, i)))
for nm, c, lb, w in KEYS:
    for on in (0, 1):
        r = w / 2 + 0.08 + GLOW
        PECAS.append((f'b_{nm}_{on}', -r, r + 0.15, -r - 0.2, r, SURF, SURF + 0.75, SURF,
                      (lambda c, on, lb, w: lambda x, y: keycap(x, y, BCOLS[c], bool(on), lb, w))(c, on, lb, w)))
for nm, w, h in LEDS:
    PECAS.append((nm, -w / 2 - GLOW, w / 2 + GLOW, -h / 2 - GLOW, h / 2 + GLOW, SURF, SURF + 0.2, SURF,
                  (lambda nm, w, h: lambda x, y: shared_box(('led', w, h), x, y, SURF, w, h, 0.07, led(LCOLS[nm[-1]], True), 0.02))(nm, w, h)))
for c in 'gr':   # LED em domo da ponte (sinal/pico): fica no alto da ponte (za = BR_Z)
    PECAS.append(('l_d_' + c, -0.2 - GLOW, 0.2 + GLOW, -0.2 - GLOW, 0.2 + GLOW, BR_Z, BR_Z + 0.25, BR_Z,
                  (lambda c: lambda x, y: led_dome(x, y, BR_Z, LCOLS[c], True))(c)))
PECAS += [('fcap', -1.0, 1.15, -1.75, 1.3, SURF, SURF + 1.25, SURF, fcap), ('mfcap', -1.2, 1.35, -1.95, 1.5, SURF, SURF + 1.35, SURF, mfcap)]

def world_and_lights(dark_bg):
    w = bpy.data.worlds.new('w'); sc.world = w; w.use_nodes = True; nt = w.node_tree
    env = nt.nodes.new('ShaderNodeTexEnvironment'); env.image = bpy.data.images.load(HDRI)
    mp = nt.nodes.new('ShaderNodeMapping'); tc = nt.nodes.new('ShaderNodeTexCoord'); mp.inputs['Rotation'].default_value = (0, 0, math.radians(120))
    nt.links.new(tc.outputs['Generated'], mp.inputs['Vector']); nt.links.new(mp.outputs['Vector'], env.inputs['Vector'])
    bg = nt.nodes['Background']; nt.links.new(env.outputs['Color'], bg.inputs['Color']); bg.inputs['Strength'].default_value = float(arg('--hdri', '0.4'))
    if dark_bg:   # a câmera vê a sala escura; a luz do HDRI continua valendo para o resto
        lp = nt.nodes.new('ShaderNodeLightPath'); dk = nt.nodes.new('ShaderNodeBackground'); dk.inputs['Color'].default_value = hexc('#0b0b0c')
        mx = nt.nodes.new('ShaderNodeMixShader'); out_ = nt.nodes['World Output']
        nt.links.new(lp.outputs['Is Camera Ray'], mx.inputs[0]); nt.links.new(bg.outputs[0], mx.inputs[1]); nt.links.new(dk.outputs[0], mx.inputs[2])
        nt.links.new(mx.outputs[0], out_.inputs['Surface'])
    def sun(nm, d, strength, angle, color=(1, 0.96, 0.9), shadow=True):
        ld = bpy.data.lights.new(nm, 'SUN'); ld.energy = strength; ld.angle = math.radians(angle); ld.color = color
        for a_ in ('use_shadow',):
            if hasattr(ld, a_): setattr(ld, a_, shadow)
        try: ld.cycles.cast_shadow = shadow
        except Exception: pass
        o = bpy.data.objects.new(nm, ld); col.objects.link(o)
        o.rotation_euler = (-Vector(d)).to_track_quat('-Z', 'Y').to_euler()
    # as direções das softboxes da amostra aprovada (real.png), como sóis macios
    sun('key', (-35, 55, 95), float(arg('--key', '4.1')), 22)
    sun('fill', (55, -70, 60), float(arg('--fill', '0.6')), 35, (0.86, 0.9, 1.0), False)
    sun('rim', (10, 110, 22), float(arg('--rim', '1.0')), 15, (1, 0.96, 0.9), False)
    sun('strip', (-70, -10, 25), float(arg('--strip', '0.8')), 20, (1.0, 0.92, 0.82), False)

def catcher(x0, x1, y0, y1, z):
    me = box_mesh(f'cat{len(bpy.data.meshes)}', x1 - x0, y1 - y0, 0.01, 0.0)
    me.materials.append(M['ssl'])
    o = bpy.data.objects.new('catcher', me); col.objects.link(o); o.location = ((x0 + x1) / 2, (y0 + y1) / 2, z - 0.01)
    o.is_shadow_catcher = True
    return o

def render_cells(cells, name, transparent, ppc, gap, maxw):
    # 1) retângulo de cada célula na tela (cm) e a distância da âncora até a borda de cima
    rects = []
    for (n, x0, x1, yb, yt, zlo, zhi, za, f) in cells:
        top = yt * CT + (zhi - za) * ST; bot = yb * CT + (zlo - za) * ST
        rects.append((n, x1 - x0, top - bot, top, x0, f, (x0, x1, yb, yt, za)))
    # 2) prateleiras na tela, com folga (a sombra de uma não pode cair na outra)
    pos = {}; x = y = rowh = 0.0
    for n, w, h, top, x0, f, fp in rects:
        if x > 0 and x + w > maxw: x = 0.0; y += rowh + gap; rowh = 0.0
        pos[n] = (x, y); x += w + gap; rowh = max(rowh, h)
    W = max(pos[r[0]][0] + r[1] for r in rects); H = y + rowh
    RX, RY = math.ceil(W * ppc), math.ceil(H * ppc); W, H = RX / ppc, RY / ppc
    mapa = {}
    for n, w, h, top, x0, f, (fx0, fx1, fyb, fyt, za) in rects:
        sx, sd = pos[n]
        ax = sx - x0
        ay = (-(sd + top) - (za - SURF) * ST) / CT      # y no mundo que põe a âncora (z = za) na altura certa da tela
        f(ax, ay)
        if transparent: catcher(ax + fx0, ax + fx1, ay + fyb, ay + fyt, za)
        mapa[n] = [round(sx * ppc), round(sd * ppc), round(w * ppc), round(h * ppc), round(-x0 * ppc, 2), round(top * ppc, 2)]
    materiais_uso()
    world_and_lights(not transparent)
    cam = bpy.data.cameras.new('cam'); cam.type = 'ORTHO'
    co = bpy.data.objects.new('cam', cam); col.objects.link(co); sc.camera = co
    cam.ortho_scale = max(W, H); cam.clip_end = 500
    yc = (-H / 2) / CT                                    # ponto (W/2, yc, SURF) cai no centro da tela
    co.location = (W / 2, yc - ST * 200, SURF + CT * 200); co.rotation_euler = (TR, 0, 0)
    sc.render.resolution_x, sc.render.resolution_y = RX, RY
    sc.render.resolution_percentage = 100
    sc.render.engine = 'CYCLES'; cy = sc.cycles
    cy.device = 'CPU'; cy.samples = SAMPLES; cy.use_adaptive_sampling = True; cy.adaptive_threshold = 0.015
    cy.use_denoising = True; cy.denoiser = 'OPENIMAGEDENOISE'
    cy.max_bounces = 8; cy.glossy_bounces = 4; cy.transmission_bounces = 4; cy.transparent_max_bounces = 8
    cy.caustics_reflective = False; cy.caustics_refractive = False
    sc.view_settings.view_transform = 'Khronos PBR Neutral'; sc.view_settings.look = 'None'
    sc.render.film_transparent = transparent
    sc.render.image_settings.file_format = 'PNG'; sc.render.image_settings.color_mode = 'RGBA' if transparent else 'RGB'
    sc.render.image_settings.color_depth = '8'
    sc.render.filepath = os.path.join(OUTDIR, name + '.png')
    print('OBJETOS', len(bpy.data.objects), 'RES', RX, RY, 'CELULAS', len(cells))
    bpy.ops.render.render(write_still=True)
    print('RENDER OK', sc.render.filepath)
    return mapa

os.makedirs(OUTDIR, exist_ok=True)
NOME = arg('--nome', PART)
if PART == 'chapas':
    lst = [c for c in CHAPAS if not ONLY or c[0] in ONLY]
    mapa = render_cells(lst, NOME, False, PPC, 3.0, 96.0)
else:
    lst = [c for c in PECAS if not ONLY or any(c[0].startswith(o) for o in ONLY)]
    if arg('--sem', ''):
        sem = arg('--sem', '').split(','); lst = [c for c in lst if not any(c[0].startswith(o) for o in sem)]
    mapa = render_cells(lst, NOME, True, PPC, 0.7, 40.0)
json.dump({'ppc': PPC, 'tilt': TILT, 'img': NOME + '.png', 'pecas': mapa}, open(os.path.join(OUTDIR, NOME + '.json'), 'w'), indent=0)

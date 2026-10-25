# SÉRIE KNOB - peças da interface renderizadas no Blender/Cycles (a mesma cena da prévia aprovada)
#
#   blender -b -P sprites.py -- <modo> <pasta de saída> <pasta das texturas> [amostras]
#
# modos:
#   mapa   -> mapa.json: posição em pixels de tudo que a interface desenha por cima (lâmpadas, knobs, textos, visor)
#   fundo  -> fundo.png: o pedal inteiro sem knobs, sem alavanca, lâmpadas apagadas e sem o nome
#   nome   -> nome_<pedal>.png: só a região do nome/subtítulo, um por pedal (a interface recorta e encaixa)
#   knob   -> knob_00..15.png: o knob grandão (transparente, com a sombra) em 16 posições de UMA canelura;
#             as caneluras se repetem, então 16 quadros cobrem o giro inteiro. A linha e o emblema a interface desenha.
#   rate / mix -> knobs pequenos, 16 quadros cada
#   alav   -> alav_int.png, alav_midi.png, alav_audio.png
# Saída em 1000 x 1284 (a janela do plugin é 500 x 642 em 2x); o recorte de cada peça sai no recorta.py.
import bpy, bmesh, math, sys, json, os
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view

argv = sys.argv[sys.argv.index('--') + 1:]
MODO, SAIDA, TEX = argv[0], argv[1], argv[2]
AMOSTRAS = int(argv[3]) if len(argv) > 3 else 64
os.makedirs(SAIDA, exist_ok=True)
QUADROS = 16

def hexcor(h): return tuple((int(h[i:i + 2], 16) / 255) ** 2.2 for i in (0, 2, 4))

PEDAIS = {
    'fole': ('FOLE', 'SIDECHAIN SINCRONIZADO'), 'picador': ('PICADOR', 'TRANCE GATE'),
    'mare': ('MARÉ', 'FILTRO EM MOVIMENTO'), 'vaivem': ('VAIVÉM', 'AUTO-PAN NO TEMPO'),
    'repique': ('REPIQUE', 'STUTTER · BEAT REPEAT'), 'eco': ('ECO', 'DELAY DE FITA'),
    'avesso': ('AVESSO', 'REVERSO NO TEMPO'), 'freio': ('FREIO', 'TAPE STOP · TAPE START'),
    'escada': ('ESCADA', 'SEQUENCIADOR DE PITCH'), 'mastiga': ('MASTIGA', 'BITCRUSH RITMADO'),
}

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
FONTE = bpy.data.fonts.load('C:/Windows/Fonts/segoeuisl.ttf')
FONTE_B = bpy.data.fonts.load('C:/Windows/Fonts/segoeuib.ttf')
FONTE_M = bpy.data.fonts.load('C:/Windows/Fonts/seguisb.ttf')

# ---------------- materiais (iguais aos da prévia) ----------------
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
    m = principled(nome, (0.2, 0.08, 0.03), rough=0.4, coat=0.8, coat_rough=0.1)
    nt = m.node_tree; n = nt.nodes; l = nt.links; b = n['Principled BSDF']
    tc = n.new('ShaderNodeTexCoord'); mp = n.new('ShaderNodeMapping')
    mp.inputs['Scale'].default_value = (0.1, 0.1, 0.1)
    mp.inputs['Location'].default_value = (0.37 if eixo == 'x' else 0.0, 0.21, 0)
    if eixo == 'x': mp.inputs['Rotation'].default_value = (0, 0, math.radians(90))
    l.new(tc.outputs['Object'], mp.inputs['Vector'])
    cor = n.new('ShaderNodeTexImage'); cor.image = bpy.data.images.load(TEX + '/nogueira_cor.jpg')
    rug = n.new('ShaderNodeTexImage'); rug.image = bpy.data.images.load(TEX + '/nogueira_rug.jpg')
    rug.image.colorspace_settings.name = 'Non-Color'
    for t in (cor, rug): l.new(mp.outputs['Vector'], t.inputs['Vector'])
    mx = n.new('ShaderNodeMix'); mx.data_type = 'RGBA'; mx.blend_type = 'MULTIPLY'
    mx.inputs['Factor'].default_value = 1.0; mx.inputs['B'].default_value = (0.5, 0.4, 0.32, 1)
    bc = n.new('ShaderNodeBrightContrast'); bc.inputs['Contrast'].default_value = 0.25
    hs = n.new('ShaderNodeHueSaturation'); hs.inputs['Saturation'].default_value = 0.7
    l.new(cor.outputs['Color'], hs.inputs['Color']); l.new(hs.outputs['Color'], bc.inputs['Color']); l.new(bc.outputs['Color'], mx.inputs['A'])
    l.new(mx.outputs['Result'], b.inputs['Base Color'])
    l.new(rug.outputs['Color'], b.inputs['Roughness'])
    bp = n.new('ShaderNodeBump'); bp.inputs['Strength'].default_value = 0.12
    l.new(cor.outputs['Color'], bp.inputs['Height']); l.new(bp.outputs['Normal'], b.inputs['Normal'])
    return m

M_PAINEL = escovado(principled('painel', (0.93, 0.93, 0.94), metal=1, rough=0.45), escala=(1.5, 700, 1.5), forca=0.18)
M_ALU = principled('alu', (0.91, 0.91, 0.92), metal=1, rough=0.2)
M_CROMO = principled('cromo', (0.95, 0.95, 0.95), metal=1, rough=0.06)
M_TAMPA = escovado(principled('tampa', (0.012, 0.012, 0.013), rough=0.28, coat=1, coat_rough=0.04), radial=True, forca=0.03)
M_TINTA = principled('tinta', (0.86, 0.8, 0.66), rough=0.55)
M_SERIG = principled('serig', (0.012, 0.012, 0.012), rough=0.6)
M_PRETO = principled('preto', (0.02, 0.02, 0.022), metal=0.6, rough=0.4)
M_VIDRO = principled('vidro', (0.004, 0.004, 0.005), rough=0.75)
# lâmpada apagada neutra: a interface pinta a cor de cada pedal (apagada e acesa) por cima
M_LAMP_OFF = principled('lamp_off', (0.05, 0.05, 0.055), rough=0.05, coat=1, coat_rough=0.02)
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
    bm = bmesh.new(); bx, tp = [], []
    for i in range(n):
        a = 2 * math.pi * i / n; r = raio_fn(a)
        bx.append(bm.verts.new((r * math.cos(a), r * math.sin(a), z0)))
        tp.append(bm.verts.new((r * math.cos(a), r * math.sin(a), z1)))
    for i in range(n):
        j = (i + 1) % n; bm.faces.new((bx[i], bx[j], tp[j], tp[i]))
    tampas = [bm.faces.new(tp), bm.faces.new(list(reversed(bx)))]
    bmesh.ops.poke(bm, faces=tampas)
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

def texto(s, x, y, tam, mat=M_TINTA, z=0.0, alin='CENTER', esp=1.0, fonte=None):
    cu = bpy.data.curves.new('t', 'FONT'); cu.body = s; cu.font = fonte or FONTE
    cu.size = tam; cu.align_x = alin; cu.align_y = 'CENTER'; cu.extrude = 0.008; cu.space_character = esp
    o = obj('t_' + s, cu, mat); o.location = (x, y, z + 0.008 + (0.03 if y < -10 else 0))
    return o

def esfera(nome, r, x, y, z, mat, achat=1.0):
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=48, v_segments=24, radius=r)
    me = bpy.data.meshes.new(nome); bm.to_mesh(me); bm.free()
    o = suave(obj(nome, me, mat)); o.location = (x, y, z); o.scale.z = achat
    return o

def knob(cx, cy, R, H, flautas):
    """knob sem linha e sem emblema (a interface desenha): só o que é igual a cada canelura"""
    pai = bpy.data.objects.new('knob', None); sc.collection.objects.link(pai)
    pai.location = (cx, cy, 0)
    pecas = [cilindro('saia', R * 1.22, 0.0, 0.45, M_ALU, bev=0.06)]
    prof = R * 0.045
    pecas.append(perfil('corpo', lambda a: R - prof * (0.5 - 0.5 * math.cos(flautas * a)), flautas * 12, 0.45, H, M_ALU, bev=0.16))
    pecas.append(cilindro('tampa', R * 0.6, H - 0.15, H + 0.02, M_TAMPA, bev=0.05))
    for p in pecas: p.parent = pai
    return pai

# ---------------- geometria (mesmas medidas da prévia) ----------------
PW, PH, F = 15.0, 20.0, 2.6
KX, KY, KR, KH, KFL = 0.0, 3.7, 6.1, 4.2, 18
RL = 9.7
TX, RX, SX, MX = -8.6, -1.9, 5.4, 11.0
PR, PRH, PFL = 1.45, 1.6, 14     # knobs pequenos (RATE e MIX iguais)
PY = -14.4
LY = -11.3

def lamp_xy(i):
    a = math.radians(135 - 27 * i)
    return KX - RL * math.sin(a), KY + RL * math.cos(a)

def painel(catcher=False):
    p = caixa('painel', -PW, -PH, PW, PH, -0.6, 0.0, M_PAINEL, bev=0.03)
    p.is_shadow_catcher = catcher
    return p

def madeiras():
    caixa('mad_esq', -PW - F, -PH - F, -PW, PH + F, -2.0, 1.1, madeira('mad_v1', 'y'), bev=0.35)
    caixa('mad_dir', PW, -PH - F, PW + F, PH + F, -2.0, 1.1, madeira('mad_v2', 'y'), bev=0.35)
    caixa('mad_sup', -PW, PH, PW, PH + F, -2.0, 1.1, madeira('mad_h1', 'x'), bev=0.35)
    caixa('mad_inf', -PW, -PH - F, PW, -PH, -2.0, 1.1, madeira('mad_h2', 'x'), bev=0.35)

def faixa(catcher=False):
    f = caixa('faixa', -13.2, -18.8, 13.2, -10.0, -0.3, 0.02, M_PRETO, bev=0.15)
    f.is_shadow_catcher = catcher
    if catcher: f.visible_shadow = False   # a sombra da própria faixa já está no fundo (senão sai dobrada)
    return f

def cena_fundo(nome=None):
    painel(); madeiras()
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = sx * (PW - 1.1), sy * (PH - 1.1)
            esfera('paraf', 0.5, x, y, -0.15, M_CROMO, achat=0.45)
            for a in (0.3, 0.3 + math.pi / 2):
                f = caixa('fenda', -0.38, -0.05, 0.38, 0.05, 0.0, 0.1, M_FENDA)
                f.location = (x, y, 0.02); f.rotation_euler.z = a
    for i in range(11):
        x, y = lamp_xy(i)
        cilindro('bezel', 0.62, 0.0, 0.28, M_CROMO, n=48, bev=0.08).location = (x, y, 0)
        esfera('lente', 0.44, x, y, 0.26, M_LAMP_OFF, achat=0.62)
        a = math.radians(135 - 27 * i); rn = RL + 1.75
        texto(str(i), KX - rn * math.sin(a), KY + rn * math.cos(a), 1.1, M_SERIG, fonte=FONTE_B)
    texto('série knob', 0, 18.1, 1.6, M_SERIG, esp=1.05, fonte=FONTE_B)
    caixa('filete', -9.5, 16.75, 9.5, 16.8, 0.0, 0.01, M_SERIG)
    if nome:
        texto(nome[0], 0, -7.0, 2.7, M_SERIG, esp=1.3, fonte=FONTE_B)
        texto(nome[1], 0, -8.85, 0.85, M_SERIG, esp=1.3, fonte=FONTE_M)
    faixa()
    for x in (-12.5, 12.5):
        for y in (-18.1, -10.7):
            esfera('pf', 0.22, x, y, 0.0, M_CROMO, achat=0.5)
    texto('TRIGGER', TX, LY, 0.8, esp=1.2, fonte=FONTE_B)
    cilindro('porca', 0.78, 0.02, 0.32, M_CROMO, n=6, bev=0.05).location = (TX, -14.3, 0)
    cilindro('bucha', 0.48, 0.32, 0.75, M_CROMO, n=48, bev=0.03).location = (TX, -14.3, 0)
    for s, x, y in (('INT', TX - 2.6, -14.3), ('MIDI', TX, -12.45), ('ÁUDIO', TX + 2.9, -14.3)):
        texto(s, x, y, 0.62, esp=1.1, fonte=FONTE_M)
    texto('RATE', RX, LY, 0.8, esp=1.2, fonte=FONTE_B)
    for i in range(7):
        tk = caixa('tick', -0.04, 2.15, 0.04, 2.4, 0, 0.01, M_TINTA)
        tk.location = (RX, PY, 0); tk.rotation_euler.z = math.radians(135 - 45 * i)
    texto('SHAPE', SX, LY, 0.8, esp=1.2, fonte=FONTE_B)
    caixa('moldura_visor', SX - 3.1, -16.0, SX + 3.1, -12.3, 0.0, 0.12, M_CROMO, bev=0.06)
    caixa('visor', SX - 2.85, -15.75, SX + 2.85, -12.55, 0.0, 0.14, M_VIDRO)
    texto('MIX', MX, LY, 0.8, esp=1.2, fonte=FONTE_B)
    for i in range(11):   # escala do MIX (0 a 100%)
        tk = caixa('tick', -0.03, 1.95, 0.03, 2.15, 0, 0.01, M_TINTA)
        tk.location = (MX, PY, 0); tk.rotation_euler.z = math.radians(135 - 27 * i)
    caixa('fundo', -60, -60, 60, 60, -2.2, -2.1, principled('fundo', (0.015, 0.014, 0.013), rough=0.9))

def alavanca(estado):
    alav = bpy.data.objects.new('alavanca', None); sc.collection.objects.link(alav)
    alav.location = (TX, -14.3, 0.7)
    if estado == 'audio': alav.rotation_euler.y = math.radians(32)
    elif estado == 'int': alav.rotation_euler.y = math.radians(-32)
    else: alav.rotation_euler.x = math.radians(-32)
    bm = bmesh.new(); bmesh.ops.create_cone(bm, cap_ends=True, segments=40, radius1=0.26, radius2=0.16, depth=2.4)
    me = bpy.data.meshes.new('bat'); bm.to_mesh(me); bm.free()
    bat = suave(obj('bat', me, M_CROMO)); bat.location = (0, 0, 1.2); bat.parent = alav
    ponta = esfera('ponta', 0.3, 0, 0, 2.4, M_CROMO); ponta.parent = alav
    return alav

# ---------------- luz, câmera ----------------
def luz_camera():
    w = bpy.data.worlds.new('mundo'); sc.world = w; w.use_nodes = True
    env = w.node_tree.nodes.new('ShaderNodeTexEnvironment'); env.image = bpy.data.images.load(TEX + '/studio.hdr')
    mp = w.node_tree.nodes.new('ShaderNodeMapping'); mp.inputs['Rotation'].default_value = (0, 0, math.radians(200))
    tc = w.node_tree.nodes.new('ShaderNodeTexCoord')
    w.node_tree.links.new(tc.outputs['Generated'], mp.inputs['Vector']); w.node_tree.links.new(mp.outputs['Vector'], env.inputs['Vector'])
    bg = w.node_tree.nodes['Background']; bg.inputs['Strength'].default_value = 0.9
    w.node_tree.links.new(env.outputs['Color'], bg.inputs['Color'])
    sol = bpy.data.lights.new('sol', 'SUN'); sol.energy = 2.6; sol.angle = math.radians(6)
    so = bpy.data.objects.new('sol', sol); sc.collection.objects.link(so)
    so.rotation_euler = (math.radians(38), 0, math.radians(-40))
    cam = bpy.data.cameras.new('cam'); cam.lens = 100; cam.sensor_fit = 'VERTICAL'; cam.sensor_height = 36
    INC, DIST = math.radians(11), 146
    co = bpy.data.objects.new('cam', cam); sc.collection.objects.link(co)
    co.location = (0, -DIST * math.sin(INC), DIST * math.cos(INC)); co.rotation_euler = (INC, 0, 0); sc.camera = co
    r = sc.render; r.engine = 'CYCLES'
    r.resolution_x, r.resolution_y = 1000, int(1000 * (PH + F) / (PW + F)); r.resolution_percentage = 100
    sc.cycles.device = 'CPU'; sc.cycles.samples = AMOSTRAS; sc.cycles.use_adaptive_sampling = True
    sc.cycles.adaptive_threshold = 0.03; sc.cycles.use_denoising = True; sc.cycles.denoiser = 'OPENIMAGEDENOISE'
    sc.cycles.max_bounces = 6; sc.cycles.seed = 7
    sc.view_settings.view_transform = 'AgX'; sc.view_settings.look = 'AgX - Medium High Contrast'
    sc.render.image_settings.file_format = 'PNG'; sc.render.image_settings.color_mode = 'RGBA'
    bpy.context.view_layer.update()   # sem isso a matriz da câmera ainda é a identidade e a projeção sai errada
    return co

def px(co, x, y, z):
    v = world_to_camera_view(sc, co, Vector((x, y, z)))
    return [round(v.x * sc.render.resolution_x, 2), round((1 - v.y) * sc.render.resolution_y, 2)]

def borda(co, x0, y0, x1, y1, z1=0.0):
    """renderiza só o retângulo (coordenadas do mundo, do chão até z1) — o resto sai transparente"""
    pts = [px(co, x, y, z) for x in (x0, x1) for y in (y0, y1) for z in (0.0, z1)]
    W, H = sc.render.resolution_x, sc.render.resolution_y
    xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
    r = sc.render; r.use_border = True; r.use_crop_to_border = False
    r.border_min_x = max(0, min(xs) / W); r.border_max_x = min(1, max(xs) / W)
    r.border_min_y = max(0, 1 - max(ys) / H); r.border_max_y = min(1, 1 - min(ys) / H)

def render(arq):
    sc.render.filepath = os.path.join(SAIDA, arq)
    bpy.ops.render.render(write_still=True)

# ---------------- modos ----------------
if MODO == 'mapa':
    co = luz_camera()
    m = {'tam': [sc.render.resolution_x, sc.render.resolution_y]}
    m['lampadas'] = []
    for i in range(11):
        x, y = lamp_xy(i)
        c = px(co, x, y, 0.53); e = px(co, x + 0.44, y, 0.53); t = px(co, x, y + 0.44, 0.53)
        m['lampadas'].append({'c': c, 'rx': round(e[0] - c[0], 2), 'ry': round(c[1] - t[1], 2)})
    def eixo(nome, x, y, z, r):
        c = px(co, x, y, z); e = px(co, x + r, y, z); t = px(co, x, y + r, z)
        m[nome] = {'c': c, 'rx': round(e[0] - c[0], 3), 'ry': round(c[1] - t[1], 3)}   # pixels por 1 unidade de raio
    eixo('knob_tampa', KX, KY, KH + 0.02, 1.0)
    eixo('knob_saia', KX, KY, 0.45, 1.0)
    eixo('knob_base', KX, KY, 0.0, 1.0)
    eixo('rate_tampa', RX, PY, PRH + 0.02, 1.0)
    eixo('mix_tampa', MX, PY, PRH + 0.02, 1.0)
    for nome, x, y in (('txt_nome', 0, -7.0), ('txt_sub', 0, -8.85), ('txt_trigger', TX, -17.05), ('txt_rate', RX, -17.05),
                       ('txt_shape', SX, -17.05), ('txt_mix', MX, -17.05)):
        m[nome] = px(co, x, y, 0.03)
    m['visor'] = [px(co, SX - 2.85, -12.55, 0.14), px(co, SX + 2.85, -15.75, 0.14)]
    m['alavanca'] = px(co, TX, -14.3, 0.7)
    m['un'] = round(px(co, 1, 0, 0)[0] - px(co, 0, 0, 0)[0], 3)   # pixels por unidade no plano do painel
    # retângulos dos recortes (x0, y0, x1, y1 em pixels), com folga para a sombra (cai para baixo e à direita)
    def ret(x0, y0, x1, y1, z1):
        pts = [px(co, x, y, z) for x in (x0, x1) for y in (y0, y1) for z in (0.0, z1)]
        return [math.floor(min(p[0] for p in pts)), math.floor(min(p[1] for p in pts)),
                math.ceil(max(p[0] for p in pts)), math.ceil(max(p[1] for p in pts))]
    m['ret'] = {
        'knob': ret(KX - KR * 1.22 - 0.6, KY - KR * 1.22 - 4.0, KX + KR * 1.22 + 3.5, KY + KR * 1.22 + 0.6, KH + 0.2),
        'rate': ret(RX - PR * 1.22 - 0.4, PY - PR * 1.22 - 1.6, RX + PR * 1.22 + 1.4, PY + PR * 1.22 + 0.4, PRH + 0.2),
        'mix': ret(MX - PR * 1.22 - 0.4, PY - PR * 1.22 - 1.6, MX + PR * 1.22 + 1.4, PY + PR * 1.22 + 0.4, PRH + 0.2),
        'alav': ret(TX - 2.2, -14.3 - 2.6, TX + 2.6, -14.3 + 2.4, 3.2),
        'nome': ret(-12.5, -9.6, 12.5, -5.4, 0.1),
    }
    with open(os.path.join(SAIDA, 'mapa.json'), 'w', encoding='utf-8') as f:
        json.dump(m, f, indent=1)
    print('mapa gravado')

elif MODO == 'fundo':
    cena_fundo(); luz_camera(); render('fundo.png')

elif MODO == 'nome':
    cena_fundo(); co = luz_camera()
    borda(co, -12.5, -9.6, 12.5, -5.4, 0.1)
    for pd, nm in PEDAIS.items():
        for o in [o for o in sc.objects if o.name.startswith('t_NOME_')]:
            bpy.data.objects.remove(o)
        a = texto(nm[0], 0, -7.0, 2.7, M_SERIG, esp=1.3, fonte=FONTE_B); a.name = 't_NOME_a'
        b = texto(nm[1], 0, -8.85, 0.85, M_SERIG, esp=1.3, fonte=FONTE_M); b.name = 't_NOME_b'
        render(f'nome_{pd}.png')

elif MODO in ('knob', 'rate', 'mix'):
    painel(catcher=True); madeiras()
    if MODO != 'knob': faixa(catcher=True)
    co = luz_camera(); sc.render.film_transparent = True
    if MODO == 'knob':
        k = knob(KX, KY, KR, KH, KFL); per = 2 * math.pi / KFL
        borda(co, KX - KR * 1.22 - 0.6, KY - KR * 1.22 - 4.0, KX + KR * 1.22 + 3.5, KY + KR * 1.22 + 0.6, KH + 0.2)
    else:
        cx = RX if MODO == 'rate' else MX
        k = knob(cx, PY, PR, PRH, PFL); per = 2 * math.pi / PFL
        borda(co, cx - PR * 1.22 - 0.4, PY - PR * 1.22 - 1.6, cx + PR * 1.22 + 1.4, PY + PR * 1.22 + 0.4, PRH + 0.2)
    for q in range(QUADROS):
        k.rotation_euler.z = per * q / QUADROS
        render(f'{MODO}_{q:02d}.png')

elif MODO == 'alav':
    painel(catcher=True); faixa(catcher=True)
    cilindro('porca', 0.78, 0.02, 0.32, M_CROMO, n=6, bev=0.05).location = (TX, -14.3, 0)
    cilindro('bucha', 0.48, 0.32, 0.75, M_CROMO, n=48, bev=0.03).location = (TX, -14.3, 0)
    co = luz_camera(); sc.render.film_transparent = True
    borda(co, TX - 2.2, -14.3 - 2.6, TX + 2.6, -14.3 + 2.4, 3.2)
    for est in ('int', 'midi', 'audio'):
        a = alavanca(est)
        render(f'alav_{est}.png')
        for o in list(a.children) + [a]: bpy.data.objects.remove(o)

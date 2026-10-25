# INSTRUMENTOS - peças de hardware renderizadas no Blender/Cycles (mesma luz e materiais da SÉRIE KNOB)
#
#   blender -b -P pecas.py -- <peça> <pasta de saída> <pasta das texturas> [amostras]
#
# peças:
#   knob_preto -> knob_preto_00..15.png: knob Moog preto canelado com tampa de alumínio torneado
#   knob_alu   -> knob_alu_00..15.png: knob de alumínio usinado canelado com tampa preta (o da SÉRIE KNOB)
#                 16 quadros de UMA canelura (as caneluras se repetem); quadro q = giro de q/16 canelura no sentido
#                 horário. O risco do ponteiro a interface desenha por cima (centro e raio da face em pecas.json).
#   lampada    -> lampada_off.png (aro cromado + lente facetada apagada) e lampada_luz.png (só a lente acesa, branca;
#                 a interface tinge na cor do LED)
#   led        -> led_off.png / led_luz.png: LED de 3 mm em domo, com o porta-LED preto
#   tecla      -> tecla.png / tecla_baixa.png: tecla de plástico escuro (a interface estica em 9 partes);
#                 tecla_luz.png: só o topo translúcido aceso (tingido pela interface)
#   madeira    -> madeira_esq.png / madeira_dir.png: lateral de nogueira envernizada
# Grava pecas.json (proporções que a interface precisa) na pasta de saída.
import bpy, bmesh, math, sys, json, os
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view

argv = sys.argv[sys.argv.index('--') + 1:]
PECA, SAIDA, TEX = argv[0], argv[1], argv[2]
AMOSTRAS = int(argv[3]) if len(argv) > 3 else 64
os.makedirs(SAIDA, exist_ok=True)
QUADROS = int(os.environ.get('QUADROS', '16'))
INC = math.radians(11)   # inclinação da câmera (a mesma da SÉRIE KNOB): a lateral do knob aparece embaixo

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene

# ---------------- materiais ----------------
def principled(nome, base, metal=0.0, rough=0.5, coat=0.0, coat_rough=0.05, emis=None, forca=0.0, transm=0.0, ior=1.45):
    m = bpy.data.materials.new(nome); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*base, 1)
    b.inputs['Metallic'].default_value = metal
    b.inputs['Roughness'].default_value = rough
    b.inputs['Coat Weight'].default_value = coat
    b.inputs['Coat Roughness'].default_value = coat_rough
    b.inputs['Transmission Weight'].default_value = transm
    b.inputs['IOR'].default_value = ior
    if emis:
        b.inputs['Emission Color'].default_value = (*emis, 1)
        b.inputs['Emission Strength'].default_value = forca
    return m

def textura_ruido(m, escala=(1.5, 500, 1.5), forca=0.12, radial=False, aneis=220):
    nt = m.node_tree; n = nt.nodes; l = nt.links
    b = n['Principled BSDF']
    tc = n.new('ShaderNodeTexCoord')
    if radial:
        t = n.new('ShaderNodeTexWave'); t.wave_type = 'RINGS'
        t.inputs['Scale'].default_value = aneis; t.inputs['Distortion'].default_value = 1.5
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

def madeira(nome, escala=0.1):
    m = principled(nome, (0.2, 0.08, 0.03), rough=0.4, coat=0.8, coat_rough=0.1)
    nt = m.node_tree; n = nt.nodes; l = nt.links; b = n['Principled BSDF']
    tc = n.new('ShaderNodeTexCoord'); mp = n.new('ShaderNodeMapping')
    mp.inputs['Scale'].default_value = (escala, escala, escala)
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

def torneado(m, aniso=0.85):
    """alumínio torneado: o brilho anisotrópico em volta do eixo faz o 'cone' de luz de peça usinada"""
    n = m.node_tree.nodes; b = n['Principled BSDF']
    tg = n.new('ShaderNodeTangent'); tg.direction_type = 'RADIAL'; tg.axis = 'Z'
    b.inputs['Anisotropic'].default_value = aniso
    m.node_tree.links.new(tg.outputs['Tangent'], b.inputs['Tangent'])
    return m

M_ALU = torneado(principled('alu', (0.91, 0.91, 0.92), metal=1, rough=0.28))
M_ALU_TORNO = torneado(principled('alu_torno', (0.9, 0.9, 0.91), metal=1, rough=0.3))
M_CROMO = principled('cromo', (0.95, 0.95, 0.95), metal=1, rough=0.06)
M_TAMPA = principled('tampa', (0.012, 0.012, 0.013), rough=0.28, coat=1, coat_rough=0.04)
M_BAQUELITE = principled('baquelite', (0.018, 0.018, 0.019), rough=0.32, coat=0.6, coat_rough=0.12)
M_PRETO_FOSCO = principled('preto_fosco', (0.012, 0.012, 0.012), rough=0.7)
M_PLASTICO = textura_ruido(principled('plastico', (0.028, 0.028, 0.03), rough=0.5, coat=0.3, coat_rough=0.3), escala=(40, 40, 40), forca=0.04)
M_LENTE_OFF = principled('lente_off', (0.10, 0.10, 0.105), rough=0.04, coat=1, coat_rough=0.02, transm=0.6, ior=1.5)
M_LENTE_LUZ = principled('lente_luz', (1, 1, 1), rough=0.05, coat=1, coat_rough=0.02, emis=(1, 1, 1), forca=0.55)
M_DOMO_OFF = principled('domo_off', (0.16, 0.16, 0.16), rough=0.08, coat=1, coat_rough=0.03, transm=0.5)
M_TOPO_LUZ = principled('topo_luz', (1, 1, 1), rough=0.4, emis=(1, 1, 1), forca=1.6)

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

def perfil(nome, raio_fn, n, z0, z1, mat, bev=0.05, raio_topo=None):
    """sólido de revolução (lateral reta ou cônica: raio_topo = fator do raio em cima)"""
    bm = bmesh.new(); bx, tp = [], []
    for i in range(n):
        a = 2 * math.pi * i / n; r = raio_fn(a); rt = r * (raio_topo or 1.0)
        bx.append(bm.verts.new((r * math.cos(a), r * math.sin(a), z0)))
        tp.append(bm.verts.new((rt * math.cos(a), rt * math.sin(a), z1)))
    for i in range(n):
        j = (i + 1) % n; bm.faces.new((bx[i], bx[j], tp[j], tp[i]))
    tampas = [bm.faces.new(tp), bm.faces.new(list(reversed(bx)))]
    bmesh.ops.poke(bm, faces=tampas)
    me = bpy.data.meshes.new(nome); bm.to_mesh(me); bm.free()
    return suave(obj(nome, me, mat), bev)

def cilindro(nome, r, z0, z1, mat, n=96, bev=0.04, raio_topo=None):
    """cilindro com tampas inteiras (n-gono): sem o leque de triângulos, a face de cima não ganha risco no sombreado"""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=n, radius1=r, radius2=r * (raio_topo or 1.0), depth=z1 - z0)
    for v in bm.verts: v.co.z += (z0 + z1) / 2
    me = bpy.data.meshes.new(nome); bm.to_mesh(me); bm.free()
    return suave(obj(nome, me, mat), bev)

def esfera(nome, r, x, y, z, mat, achat=1.0, seg=48):
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=seg, v_segments=seg // 2, radius=r)
    me = bpy.data.meshes.new(nome); bm.to_mesh(me); bm.free()
    o = suave(obj(nome, me, mat)); o.location = (x, y, z); o.scale.z = achat
    return o

def caixa_arred(nome, w, h, z0, z1, raio, mat, bev=0.04):
    """caixa de cantos arredondados (planta w × h centrada na origem)"""
    bm = bmesh.new(); vb, vt = [], []
    pts = []
    for cx, cy, a0 in ((w / 2 - raio, h / 2 - raio, 0), (-w / 2 + raio, h / 2 - raio, 90),
                       (-w / 2 + raio, -h / 2 + raio, 180), (w / 2 - raio, -h / 2 + raio, 270)):
        for k in range(10):
            a = math.radians(a0 + 90 * k / 9)
            pts.append((cx + raio * math.cos(a), cy + raio * math.sin(a)))
    for x, y in pts:
        vb.append(bm.verts.new((x, y, z0))); vt.append(bm.verts.new((x, y, z1)))
    n = len(pts)
    for i in range(n):
        j = (i + 1) % n; bm.faces.new((vb[i], vb[j], vt[j], vt[i]))
    bm.faces.new(vt); bm.faces.new(list(reversed(vb)))
    me = bpy.data.meshes.new(nome); bm.to_mesh(me); bm.free()
    o = obj(nome, me, mat)
    md = o.modifiers.new('bev', 'BEVEL'); md.width = bev; md.segments = 4
    md.limit_method = 'ANGLE'; md.angle_limit = math.radians(50); md.harden_normals = True
    for p in o.data.polygons: p.use_smooth = True
    return o

def chao(cor=(0.5, 0.5, 0.5)):
    """painel que só recebe a sombra (sai transparente)"""
    bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=50)
    me = bpy.data.meshes.new('chao'); bm.to_mesh(me); bm.free()
    o = obj('chao', me, principled('chao', cor, rough=0.5)); o.is_shadow_catcher = True
    return o

# ---------------- luz, câmera (ortográfica, inclinada como na SÉRIE KNOB) ----------------
def luz_camera(lado_x, lado_y, px_x, px_y, centro=(0, 0), inclina=True):
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
    for nome, pos, tam, forca in (('softbox', (-7, 9, 16), 10, 1800), ('rebatedor', (8, -6, 14), 8, 500)):
        ld = bpy.data.lights.new(nome, 'AREA'); ld.shape = 'RECTANGLE'; ld.size = tam; ld.size_y = tam * 0.6; ld.energy = forca
        lo = bpy.data.objects.new(nome, ld); sc.collection.objects.link(lo); lo.location = pos
        lo.rotation_euler = (Vector((0, 0, 0)) - Vector(pos)).to_track_quat('-Z', 'Y').to_euler()
    cam = bpy.data.cameras.new('cam'); cam.type = 'ORTHO'
    cam.ortho_scale = max(lado_x, lado_y); cam.sensor_fit = 'AUTO'
    co = bpy.data.objects.new('cam', cam); sc.collection.objects.link(co)
    inc = INC if inclina else 0.0
    D = 60
    co.location = (centro[0], centro[1] - D * math.sin(inc), D * math.cos(inc)); co.rotation_euler = (inc, 0, 0); sc.camera = co
    r = sc.render; r.engine = 'CYCLES'
    r.resolution_x, r.resolution_y = px_x, px_y; r.resolution_percentage = 100
    sc.cycles.device = 'CPU'; sc.cycles.samples = AMOSTRAS; sc.cycles.use_adaptive_sampling = True
    sc.cycles.adaptive_threshold = 0.02; sc.cycles.use_denoising = True; sc.cycles.denoiser = 'OPENIMAGEDENOISE'
    sc.cycles.max_bounces = 8; sc.cycles.transmission_bounces = 8; sc.cycles.seed = 7
    sc.view_settings.view_transform = 'AgX'; sc.view_settings.look = 'AgX - Medium High Contrast'
    sc.render.image_settings.file_format = 'PNG'; sc.render.image_settings.color_mode = 'RGBA'
    sc.render.film_transparent = True
    bpy.context.view_layer.update()
    return co

def px(co, x, y, z):
    v = world_to_camera_view(sc, co, Vector((x, y, z)))
    return (v.x * sc.render.resolution_x, (1 - v.y) * sc.render.resolution_y)

def render(arq):
    sc.render.filepath = os.path.join(SAIDA, arq)
    bpy.ops.render.render(write_still=True)

def grava_json(chave, dados):
    arq = os.path.join(SAIDA, 'pecas.json')
    j = json.load(open(arq, encoding='utf-8')) if os.path.exists(arq) else {}
    j[chave] = dados
    json.dump(j, open(arq, 'w', encoding='utf-8'), indent=1, ensure_ascii=False)

# ---------------- knobs (raio 1 = raio do corpo) ----------------
LADO_KNOB, PX_KNOB = 3.2, 320          # o quadro cobre 3,2 raios: sobra para a sombra (cai para baixo e à direita)
CENTRO_KNOB = (0.18, -0.22)            # a câmera mira um pouco abaixo/à direita do knob, onde a sombra cai

def knob_preto():
    """Moog: corpo de baquelite preto canelado, levemente cônico; face de cima côncava e tampa de alumínio torneado"""
    fl, H = 30, 0.72
    pai = bpy.data.objects.new('knob', None); sc.collection.objects.link(pai)
    prof = 0.045
    corpo = perfil('corpo', lambda a: 1.0 - prof * (0.5 - 0.5 * math.cos(fl * a)), fl * 12, 0.0, H, M_BAQUELITE, bev=0.07, raio_topo=0.93)
    saia = cilindro('saia', 1.08, 0.0, 0.12, M_BAQUELITE, bev=0.04)
    face = cilindro('face', 0.83, H - 0.05, H + 0.005, M_BAQUELITE, bev=0.03)
    tampa = cilindro('tampa', 0.5, H - 0.02, H + 0.035, M_ALU_TORNO, bev=0.015)
    for p in (corpo, saia, face, tampa): p.parent = pai
    return pai, fl, H, 0.83

def knob_alu():
    """o da SÉRIE KNOB: saia e corpo de alumínio usinado, canelado, com tampa preta brilhante"""
    fl, H = 18, 0.69
    pai = bpy.data.objects.new('knob', None); sc.collection.objects.link(pai)
    prof = 0.045
    saia = cilindro('saia', 1.22, 0.0, 0.074, M_ALU, bev=0.01)
    corpo = perfil('corpo', lambda a: 1.0 - prof * (0.5 - 0.5 * math.cos(fl * a)), fl * 12, 0.074, H, M_ALU, bev=0.026)
    tampa = cilindro('tampa', 0.6, H - 0.025, H + 0.003, M_TAMPA, bev=0.008)
    for p in (saia, corpo, tampa): p.parent = pai
    return pai, fl, H, 0.6

def faz_knob(nome, fabrica):
    chao()
    co = luz_camera(LADO_KNOB, LADO_KNOB, PX_KNOB, PX_KNOB, CENTRO_KNOB)
    k, fl, H, rface = fabrica()
    per = 2 * math.pi / fl
    for q in range(QUADROS):
        k.rotation_euler.z = -per * q / QUADROS     # horário visto de cima
        render(f'{nome}_{q:02d}.png')
    c = px(co, 0, 0, H); b = px(co, 0, 0, 0); e = px(co, 1.0, 0, 0)
    grava_json(nome, {'quadros': QUADROS, 'caneluras': fl, 'tam': PX_KNOB,
                      'centro_face': [round(c[0] / PX_KNOB, 4), round(c[1] / PX_KNOB, 4)],
                      'centro_base': [round(b[0] / PX_KNOB, 4), round(b[1] / PX_KNOB, 4)],
                      'raio': round((e[0] - b[0]) / PX_KNOB, 4), 'raio_face': rface})

# ---------------- lâmpada (jewel) e LED ----------------
def faz_lampada():
    chao()
    co = luz_camera(3.4, 3.4, 160, 160, (0.1, -0.12))
    aro = cilindro('aro', 1.0, 0.0, 0.42, M_CROMO, n=64, bev=0.12)
    lente = esfera('lente', 0.74, 0, 0, 0.36, M_LENTE_OFF, achat=0.7, seg=12)   # poucas faces = lente facetada
    for p in lente.data.polygons: p.use_smooth = False
    render('lampada_off.png')
    lente.data.materials[0] = M_LENTE_LUZ
    aro.is_holdout = True
    sc.objects['chao'].hide_render = True
    render('lampada_luz.png')
    c = px(co, 0, 0, 0.36); e = px(co, 0.74, 0, 0.36)
    grava_json('lampada', {'tam': 160, 'centro': [round(c[0] / 160, 4), round(c[1] / 160, 4)], 'raio_lente': round((e[0] - c[0]) / 160, 4)})

def faz_led():
    chao()
    co = luz_camera(3.0, 3.0, 96, 96, (0.08, -0.1))
    porta = cilindro('porta', 1.0, 0.0, 0.22, M_PRETO_FOSCO, n=48, bev=0.08)
    domo = esfera('domo', 0.7, 0, 0, 0.18, M_DOMO_OFF, achat=0.85, seg=40)
    render('led_off.png')
    domo.data.materials[0] = M_LENTE_LUZ
    porta.is_holdout = True
    sc.objects['chao'].hide_render = True
    render('led_luz.png')
    c = px(co, 0, 0, 0.2); e = px(co, 0.7, 0, 0.2)
    grava_json('led', {'tam': 96, 'centro': [round(c[0] / 96, 4), round(c[1] / 96, 4)], 'raio_domo': round((e[0] - c[0]) / 96, 4)})

# ---------------- tecla (esticada em 9 partes pela interface) ----------------
def faz_tecla():
    W, Ht, R = 3.0, 2.0, 0.42
    LX, LY = W + 0.9, Ht + 0.9
    PXU = 64                                 # pixels por unidade
    chao()
    co = luz_camera(LX, LY, round(LX * PXU), round(LY * PXU), (0.08, -0.12), inclina=False)
    poco = caixa_arred('poco', W + 0.16, Ht + 0.16, -0.05, 0.02, R + 0.08, M_PRETO_FOSCO, bev=0.02)
    tecla = caixa_arred('tecla', W, Ht, 0.0, 0.42, R, M_PLASTICO, bev=0.1)
    render('tecla.png')
    tecla.scale.z = 0.45
    render('tecla_baixa.png')
    tecla.scale.z = 1.0
    topo = caixa_arred('topo', W - 0.36, Ht - 0.36, 0.40, 0.425, R - 0.18, M_TOPO_LUZ, bev=0.02)
    for o in (tecla, poco): o.is_holdout = True
    sc.objects['chao'].hide_render = True
    render('tecla_luz.png')
    # fatias da esticada: margem + raio + chanfro, em pixels da imagem
    a = px(co, -W / 2, Ht / 2, 0.42); b = px(co, W / 2, -Ht / 2, 0.42)
    grava_json('tecla', {'tam': [round(LX * PXU), round(LY * PXU)], 'face': [round(a[0]), round(a[1]), round(b[0]), round(b[1])],
                         'fatia': round((R + 0.12) * PXU)})

# ---------------- lateral de madeira ----------------
def faz_madeira():
    W, H = 1.0, 24.0
    PXU = 72
    co = luz_camera(W + 0.02, H + 0.02, round((W + 0.02) * PXU), round((H + 0.02) * PXU), inclina=False)
    sc.render.film_transparent = False
    for lado in ('esq', 'dir'):
        for o in [o for o in sc.objects if o.name.startswith('mad')]: bpy.data.objects.remove(o)
        m = madeira('mad_' + lado, escala=0.06)
        nt = m.node_tree; mp = [n for n in nt.nodes if n.type == 'MAPPING'][0]
        mp.inputs['Location'].default_value = (0.3 if lado == 'esq' else 0.7, 0.1, 0)
        b = bpy.data.objects.new('madeira', None)
        bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1)
        for v in bm.verts: v.co = Vector((v.co.x * W, v.co.y * H, v.co.z * 1.2 - 0.6))
        me = bpy.data.meshes.new('mad'); bm.to_mesh(me); bm.free()
        o = suave(obj('mad_' + lado, me, m), 0.18, 5)
        render(f'madeira_{lado}.png')
    grava_json('madeira', {'tam': [round((W + 0.02) * PXU), round((H + 0.02) * PXU)]})

if PECA == 'knob_preto': faz_knob('knob_preto', knob_preto)
elif PECA == 'knob_alu': faz_knob('knob_alu', knob_alu)
elif PECA == 'lampada': faz_lampada()
elif PECA == 'led': faz_led()
elif PECA == 'tecla': faz_tecla()
elif PECA == 'madeira': faz_madeira()
else: raise SystemExit('peça desconhecida: ' + PECA)
print('ok', PECA)

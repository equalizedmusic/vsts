# MESA — desenho do canal e do MASTER em centímetros (a mesma medida do render 3D aprovado em 30/10).
# Fonte única: o render_mesa.py (Blender) usa para montar as chapas e o gera_layout.py escreve o Source/Layout.h.
# x = a partir do centro do módulo; y = para cima (o topo da ponte de VUs está em +1.8, o pé do fader em -45).

SW = 3.8                 # largura de um canal
SURF = 0.15              # altura da chapa
GAP = 0.05               # fresta entre módulos
Y_BR_TOP, Y_BR_BOT = 1.8, -4.4
SEC = dict(pre=(-4.8, -11.4), eq=(-11.4, -21.9), dyn=(-21.9, -26.3), out=(-26.3, -31.0), fad=(-31.0, -45.0))
MW = 14.0                # largura do MASTER
MSEC = dict(logo=(-4.8, -11.4), cola=(-11.4, -26.3), out=(-26.3, -45.0))

# ----------------------------------------------------------------------------------------- canal
VU_CH = dict(x=0.0, y=-0.9, w=3.1, h=2.5)          # VU da ponte (face)
NUM_Y = -3.15
SIG = (-1.2, -3.15)
PEAK = (1.2, -3.15)
SEL_Y = -3.65

PRE_NAME_Y = -5.55
PRE_KNOBS = dict(gain=(0.0, -7.55, 1.75), drive=(-0.95, -9.75, 1.1), hpf=(0.95, -9.75, 1.1))
PRE_PAD = (0.0, -9.75, 0.5)
PRE_LABELS = [('GAIN', 0.0, -8.95, 0.27), ('DRIVE', -0.95, -10.65, 0.22), ('HPF', 0.95, -10.65, 0.22), ('PAD', 0.0, -10.25, 0.18)]

EQ_NAME_Y = -12.0
EQ_ROWS = [('hf', 'HF', 'kHz', 'red'), ('hmf', 'HMF', 'kHz', 'grn'), ('lmf', 'LMF', 'Hz', 'blu'), ('lf', 'LF', 'Hz', 'brn')]
def eq_row_y(k): return -13.55 - k * 2.0
EQ_GAIN_X, EQ_GAIN_D = -0.92, 1.3
EQ_FREQ_X, EQ_FREQ_D = 0.92, 1.12
EQ_BTN = (-0.7, -21.15, 0.62)
DYNPRE_BTN = (0.7, -21.15, 0.62)    # PRÉ: dinâmica antes do EQ

DYN_NAME_Y = -22.5
DYN_KNOBS_Y, DYN_D = -23.95, 1.0
DYN_X = [-1.15, 0.0, 1.15]
GR_LEDS = [(-1.0 + k * 0.5, -25.65) for k in range(5)]
GR_LED_W, GR_LED_H = 0.3, 0.2

G_BTNS = [(-1.0 + k * 1.0, -26.95, 0.66) for k in range(3)]
PAN = (0.0, -28.55, 1.3)
MUTE = (-0.75, -30.35, 0.62)
SOLO = (0.75, -30.35, 0.62)

FAD_X, FAD_YT, FAD_YB = -0.3, -31.9, -42.6
FAD_CAP_W, FAD_CAP_H = 1.25, 2.0
FAD_MARKS = [['+10', 0], ['+5', .1], ['0', .22], ['5', .34], ['10', .46], ['20', .62], ['30', .75], ['50', .88], ['∞', 1]]
METER_X = [0.95, 1.35]
METER_N = 20
METER_W = 0.3
FAD_DB = (1.15, -43.2)
TAPE = (0.0, -44.2, 3.45, 1.0)
DOT = (-1.42, -44.2, 0.36)

# ----------------------------------------------------------------------------------------- MASTER
MVU = [(-3.3, -1.25, 5.6, 4.4), (3.3, -1.25, 5.6, 4.4)]
MODE_BTNS = [(1.4 + k * 1.75, -6.4, 0.75) for k in range(3)]          # CANAL GRUPO MASTER
OPT_BTNS = [(1.4 + k * 1.75, -9.5, 0.62) for k in range(4)]           # VU PPM ECO HQ
COLA_VU = (0.0, -14.9, 4.6, 3.2)
COLA_KNOBS = [('thr', -4.0, -19.2, 'gry'), ('atk', 0.0, -19.2, 'blk'), ('rel', 4.0, -19.2, 'blk'),
              ('mkp', -4.0, -22.9, 'org'), ('rat', 0.0, -22.9, 'blk'), ('mix', 4.0, -22.9, 'gry')]
COLA_D = 1.9
COLA_IN = (-4.0, -25.6, 0.66)
COLA_HPF = (2.9, -25.6, 0.66)
MFAD_X, MFAD_YT, MFAD_YB = -1.4, -28.6, -42.6
MFAD_CAP_W, MFAD_CAP_H = 1.6, 2.3
MMETER_X = [1.4, 2.2]
MMETER_N = 24
MMETER_W = 0.55
MREAD = [(4.6, -30.6 - k * 2.2) for k in range(3)]                     # FADER, PICO, GR (valores)
MTAPE = (-1.4, -44.2, 4.0, 1.0)

# ----------------------------------------------------------------------------------------- console
VIS = 12                 # canais à vista (mais que isso: rolagem)
WOOD_W = 2.2
EDGE = 0.3
X_CH0 = WOOD_W + EDGE                        # borda esquerda do 1º canal (a partir da borda esquerda da janela)
X_GP0 = X_CH0 + VIS * SW + 1.0               # 1º grupo
X_M0 = X_GP0 + 3 * SW + 1.0                  # MASTER
TOTAL_W = X_M0 + MW + EDGE + WOOD_W
TOP_FRAME = 1.6                              # madeira em cima da ponte
Y_TOP = Y_BR_TOP + TOP_FRAME                 # y (cm, do render) da borda de cima da janela
ARM = 4.6                                    # descanso de braço
Y_BOT = -45.0 - ARM
TOTAL_H = Y_TOP - Y_BOT

# famílias de knob por modelo (como no render aprovado)
PRE_FAM = {'LIMPO': 'ssl', 'API 312': 'api', 'BRIT 73': 'neve'}
EQ_FAM = {'E-4K': 'ssl', 'BRIT 81': 'neve'}
PANEL = {'ssl': 'ssl', 'api': 'api', 'neve': 'neve'}

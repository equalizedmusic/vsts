#!/usr/bin/env python3
"""Gera o banco FACTORY 1 do LIMÃO: Source/presets/fabrica.inc (40 presets por categoria, 9 categorias).

Uso:
    python gerar-presets.py

Cada preset traz o som E o padrão do sequenciador. Os presets feitos à mão (MANUAIS) entram primeiro na
categoria; o resto sai de receitas por categoria (faixas de parâmetro + jeito de montar a linha), com
semente fixa pelo nome — rodar de novo dá exatamente o mesmo arquivo.
Padrão em texto: notas em semitons a partir do Dó 2 (−12..24), "-" = pausa, "~" = ligadura;
acc/sld com "x" no passo marcado.
"""
import random
import zlib
from pathlib import Path

CATEGORIAS = ["CLÁSSICO", "ACID HOUSE", "ACID TECHNO", "TRANCE", "BASS", "LEAD", "SQUELCH", "DUB", "FX"]
POR_CATEGORIA = 40

# faixas dos parâmetros (iguais ao createLayout de Params.h)
FAIXAS = {
    "wave": (0, 1), "tune": (-12, 12), "cutoff": (40, 12000), "reso": (0, 100), "envmod": (0, 100),
    "decay": (30, 2500), "accent": (0, 100), "slide": (10, 400), "acid": (0, 100),
    "drvtype": (0, 2), "drive": (0, 100), "dlon": (0, 1), "dldiv": (0, 5), "dlfb": (0, 95), "dlmix": (0, 100),
    "rvon": (0, 1), "rvsize": (0, 100), "rvmix": (0, 100), "volume": (-36, 6),
    "seqlen": (1, 32), "seqrate": (0, 4), "swing": (0, 75),
}

# ---------------- os feitos à mão (os 14 da primeira versão) ----------------
MANUAIS = [
    ("CLÁSSICO", "303 Clássico",
     {"cutoff": 460, "reso": 72, "envmod": 62, "decay": 340, "accent": 70, "slide": 60, "drive": 18},
     "0 0 12 0 ~ 3 0 15 0 10 - 0 7 0 12 3", "x...x..x...x..x.", "..x....x.....x.."),
    ("CLÁSSICO", "Minimal",
     {"cutoff": 520, "reso": 50, "envmod": 50, "decay": 200, "accent": 60, "drive": 10,
      "dlon": 1, "dldiv": 3, "dlfb": 40, "dlmix": 22},
     "0 - - 0 - - 12 - 0 - - - 3 - - -", "x.....x.....x...", "................"),
    ("ACID HOUSE", "Acid House",
     {"cutoff": 380, "reso": 82, "envmod": 70, "decay": 520, "accent": 75, "slide": 70, "drive": 30, "rvon": 1, "rvmix": 14},
     "0 12 0 0 7 - 0 10 12 0 3 0 - 5 0 12", "x.....x.x...x...", ".x.....x.x....x."),
    ("ACID HOUSE", "Garagem Swing",
     {"cutoff": 500, "reso": 68, "envmod": 60, "decay": 320, "accent": 65, "drive": 28, "swing": 45},
     "0 0 3 0 5 0 3 0 0 12 0 10 7 0 5 3", "x...x...x...x...", "....x.......x..."),
    ("ACID TECHNO", "Techno Pesado",
     {"wave": 1, "cutoff": 300, "reso": 55, "envmod": 72, "decay": 220, "accent": 80, "drvtype": 1, "drive": 58,
      "dlon": 1, "dldiv": 3, "dlfb": 35, "dlmix": 16},
     "0 0 - 0 0 - 0 12 0 0 - 0 1 0 - 0", "x..x..x.x..x..x.", "......x........."),
    ("TRANCE", "Psytrance Rolling",
     {"cutoff": 820, "reso": 42, "envmod": 45, "decay": 150, "accent": 40, "slide": 30, "drive": 25, "volume": -5},
     "- 4 4 4 - 4 4 4 - 4 4 4 - 4 4 7", ".x...x...x...x..", "................"),
    ("BASS", "Borracha",
     {"cutoff": 220, "reso": 60, "envmod": 85, "decay": 600, "accent": 50, "slide": 120, "drive": 15},
     "0 ~ 7 0 ~ 12 0 ~ 5 7 ~ 0 3 ~ 0 ~", "x.....x.....x...", "x.x..x..x.x..x.."),
    ("BASS", "Sub Grave",
     {"cutoff": 160, "reso": 30, "envmod": 35, "decay": 500, "accent": 40, "tune": -12, "drive": 8},
     "0 ~ - 0 - 0 ~ - 0 ~ - 0 3 ~ 0 -", "x.......x.......", "................"),
    ("LEAD", "Lead Gritado",
     {"wave": 1, "cutoff": 900, "reso": 86, "envmod": 58, "decay": 700, "accent": 70, "slide": 110, "drvtype": 0,
      "drive": 62, "dlon": 1, "dldiv": 3, "dlfb": 55, "dlmix": 30, "rvon": 1, "rvsize": 60, "rvmix": 22, "seqlen": 32},
     "12 ~ 15 12 ~ 19 ~ 17 15 ~ 12 10 12 ~ - 7 12 ~ 15 17 ~ 19 22 ~ 24 ~ 19 17 15 ~ 12 -",
     "x....x..x...x.....x..x..x...x...", ".x...x...x...x...x..x...x...x..."),
    ("SQUELCH", "Squelch 32",
     {"cutoff": 420, "reso": 88, "envmod": 66, "decay": 300, "accent": 85, "slide": 55, "drive": 35, "acid": 25, "seqlen": 32},
     "0 0 12 3 0 - 15 0 0 10 0 7 12 0 - 3 0 0 12 5 0 - 17 0 0 10 0 7 15 12 - 0",
     "x..x...x..x...x.x..x..x...x..x..", "..x....x...x...x..x....x..x....x"),
    ("SQUELCH", "Ácido Total",
     {"wave": 1, "cutoff": 380, "reso": 80, "envmod": 70, "decay": 260, "accent": 90, "slide": 50, "acid": 70,
      "drvtype": 1, "drive": 45, "dlon": 1, "dlmix": 18},
     "0 12 0 0 15 0 12 3 0 0 17 0 12 0 10 7", "x.x...x.x.x...x.", ".x...x...x...x.."),
    ("DUB", "Dub Ácido",
     {"cutoff": 350, "reso": 75, "envmod": 55, "decay": 450, "accent": 60, "slide": 80, "drive": 20,
      "dlon": 1, "dldiv": 3, "dlfb": 72, "dlmix": 38, "rvon": 1, "rvsize": 75, "rvmix": 28, "seqrate": 4},
     "0 - 7 ~ 10 - 0 12", "x...x...", "..x....x"),
    ("FX", "Sereia",
     {"cutoff": 900, "reso": 96, "envmod": 40, "decay": 900, "accent": 55, "slide": 180, "drive": 22,
      "dlon": 1, "dldiv": 2, "dlfb": 60, "dlmix": 25, "rvon": 1, "rvmix": 25},
     "12 ~ 24 ~ 19 ~ 12 ~ 15 ~ 22 ~ 17 ~ 12 ~", "x.......x.......", "x.x.x.x.x.x.x.x."),
    ("FX", "Bitcrush Ácido",
     {"cutoff": 600, "reso": 70, "envmod": 60, "decay": 260, "accent": 70, "drvtype": 2, "drive": 48},
     "0 12 0 15 0 12 - 10 0 12 0 7 - 3 0 5", "x...x...x...x...", "...x.......x...."),
]

# ---------------- nomes (criativos, em PT; jargão de gênero em inglês) ----------------
NOMES = {
    "CLÁSSICO": """Prata Original; Chicago 87; Caixa Prateada; Fita Cassete; Primeiro Contato; Linha Mestra; Botão de Fábrica;
        Manual de Instruções; Tecla Morta; Pilha Fraca; Bateria Nova; Passo a Passo; Velha Escola; Linha Cruzada;
        Grave de Garagem; Ácido Puro; Prato Feito; Clássico do Baile; Disco Riscado; Vinil Gasto; Lado B; Fita Dupla;
        Sinal Verde; Tom Cheio; Envelope Curto; Resposta Rápida; Dezesseis Passos; Caixa de Ritmo; Tempo Certo;
        Batida Seca; Filtro Aberto; Filtro Fechado; Moleque de 88; Fábrica de Ácido; Laranja Lima; Limão Siciliano;
        Casca Fina; Gomo Ácido; Suco Puro; Raspa de Limão""",
    "ACID HOUSE": """Warehouse; Pista Cheia; Sexta à Noite; Luz Negra; Fumaça no Salão; Smiley; Verão de 88; Rave no Galpão;
        Mão pra Cima; Clube Escuro; Estrobo; Galpão Lotado; Fim de Festa; Pulseira Neon; Sábado Ácido; Bate-Estaca Doce;
        Quadradinho; Passinho; Groove de Esquina; Baile Ácido; Copo Suado; Pé na Pista; Gingado; Sotaque House;
        Calçada Molhada; Disco Ácida; Noite Longa; Amanhecer; Pista Molhada; Corrente de Ar; Swing de Garagem;
        Cerveja Gelada; Groove Torto; Salto Alto; Balanço Ácido; Ritmo de Rua; Viela; Esquina 303; Contratempo; Requebrado""",
    "ACID TECHNO": """Concreto; Martelo; Fábrica Escura; Prensa Hidráulica; Turbina; Aço Inox; Chão de Fábrica; Bunker; Túnel;
        Sirene Industrial; Engrenagem; Rebite; Usina; Fornalha; Pistão; Correia; Torno Mecânico; Solda; Ferro Fundido;
        Britadeira; Moinho; Caldeira; Galpão 4; Linha de Montagem; Esteira; Marreta; Bigorna; Trilho; Viaduto; Subsolo;
        Porão; Ruído Branco; Pulso Duro; Ácido Bruto; Corrosivo; Ferrugem; Cimento; Asfalto; Bate-Estaca; Pressão Alta""",
    "TRANCE": """Rolling Verde; Galope; Floresta; Trilha Noturna; Lua Cheia; Fogueira; Mandala; Cachoeira; Cogumelo; Raiz;
        Serpente; Correnteza; Sol Nascente; Maré; Pé Descalço; Poeira; Tambor da Mata; Trem Noturno; Locomotiva; Rodopio;
        Hélice; Vórtice; Espiral; Pêndulo; Motor Ligado; Roda Gigante; Estrada; Batimento; Pulsação; Rolo Compressor;
        Engrenado; Cavalgada; Trote; Moto Contínuo; Ciclone; Tornado; Redemoinho; Corredeira; Ventania; Frequência""",
    "BASS": """Subsolo Grave; Peso Pesado; Fundo do Mar; Elástico; Chiclete; Goma; Balão; Barriga; Trovão; Terremoto;
        Grave Redondo; Grave Quadrado; Tapete; Almofada; Pântano; Lama; Melado; Mel; Caramelo; Rocha; Âncora; Baleia;
        Submarino; Profundo; Abismo; Caverna; Poço; Barril; Tambor; Bumbo Gordo; Muralha; Colchão; Veludo; Couro;
        Madeira Oca; Fole; Sopro Grave; Gordura; Doce de Leite; Rapadura""",
    "LEAD": """Grito; Navalha Fina; Agulha; Faísca; Relâmpago; Laser; Assobio; Canto Ácido; Sirene Alta; Chicote; Flecha;
        Cometa; Estrela; Raio; Holofote; Megafone; Alarme; Vidro; Cristal; Neon; Prata Viva; Fio Desencapado;
        Curto-Circuito; Tensão; Voltagem; Ozônio; Cítrico; Lima-da-Pérsia; Casca Amarga; Azedinho; Picante; Pimenta;
        Gengibre; Hortelã; Menta; Gelo; Arrepio; Soprano; Falsete; Solo Ácido""",
    "SQUELCH": """Borbulha; Gargarejo; Esguicho; Espremido; Chiado; Grilo; Sapo; Pato; Boca Aberta; Wah-Wah; Língua; Chupeta;
        Bolha; Efervescente; Sal de Frutas; Refrigerante; Espuma; Ácido Cítrico; Vinagre; Azedo; Careta; Mordida;
        Beliscão; Cócegas; Risada; Soluço; Engasgo; Ronco; Chorinho; Miado; Latido; Uivo; Grunhido; Rosnado; Gemido;
        Ganido; Assobio Ácido; Elástico Ácido; Squelch Total; Tchibum""",
    "DUB": """Eco Distante; Vale; Cânion; Montanha; Catedral; Câmara; Sala Vazia; Corredor; Túnel de Eco; Neblina; Nuvem;
        Vapor; Fumaça; Ressaca; Maré Baixa; Onda Lenta; Ilha; Areia; Concha; Coral; Profundezas; Mergulho; Cais; Farol;
        Navio; Âncora Solta; Rede; Balanço; Rede de Dormir; Fim de Tarde; Pôr do Sol; Brisa; Sereno; Garoa; Chuva Fina;
        Temporal; Trovoada Distante; Horizonte Ácido; Dub de Quintal; Kingston""",
    "FX": """Alienígena; Disco Voador; Radar; Sonar; Satélite; Morse; Rádio Velho; Modem; Fax; Robô; Androide; Glitch; Pixel;
        8 Bits; Fliperama; Game Over; Bug; Vírus; Estática; Interferência; Antena; Teletransporte; Buraco Negro;
        Gravidade Zero; Órbita; Meteoro; Eclipse; Big Bang; Laboratório; Experimento; Mutante; Tubo de Ensaio; Reagente;
        Cobaia; Choque; Dínamo; Bobina; Tesla; Gerador; Defeito""",
}

ESCALAS = {
    "menor": [0, 3, 5, 7, 10],
    "dorico": [0, 2, 3, 5, 7, 9, 10],
    "frigio": [0, 1, 3, 5, 7, 8, 10],
    "blues": [0, 3, 5, 6, 7, 10],
    "cromatico": list(range(12)),
}


# ---------------- montagem das linhas ----------------
class Linha:
    """Um padrão em construção: lista de passos (nota int | '-' | '~'), accents e slides."""

    def __init__(self, n):
        self.n = n
        self.notas = ["-"] * n
        self.acc = [False] * n
        self.sld = [False] * n

    def texto(self):
        notas = " ".join(str(x) for x in self.notas)
        acc = "".join("x" if a else "." for a in self.acc)
        sld = "".join("x" if s else "." for s in self.sld)
        return notas, acc, sld

    def limpar(self):
        # ligadura no 1º passo ou depois de pausa não segura nada: vira pausa; slide em pausa não existe
        for i in range(self.n):
            if self.notas[i] == "~" and (i == 0 or self.notas[i - 1] == "-"):
                self.notas[i] = "-"
            if self.notas[i] == "-":
                self.sld[i] = False
                self.acc[i] = False
        if all(x in ("-", "~") for x in self.notas):
            self.notas[0] = 0


def grau(rnd, escala, pesos=None, oitava=0.0, grave=0.0, faixa=(-12, 24)):
    """Nota da escala: muita tônica, às vezes oitava acima ou abaixo."""
    notas = ESCALAS[escala]
    pesos = pesos or [6 if x == 0 else 2 for x in notas]
    n = rnd.choices(notas, weights=pesos)[0]
    if rnd.random() < oitava:
        n += 12
    elif rnd.random() < grave:
        n -= 12
    return max(faixa[0], min(faixa[1], n))


def motivo(rnd, tam, escala, p_pausa, p_liga, p_acc, p_sld, oitava, grave=0.05, faixa=(-12, 24), base=0):
    """Um motivo de 'tam' passos; o padrão inteiro é feito repetindo e variando motivos (soa como linha, não sorteio)."""
    m = []
    for i in range(tam):
        r = rnd.random()
        if i > 0 and r < p_pausa:
            nota = "-"
        elif i > 0 and r < p_pausa + p_liga:
            nota = "~"
        else:
            nota = grau(rnd, escala, oitava=oitava, grave=grave, faixa=faixa) + base
            nota = max(faixa[0], min(faixa[1], nota))
        m.append([nota, rnd.random() < p_acc, rnd.random() < p_sld])
    if m[0][0] in ("-", "~"):
        m[0][0] = base
    m[0][1] = m[0][1] or rnd.random() < 0.6     # o "1" costuma vir acentuado
    return m


def variar(rnd, m, quanto, escala, faixa):
    out = [list(x) for x in m]
    for x in out:
        if rnd.random() < quanto:
            if isinstance(x[0], int) and rnd.random() < 0.5:
                x[0] = max(faixa[0], min(faixa[1], grau(rnd, escala, oitava=0.3, faixa=faixa)))
            else:
                x[1] = not x[1] if rnd.random() < 0.5 else x[1]
                x[2] = not x[2] if rnd.random() < 0.4 else x[2]
    return out


def montar(rnd, n, motivos):
    L = Linha(n)
    seq = [s for m in motivos for s in m][:n]
    for i, (nota, a, s) in enumerate(seq):
        L.notas[i], L.acc[i], L.sld[i] = nota, a, s
    L.limpar()
    return L


def linha_frase(rnd, n, escala, p_pausa, p_liga, p_acc, p_sld, oitava, faixa=(-12, 24), base=0, tam_motivo=None,
                quanto=0.3):
    """Frase A A' A B: o jeito clássico de linha de 303."""
    tam = tam_motivo or (4 if n <= 8 else 8 if n <= 16 else 8)
    a = motivo(rnd, tam, escala, p_pausa, p_liga, p_acc, p_sld, oitava, faixa=faixa, base=base)
    b = motivo(rnd, tam, escala, p_pausa, p_liga, p_acc, p_sld, oitava + 0.1, faixa=faixa, base=base)
    blocos = []
    forma = rnd.choice(["AA'", "AB", "AA'AB", "ABAB'", "AAAB"])
    while sum(len(x) for x in blocos) < n:
        for ch in forma.replace("'", ""):
            blocos.append(variar(rnd, a if ch == "A" else b, quanto * rnd.random(), escala, faixa))
    return montar(rnd, n, blocos)


def linha_rolling(rnd, n, escala):
    """Rolling: o bumbo no tempo (pausa) e três semicolcheias de baixo; troca de nota por compasso."""
    L = Linha(n)
    notas = ESCALAS[escala]
    galope = rnd.random() < 0.35
    raiz = 0
    for i in range(n):
        if i % 16 == 0:
            raiz = rnd.choice([0, 0, 0, notas[1], notas[-2], 7 if 7 in notas else notas[3]])
        pos = i % 4
        if pos == 0:
            L.notas[i] = "-"
        elif galope and pos == 2:
            L.notas[i] = "-"
        else:
            nota = raiz
            if pos == 3 and rnd.random() < 0.18:
                nota = raiz + rnd.choice([12, notas[2], -12 if raiz >= 0 else 0])
            L.notas[i] = max(-12, min(24, nota))
            L.acc[i] = pos == 1 and rnd.random() < 0.5
            L.sld[i] = pos == 3 and rnd.random() < 0.08
    L.limpar()
    return L


def linha_motor(rnd, n, escala, p_pausa, p_acc):
    """Acid techno: semicolcheias quase todas tocando, muita tônica, oitava e um 'grito' no fim da volta."""
    L = Linha(n)
    notas = ESCALAS[escala]
    for i in range(n):
        if i > 0 and rnd.random() < p_pausa:
            continue
        r = rnd.random()
        L.notas[i] = 0 if r < 0.55 else 12 if r < 0.72 else rnd.choice(notas[1:]) if r < 0.92 else -12
        L.acc[i] = (i % 4 == 0 and rnd.random() < 0.7) or rnd.random() < p_acc
        L.sld[i] = rnd.random() < 0.1
    fim = n - rnd.choice([2, 3, 4])
    for i in range(max(0, fim), n):
        if rnd.random() < 0.6:
            L.notas[i] = rnd.choice([12, 15, 17, 19 if 7 in notas else 15])
            L.acc[i] = True
    L.notas[0], L.acc[0] = 0, True      # a volta começa na tônica, acentuada
    L.limpar()
    return L


def linha_sereia(rnd, n, escala):
    """FX: saltos largos com slide longo (sirene)."""
    L = Linha(n)
    notas = ESCALAS[escala]
    i = 0
    while i < n:
        L.notas[i] = rnd.choice([0, 12, 24, notas[rnd.randrange(len(notas))] + 12, -12])
        L.sld[i] = rnd.random() < 0.75
        L.acc[i] = rnd.random() < 0.3
        dur = rnd.choice([1, 2, 2, 3, 4])
        for k in range(1, dur):
            if i + k < n:
                L.notas[i + k] = "~"
        i += dur
    L.limpar()
    return L


def linha_cromatica(rnd, n):
    """FX: escada cromática / aleatória com saltos."""
    L = Linha(n)
    sobe = rnd.random() < 0.5
    passo = rnd.choice([1, 1, 2, 3, 5, 7])
    x = rnd.choice([-12, 0, 12]) if sobe else rnd.choice([12, 24])
    for i in range(n):
        if rnd.random() < 0.15 and i > 0:
            L.notas[i] = "-"
            continue
        L.notas[i] = max(-12, min(24, x))
        x += passo if sobe else -passo
        if x > 24 or x < -12:
            x = rnd.choice([-12, 0, 12])
        L.acc[i] = rnd.random() < 0.3
        L.sld[i] = rnd.random() < 0.3
    L.limpar()
    return L


# ---------------- receitas por categoria ----------------
def U(rnd, a, b):
    return rnd.uniform(a, b)


def receita(cat, rnd):
    v = {}
    esc = "menor"
    rate = 2
    if cat == "CLÁSSICO":
        esc = rnd.choice(["menor", "menor", "dorico", "blues"])
        v.update(wave=1 if rnd.random() < 0.25 else 0, cutoff=U(rnd, 300, 720), reso=U(rnd, 55, 82), envmod=U(rnd, 45, 72),
                 decay=U(rnd, 200, 620), accent=U(rnd, 50, 82), slide=U(rnd, 40, 90), drvtype=0, drive=U(rnd, 4, 30))
        if rnd.random() < 0.2:
            v.update(dlon=1, dldiv=3, dlfb=U(rnd, 25, 45), dlmix=U(rnd, 10, 20))
        n = rnd.choice([16, 16, 16, 32])
        L = linha_frase(rnd, n, esc, 0.14, 0.08, 0.26, 0.2, 0.25)
    elif cat == "ACID HOUSE":
        esc = rnd.choice(["menor", "dorico", "dorico", "blues"])
        v.update(wave=1 if rnd.random() < 0.2 else 0, cutoff=U(rnd, 300, 620), reso=U(rnd, 65, 86), envmod=U(rnd, 55, 78),
                 decay=U(rnd, 300, 720), accent=U(rnd, 55, 85), slide=U(rnd, 50, 100), drvtype=0, drive=U(rnd, 14, 40))
        if rnd.random() < 0.5:
            v["swing"] = U(rnd, 20, 50)
        if rnd.random() < 0.4:
            v.update(rvon=1, rvsize=U(rnd, 35, 60), rvmix=U(rnd, 10, 20))
        if rnd.random() < 0.25:
            v.update(dlon=1, dldiv=rnd.choice([2, 3]), dlfb=U(rnd, 25, 45), dlmix=U(rnd, 10, 20))
        L = linha_frase(rnd, 16, esc, 0.16, 0.06, 0.25, 0.22, 0.4)
    elif cat == "ACID TECHNO":
        esc = rnd.choice(["menor", "frigio", "frigio", "blues"])
        v.update(wave=1 if rnd.random() < 0.5 else 0, cutoff=U(rnd, 200, 460), reso=U(rnd, 50, 82), envmod=U(rnd, 60, 88),
                 decay=U(rnd, 140, 360), accent=U(rnd, 70, 96), slide=U(rnd, 30, 70),
                 drvtype=1 if rnd.random() < 0.6 else 0, drive=U(rnd, 38, 75), acid=U(rnd, 0, 40))
        if rnd.random() < 0.5:
            v.update(dlon=1, dldiv=3, dlfb=U(rnd, 25, 45), dlmix=U(rnd, 8, 18))
        n = rnd.choice([16, 16, 32])
        L = linha_motor(rnd, n, esc, 0.12, 0.2)
    elif cat == "TRANCE":
        esc = rnd.choice(["frigio", "frigio", "menor"])
        v.update(wave=1 if rnd.random() < 0.3 else 0, cutoff=U(rnd, 480, 1250), reso=U(rnd, 22, 55), envmod=U(rnd, 28, 55),
                 decay=U(rnd, 90, 230), accent=U(rnd, 28, 52), slide=U(rnd, 20, 50), drvtype=0, drive=U(rnd, 12, 36),
                 volume=-5)
        n = rnd.choice([16, 32, 32])
        L = linha_rolling(rnd, n, esc)
    elif cat == "BASS":
        esc = rnd.choice(["menor", "menor", "dorico"])
        v.update(wave=1 if rnd.random() < 0.5 else 0, cutoff=U(rnd, 110, 360), reso=U(rnd, 12, 52), envmod=U(rnd, 18, 55),
                 decay=U(rnd, 300, 1300), accent=U(rnd, 28, 62), slide=U(rnd, 60, 160), drvtype=0, drive=U(rnd, 4, 26))
        if rnd.random() < 0.4:
            v["tune"] = -12
        L = linha_frase(rnd, 16, esc, 0.2, 0.22, 0.18, 0.25, 0.12, faixa=(-12, 12))
    elif cat == "LEAD":
        esc = rnd.choice(["menor", "dorico", "frigio", "blues"])
        v.update(wave=1 if rnd.random() < 0.5 else 0, cutoff=U(rnd, 700, 2100), reso=U(rnd, 58, 90), envmod=U(rnd, 38, 70),
                 decay=U(rnd, 450, 1300), accent=U(rnd, 55, 85), slide=U(rnd, 80, 210), drvtype=0, drive=U(rnd, 38, 72),
                 volume=-8)
        if rnd.random() < 0.85:
            v.update(dlon=1, dldiv=rnd.choice([1, 3, 3, 5]), dlfb=U(rnd, 38, 62), dlmix=U(rnd, 18, 34))
        if rnd.random() < 0.7:
            v.update(rvon=1, rvsize=U(rnd, 45, 75), rvmix=U(rnd, 14, 30))
        L = linha_frase(rnd, 32, esc, 0.1, 0.2, 0.25, 0.3, 0.3, faixa=(0, 24), base=12, tam_motivo=8)
    elif cat == "SQUELCH":
        esc = rnd.choice(["menor", "frigio", "blues"])
        v.update(wave=1 if rnd.random() < 0.35 else 0, cutoff=U(rnd, 240, 620), reso=U(rnd, 85, 98), envmod=U(rnd, 60, 88),
                 decay=U(rnd, 190, 460), accent=U(rnd, 78, 100), slide=U(rnd, 40, 90), acid=U(rnd, 20, 80),
                 drvtype=1 if rnd.random() < 0.3 else 0, drive=U(rnd, 22, 50), volume=-8)
        n = rnd.choice([16, 16, 32])
        L = linha_frase(rnd, n, esc, 0.1, 0.05, 0.42, 0.25, 0.35)
    elif cat == "DUB":
        esc = rnd.choice(["menor", "dorico"])
        v.update(wave=1 if rnd.random() < 0.3 else 0, cutoff=U(rnd, 240, 520), reso=U(rnd, 58, 86), envmod=U(rnd, 40, 66),
                 decay=U(rnd, 380, 950), accent=U(rnd, 45, 70), slide=U(rnd, 60, 120), drvtype=0, drive=U(rnd, 8, 28),
                 dlon=1, dldiv=rnd.choice([3, 3, 4, 5]), dlfb=U(rnd, 58, 82), dlmix=U(rnd, 28, 46),
                 rvon=1, rvsize=U(rnd, 60, 92), rvmix=U(rnd, 18, 36))
        rate = 4 if rnd.random() < 0.3 else 2
        n = rnd.choice([8, 16, 16])
        L = linha_frase(rnd, n, esc, 0.42, 0.1, 0.25, 0.2, 0.2, tam_motivo=4)
    else:  # FX
        esc = rnd.choice(["menor", "frigio", "blues", "cromatico"])
        v.update(wave=rnd.choice([0, 1]), cutoff=U(rnd, 200, 1800), reso=U(rnd, 60, 98), envmod=U(rnd, 30, 95),
                 decay=U(rnd, 60, 1600), accent=U(rnd, 40, 100), slide=U(rnd, 60, 380),
                 drvtype=rnd.choice([0, 1, 2, 2]), drive=U(rnd, 20, 70), acid=U(rnd, 0, 90), volume=-8)
        if rnd.random() < 0.5:
            v["tune"] = rnd.choice([-7, -5, 5, 7])
        if rnd.random() < 0.6:
            v.update(dlon=1, dldiv=rnd.randrange(6), dlfb=U(rnd, 40, 85), dlmix=U(rnd, 18, 40))
        if rnd.random() < 0.6:
            v.update(rvon=1, rvsize=U(rnd, 40, 95), rvmix=U(rnd, 15, 40))
        rate = rnd.choice([0, 1, 2, 2, 3])
        n = rnd.choice([12, 16, 24, 32])
        estilo = rnd.random()
        L = linha_sereia(rnd, n, esc) if estilo < 0.35 else linha_cromatica(rnd, n) if estilo < 0.65 else \
            linha_frase(rnd, n, esc, 0.2, 0.15, 0.35, 0.4, 0.5, tam_motivo=rnd.choice([3, 4, 6]))
    if rate != 2:
        v["seqrate"] = rate
    # drive e ressonância altos empurram o volume: compensa um pouco
    if v.get("drive", 0) > 55 and "volume" not in v:
        v["volume"] = -8
    return v, L


def arredonda(pid, x):
    lo, hi = FAIXAS[pid]
    x = max(lo, min(hi, x))
    if pid in ("cutoff", "decay") and x >= 1000:
        return float(round(x / 10) * 10)
    return float(round(x))


def valida(nome, cat, vals, notas, acc, sld):
    assert cat in CATEGORIAS, cat
    for k, x in vals.items():
        lo, hi = FAIXAS[k]
        assert lo <= x <= hi, (nome, k, x)
    itens = notas.split()
    assert 1 <= len(itens) <= 32, (nome, len(itens))
    assert len(acc) == len(itens) == len(sld), (nome, len(itens), len(acc), len(sld))
    for t in itens:
        assert t in ("-", "~") or -12 <= int(t) <= 24, (nome, t)


def cpp(s):
    return s.replace("\\", "\\\\").replace('"', '\\"')


def main():
    raiz = Path(__file__).resolve().parent.parent
    saida = raiz / "Source" / "presets" / "fabrica.inc"
    vistos = set()
    linhas = [
        "// GERADO por tools/gerar-presets.py — não editar à mão (mude o script e rode de novo).",
        "// LIMÃO — banco FACTORY 1: 40 presets por categoria, na ordem de categoryNames.",
        "// { nome, categoria, { valores REAIS }, notas, accent, slide }",
    ]
    total = 0
    for ci, cat in enumerate(CATEGORIAS):
        presets = []
        for (c, nome, vals, notas, acc, sld) in MANUAIS:
            if c == cat:
                presets.append((nome, {k: float(x) for k, x in vals.items()}, notas, acc, sld))
        nomes = [x.strip() for x in NOMES[cat].replace("\n", " ").split(";") if x.strip()]
        for nome in nomes:
            if len(presets) >= POR_CATEGORIA:
                break
            rnd = random.Random(zlib.crc32((cat + "|" + nome).encode("utf-8")))
            vals, L = receita(cat, rnd)
            vals = {k: arredonda(k, x) for k, x in vals.items()}
            notas, acc, sld = L.texto()
            presets.append((nome, vals, notas, acc, sld))
        assert len(presets) == POR_CATEGORIA, (cat, len(presets))

        linhas.append("")
        linhas.append(f"// ---- {cat} ----")
        for nome, vals, notas, acc, sld in presets:
            assert nome.lower() not in vistos, f"nome repetido: {nome}"
            vistos.add(nome.lower())
            valida(nome, cat, vals, notas, acc, sld)
            kv = ", ".join(f'{{ "{k}", {x:g}.0f }}' if x == int(x) else f'{{ "{k}", {x:g}f }}' for k, x in vals.items())
            linhas.append(f'{{ "{cpp(nome)}", {ci},')
            linhas.append(f"  {{ {kv} }},")
            linhas.append(f'  "{notas}",')
            linhas.append(f'  "{acc}", "{sld}" }},')
            total += 1

    saida.parent.mkdir(parents=True, exist_ok=True)
    saida.write_text("\n".join(linhas) + "\n", encoding="utf-8", newline="\n")
    print(f"{saida}: {total} presets")


if __name__ == "__main__":
    main()

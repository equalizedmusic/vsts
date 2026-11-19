#!/usr/bin/env python3
"""Bancos de presets do CHUTE (.chutebank). Formato descrito em Source/Bancos.h.

    python banco.py gera  <modelos.h> <saida.chutebank> --pacote NOME [--autor X] [--versao 1.0]
                          [--banco NOVO_NOME]           (troca o nome do banco de todos os presets)
                          [--ajustes ajustes.json]      ({"NOME DO PRESET": {"clicklevel": 3, "drive": 20}, ...})
    python banco.py lista <arquivo.chutebank>           (pacote, bancos e presets, para conferir)
    python banco.py abre  <arquivo.chutebank> <saida.h> (volta para o formato Modelos.h, para editar)

<modelos.h> = um cabeçalho no formato do Modelos.h antigo (struct Modelo + arrays dN/aN/cN + tabela modelos[]),
que é o que o gerador de modelos produz a partir de um pack de kicks. A FÁBRICA (os 50) sai assim:
    python banco.py gera ../Source/Modelos.h ../bancos/fabrica/FABRICA.chutebank --pacote FABRICA
Banco novo = um .chutebank em ../bancos/ (vai no instalador do CHUTE e no pacote de presets "CHUTE-Bancos").
"""
import argparse
import json
import os
import re
import struct
import sys
import zlib

CHAVE = 0x43485554454B494B  # igual a chaveMistura em Bancos.h
M64 = (1 << 64) - 1


def mistura(dados, semente):
    s = (semente ^ CHAVE) & M64 or 0x9E3779B97F4A7C15
    out = bytearray(dados)
    for i in range(0, len(out), 8):
        s ^= s >> 12
        s = (s ^ (s << 25)) & M64
        s ^= s >> 27
        r = (s * 0x2545F4914F6CDD1D) & M64
        for k in range(min(8, len(out) - i)):
            out[i + k] ^= (r >> (8 * k)) & 0xFF
    return bytes(out)


def txt(s):
    b = s.encode("utf-8")
    if len(b) > 255:
        sys.exit(f"texto longo demais: {s[:40]}…")
    return struct.pack("<B", len(b)) + b


# ---------------------------------------------------------------- Modelos.h → lista de presets

def le_modelos_h(caminho):
    t = open(caminho, encoding="utf-8").read()
    taxa = float(re.search(r"taxaClick\s*=\s*([\d.]+)", t).group(1)) if "taxaClick" in t else 44100.0
    arrays = {}
    for m in re.finditer(r"static const (?:float|int16_t) (\w+)\[\w+\] = \{([^}]*)\};", t):
        arrays[m.group(1)] = [float(x.rstrip("f")) for x in m.group(2).split(",") if x.strip()]
    tabela = re.search(r"modelos\[\w+\] = \{(.*?)\n\};", t, re.S).group(1)
    presets = []
    for m in re.finditer(r'\{\s*"([^"]*)",\s*"([^"]*)",\s*(-?\d+),\s*([^}]*)\}', tabela):
        campos = [c.strip() for c in m.group(4).split(",")]
        nums = [float(c.rstrip("f")) for c in campos[:7]]
        d, a, c = campos[7:10]
        presets.append({
            "banco": m.group(1), "nome": m.group(2), "nota": int(m.group(3)),
            "fEnd": nums[0], "t0": nums[1], "fase0": nums[2], "sinal": nums[3], "fIni": nums[4],
            "dropMs": nums[5], "decayMs": nums[6], "taxaClick": taxa,
            "d": arrays[d], "a": arrays[a], "click": [int(x) for x in arrays[c]], "ajustes": {},
        })
    if not presets:
        sys.exit(f"nenhum preset em {caminho}")
    return presets


def grava(presets, saida, pacote, autor, versao):
    c = txt(pacote) + txt(autor) + txt(versao) + struct.pack("<H", len(presets))
    for p in presets:
        c += txt(p["banco"]) + txt(p["nome"]) + struct.pack("<h", p["nota"])
        c += struct.pack("<8f", p["fEnd"], p["t0"], p["fase0"], p["sinal"], p["fIni"], p["dropMs"], p["decayMs"], p["taxaClick"])
        if len(p["d"]) != len(p["a"]):
            sys.exit(f"{p['nome']}: curvas d e a de tamanhos diferentes")
        c += struct.pack("<H", len(p["d"])) + struct.pack(f"<{len(p['d'])}f", *p["d"]) + struct.pack(f"<{len(p['a'])}f", *p["a"])
        c += struct.pack("<H", len(p["click"])) + struct.pack(f"<{len(p['click'])}h", *p["click"])
        aj = p.get("ajustes") or {}
        c += struct.pack("<B", len(aj))
        for k, v in aj.items():
            c += txt(k) + struct.pack("<f", float(v))
    semente = int.from_bytes(os.urandom(8), "little")
    cab = b"CHTB" + struct.pack("<HHQII", 1, 0, semente, len(c), zlib.crc32(c) & 0xFFFFFFFF)
    os.makedirs(os.path.dirname(os.path.abspath(saida)), exist_ok=True)
    with open(saida, "wb") as f:
        f.write(cab + mistura(c, semente))


# ---------------------------------------------------------------- .chutebank → lista

def le_banco(caminho):
    b = open(caminho, "rb").read()
    if b[:4] != b"CHTB":
        sys.exit("não é um .chutebank")
    ver, _, semente, tam, crc = struct.unpack("<HHQII", b[4:24])
    c = mistura(b[24:], semente)
    if len(c) != tam or (zlib.crc32(c) & 0xFFFFFFFF) != crc:
        sys.exit("banco corrompido (tamanho ou crc)")
    i = 0

    def pega(fmt):
        nonlocal i
        v = struct.unpack_from(fmt, c, i)
        i += struct.calcsize(fmt)
        return v

    def t():
        (n,) = pega("<B")
        return pega(f"<{n}s")[0].decode("utf-8")

    pacote, autor, versao = t(), t(), t()
    (n,) = pega("<H")
    presets = []
    for _ in range(n):
        p = {"banco": t(), "nome": t(), "nota": pega("<h")[0]}
        for k, v in zip(["fEnd", "t0", "fase0", "sinal", "fIni", "dropMs", "decayMs", "taxaClick"], pega("<8f")):
            p[k] = v
        (nc,) = pega("<H")
        p["d"] = list(pega(f"<{nc}f"))
        p["a"] = list(pega(f"<{nc}f"))
        (nk,) = pega("<H")
        p["click"] = list(pega(f"<{nk}h"))
        (na,) = pega("<B")
        p["ajustes"] = {}
        for _ in range(na):
            k = t()
            p["ajustes"][k] = pega("<f")[0]
        presets.append(p)
    return {"pacote": pacote, "autor": autor, "versao": versao, "presets": presets}


def g(v):
    return f"{v:.5g}f"


def grava_modelos_h(presets, saida):
    nc, nk = len(presets[0]["d"]), len(presets[0]["click"])
    if any(len(p["d"]) != nc or len(p["click"]) != nk for p in presets):
        sys.exit("abre: os presets têm tamanhos diferentes (o Modelos.h pede tamanhos iguais)")
    L = ["#pragma once", "// Aberto de um .chutebank por tools/banco.py (formato do Modelos.h antigo).", "#include <cstdint>", "",
         "namespace chute", "{", f"constexpr int numModelos = {len(presets)};", f"constexpr int nClick = {nk};",
         f"constexpr int nCurva = {nc};", f"constexpr double taxaClick = {presets[0]['taxaClick']:.1f};", "",
         "struct Modelo", "{",
         "    const char* banco; const char* nome; int nota; float fEnd, t0, fase0, sinal, fIni, dropMs, decayMs;",
         "    const float* d; const float* a; const int16_t* click;", "};", ""]
    for i, p in enumerate(presets):
        L.append(f"static const float d{i}[nCurva] = {{{','.join(g(x) for x in p['d'])}}};")
        L.append(f"static const float a{i}[nCurva] = {{{','.join(g(x) for x in p['a'])}}};")
        L.append(f"static const int16_t c{i}[nClick] = {{{','.join(str(x) for x in p['click'])}}};")
    L.append("")
    L.append("static const Modelo modelos[numModelos] = {")
    for i, p in enumerate(presets):
        nums = ", ".join(g(p[k]) for k in ["fEnd", "t0", "fase0", "sinal", "fIni", "dropMs", "decayMs"])
        L.append(f'    {{ "{p["banco"]}", "{p["nome"]}", {p["nota"]}, {nums}, d{i}, a{i}, c{i} }},')
    L += ["};", "} // namespace chute", ""]
    with open(saida, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(L))


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    a = sub.add_parser("gera")
    a.add_argument("modelos")
    a.add_argument("saida")
    a.add_argument("--pacote", required=True)
    a.add_argument("--autor", default="InsideStudioSP")
    a.add_argument("--versao", default="1.0")
    a.add_argument("--banco")
    a.add_argument("--ajustes")
    b = sub.add_parser("lista")
    b.add_argument("arquivo")
    c = sub.add_parser("abre")
    c.add_argument("arquivo")
    c.add_argument("saida")
    x = ap.parse_args()
    if x.cmd == "gera":
        presets = le_modelos_h(x.modelos)
        if x.banco:
            for p in presets:
                p["banco"] = x.banco
        if x.ajustes:
            aj = json.load(open(x.ajustes, encoding="utf-8"))
            for p in presets:
                p["ajustes"] = aj.get(p["nome"], {})
        grava(presets, x.saida, x.pacote, x.autor, x.versao)
        print(f"{x.saida}: {len(presets)} presets, {os.path.getsize(x.saida)} bytes")
    elif x.cmd == "lista":
        r = le_banco(x.arquivo)
        print(f"pacote {r['pacote']} · {r['autor']} · versão {r['versao']} · {len(r['presets'])} presets")
        for p in r["presets"]:
            extra = f"  {p['ajustes']}" if p["ajustes"] else ""
            print(f"  {p['banco']:<8} {p['nome']:<20} nota {p['nota']}{extra}")
    else:
        grava_modelos_h(le_banco(x.arquivo)["presets"], x.saida)
        print("gravado:", x.saida)


if __name__ == "__main__":
    main()

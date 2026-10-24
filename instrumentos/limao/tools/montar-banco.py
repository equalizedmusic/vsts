#!/usr/bin/env python3
"""Monta um banco avulso de presets do LIMÃO (.lmobank) a partir dos presets salvos no plugin.

Uso:
    python montar-banco.py <pasta> "<nome do banco>" [saida.lmobank] [--autor "InsideStudioSP"]

<pasta> tem uma subpasta por categoria (CLÁSSICO, ACID HOUSE, ACID TECHNO, TRANCE, BASS, LEAD, SQUELCH, DUB, FX)
com os .xml salvos pelo "Salvar como novo preset…" do LIMÃO. Ficam em:
    Windows: %APPDATA%\\InsideStudioSP\\LIMAO\\Presets
    Mac:     ~/Library/Audio/Presets/InsideStudioSP/LIMAO
O nome do preset é o nome do arquivo. .xml solto na raiz da pasta vai para OUTROS.
O padrão do sequenciador vai junto (só os passos dentro do comprimento salvo).
Quem baixa o banco abre o LIMÃO, menu de presets > "Importar banco de presets…" e escolhe o arquivo.
"""
import argparse
import sys
import unicodedata
import xml.etree.ElementTree as ET
from pathlib import Path

CATEGORIAS = ["CLÁSSICO", "ACID HOUSE", "ACID TECHNO", "TRANCE", "BASS", "LEAD", "SQUELCH", "DUB", "FX"]
PERFORMANCE = {"seqon"}   # o preset não mexe nele


def sem_acento(s):
    return "".join(c for c in unicodedata.normalize("NFD", s) if unicodedata.category(c) != "Mn").upper()


def padrao_em_texto(packed, passos):
    """'p,m,a,s;' por passo (estado do plugin) → notas / acc / sld no texto dos presets."""
    notas, acc, sld = [], "", ""
    itens = [x for x in packed.split(";") if x.strip()]
    for i in range(passos):
        f = itens[i].split(",") if i < len(itens) else ["0", "0", "0", "0"]
        p, m, a, s = (int(x) for x in f[:4])
        notas.append("-" if m == 0 else "~" if m == 2 else str(max(-12, min(24, p))))
        acc += "x" if a else "."
        sld += "x" if s else "."
    return " ".join(notas), acc, sld


def ler_preset(arquivo: Path):
    raiz = ET.parse(arquivo).getroot()
    if raiz.tag != "LIMAO":
        raise ValueError(f"{arquivo.name}: não é um preset do LIMÃO")
    valores, passos = [], 16
    for p in raiz.iter("PARAM"):
        pid, val = p.get("id"), p.get("value")
        if pid and val is not None and pid not in PERFORMANCE:
            valores.append((pid, val))
            if pid == "seqlen":
                passos = max(1, min(32, round(float(val))))
    padrao = padrao_em_texto(raiz.get("pattern", ""), passos) if raiz.get("pattern") else None
    return valores, padrao


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("pasta")
    ap.add_argument("nome")
    ap.add_argument("saida", nargs="?")
    ap.add_argument("--autor", default="InsideStudioSP")
    a = ap.parse_args()

    pasta = Path(a.pasta)
    if not pasta.is_dir():
        sys.exit(f"pasta não encontrada: {pasta}")
    if a.nome.strip().upper() in ("FACTORY 1", "USER"):
        sys.exit("esse nome é reservado do LIMÃO")

    banco = ET.Element("LIMAOBANK", {"format": "1", "name": a.nome.strip(), "author": a.autor})
    total = 0
    # subpasta com o nome da categoria (com ou sem acento, maiúscula ou minúscula); o resto avisa e pula
    por_nome = {sem_acento(c): c for c in CATEGORIAS}
    subpastas = {}
    for d in pasta.iterdir():
        if d.is_dir():
            if sem_acento(d.name) in por_nome:
                subpastas[por_nome[sem_acento(d.name)]] = d
            else:
                print(f"aviso: pasta '{d.name}' não é uma categoria, ficou de fora", file=sys.stderr)
    grupos = [(c, subpastas[c]) for c in CATEGORIAS if c in subpastas] + [("", pasta)]
    for categoria, dir_ in grupos:
        for arq in sorted(dir_.glob("*.xml"), key=lambda f: f.stem.lower()):
            valores, padrao = ler_preset(arq)
            el = ET.SubElement(banco, "PRESET", {"name": arq.stem, "category": categoria})
            for pid, val in valores:
                ET.SubElement(el, "PARAM", {"id": pid, "value": val})
            if padrao:
                ET.SubElement(el, "PATTERN", {"notes": padrao[0], "acc": padrao[1], "sld": padrao[2]})
            total += 1
    if total == 0:
        sys.exit("nenhum preset .xml encontrado")

    saida = Path(a.saida) if a.saida else Path(a.nome.strip() + ".lmobank")
    ET.indent(banco)
    ET.ElementTree(banco).write(saida, encoding="UTF-8", xml_declaration=True)
    print(f"{saida}: {total} presets")


if __name__ == "__main__":
    main()

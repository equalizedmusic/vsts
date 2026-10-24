#!/usr/bin/env python3
"""Monta um banco avulso de presets do VACINA (.vcnbank) a partir dos presets salvos no plugin.

Uso:
    python montar-banco.py <pasta> "<nome do banco>" [saida.vcnbank] [--autor "InsideStudioSP"]

<pasta> tem uma subpasta por categoria (BASS, LEAD, PAD, PLUCK, ARP, KEYS, SUPERSAW, ATMOSFERA, FX)
com os .xml salvos pelo "Salvar como novo preset…" do VACINA. Ficam em:
    Windows: %APPDATA%\\InsideStudioSP\\VACINA\\Presets
    Mac:     ~/Library/Audio/Presets/InsideStudioSP/VACINA
O nome do preset é o nome do arquivo. .xml solto na raiz da pasta vai para OUTROS.
Quem baixa o banco abre o VACINA, menu de presets > "Importar banco de presets…" e escolhe o arquivo.
"""
import argparse
import sys
import unicodedata
import xml.etree.ElementTree as ET
from pathlib import Path

CATEGORIAS = ["BASS", "LEAD", "PAD", "PLUCK", "ARP", "KEYS", "SUPERSAW", "ATMOSFERA", "FX"]


def sem_acento(s):
    return "".join(c for c in unicodedata.normalize("NFD", s) if unicodedata.category(c) != "Mn").upper()


def ler_preset(arquivo: Path):
    raiz = ET.parse(arquivo).getroot()
    if raiz.tag != "VACINA":
        raise ValueError(f"{arquivo.name}: não é um preset do VACINA")
    return [(p.get("id"), p.get("value")) for p in raiz.iter("PARAM") if p.get("id") and p.get("value") is not None]


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
        sys.exit("esse nome é reservado do VACINA")

    banco = ET.Element("VACINABANK", {"format": "1", "name": a.nome.strip(), "author": a.autor})
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
            el = ET.SubElement(banco, "PRESET", {"name": arq.stem, "category": categoria})
            for pid, val in ler_preset(arq):
                ET.SubElement(el, "PARAM", {"id": pid, "value": val})
            total += 1
    if total == 0:
        sys.exit("nenhum preset .xml encontrado")

    saida = Path(a.saida) if a.saida else Path(a.nome.strip() + ".vcnbank")
    ET.indent(banco)
    ET.ElementTree(banco).write(saida, encoding="UTF-8", xml_declaration=True)
    print(f"{saida}: {total} presets")


if __name__ == "__main__":
    main()

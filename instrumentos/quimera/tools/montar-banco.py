#!/usr/bin/env python3
"""Monta um banco avulso de presets do QUIMERA (.qmrbank) a partir dos presets salvos no plugin.

Uso:
    python montar-banco.py <pasta> "<nome do banco>" [saida.qmrbank] [--autor "InsideStudioSP"]

<pasta> tem uma subpasta por categoria (BASS, LEAD, PLUCK, PAD, ATMOS, PERC, DRUMS, KEYS, FX, ARP) com os
.xml salvos pelo "Salvar como novo preset…" do QUIMERA. Ficam em:
    Windows: %APPDATA%\\InsideStudioSP\\QUIMERA\\Presets
    Mac:     ~/Library/Audio/Presets/InsideStudioSP/QUIMERA
O nome do preset é o nome do arquivo. .xml solto na raiz da pasta vai para OUTROS.
Quem baixa o banco abre o QUIMERA, menu de presets > "Importar banco de presets…" e escolhe o arquivo.
"""
import argparse
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

CATEGORIAS = ["BASS", "LEAD", "PLUCK", "PAD", "ATMOS", "PERC", "DRUMS", "KEYS", "FX", "ARP"]
PERFORMANCE = {"mpe", "mpebend", "hold"}   # o preset não mexe neles


def ler_preset(arquivo: Path):
    raiz = ET.parse(arquivo).getroot()
    if raiz.tag != "QUIMERA":
        raise ValueError(f"{arquivo.name}: não é um preset do QUIMERA")
    valores = []
    for p in raiz.iter("PARAM"):
        pid, val = p.get("id"), p.get("value")
        if pid and val is not None and pid not in PERFORMANCE:
            valores.append((pid, val))
    return valores


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
        sys.exit("esse nome é reservado do QUIMERA")

    banco = ET.Element("QUIMERABANK", {"format": "1", "name": a.nome.strip(), "author": a.autor})
    total = 0
    # subpasta com o nome da categoria (maiúscula ou minúscula, na ordem do menu); o resto avisa e pula
    subpastas = {d.name.upper(): d for d in pasta.iterdir() if d.is_dir()}
    for nome in subpastas:
        if nome not in CATEGORIAS:
            print(f"aviso: pasta '{subpastas[nome].name}' não é uma categoria, ficou de fora", file=sys.stderr)
    grupos = [(c, subpastas[c]) for c in CATEGORIAS if c in subpastas] + [("", pasta)]
    for categoria, dir_ in grupos:
        for arq in sorted(dir_.glob("*.xml"), key=lambda f: f.stem.lower()):
            valores = ler_preset(arq)
            el = ET.SubElement(banco, "PRESET", {"name": arq.stem, "category": categoria})
            for pid, val in valores:
                ET.SubElement(el, "PARAM", {"id": pid, "value": val})
            total += 1
    if total == 0:
        sys.exit("nenhum preset .xml encontrado")

    saida = Path(a.saida) if a.saida else Path(a.nome.strip() + ".qmrbank")
    ET.indent(banco)
    ET.ElementTree(banco).write(saida, encoding="UTF-8", xml_declaration=True)
    print(f"{saida}: {total} presets")


if __name__ == "__main__":
    main()

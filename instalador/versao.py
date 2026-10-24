#!/usr/bin/env python3
"""Versão e notas de cada pacote, lidas de novidades/<arquivo>.md (o primeiro título "## x.y.z" é a versão atual).

    python3 instalador/versao.py Limao             -> 0.3.0
    python3 instalador/versao.py Limao --tag       -> limao-v0.3.0
    python3 instalador/versao.py Limao --titulo    -> LIMÃO 0.3.0
    python3 instalador/versao.py Limao --notas     -> o texto da seção da versão atual (vira a nota do release)

<pacote> é o mesmo nome do --pacote do montar.py (Criativo, Mix, Instrumentos, Limao, Vacina).
O CMake lê a mesma versão (instrumentos/CMakeLists.txt e CMakeLists.txt da raiz), então plugin, instalador e
release andam juntos: para lançar, basta escrever a seção nova no topo do arquivo de novidades.
"""
import argparse
import os
import re
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# pacote do instalador -> (arquivo em novidades/, prefixo da tag, nome no título do release)
PACOTES = {
    "Criativo":     ("Criativo.md", "criativo", "Linha Criativa"),
    "Mix":          ("Mix.md",      "mix",      "Linha de Mix"),
    "Instrumentos": ("Quimera.md",  "quimera",  "QUIMERA"),
    "Limao":        ("Limao.md",    "limao",    "LIMÃO"),
    "Vacina":       ("Vacina.md",   "vacina",   "VACINA"),
}

TITULO = re.compile(r"^## (\d+\.\d+\.\d+)\b")


def secoes(pacote):
    arquivo = os.path.join(RAIZ, "novidades", PACOTES[pacote][0])
    with open(arquivo, encoding="utf-8") as f:
        linhas = f.read().splitlines()
    atual, corpo, saida = None, [], []
    for ln in linhas:
        m = TITULO.match(ln)
        if m:
            if atual:
                saida.append((atual, corpo))
            atual, corpo = m.group(1), []
        elif atual:
            corpo.append(ln)
    if atual:
        saida.append((atual, corpo))
    if not saida:
        sys.exit(f"{arquivo}: nenhum título '## x.y.z'")
    return saida


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pacote", choices=sorted(PACOTES))
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--tag", action="store_true")
    g.add_argument("--titulo", action="store_true")
    g.add_argument("--notas", action="store_true")
    a = ap.parse_args()
    versao, corpo = secoes(a.pacote)[0]
    _, prefixo, nome = PACOTES[a.pacote]
    if a.tag:
        print(f"{prefixo}-v{versao}")
    elif a.titulo:
        print(f"{nome} {versao}")
    elif a.notas:
        print("\n".join(corpo).strip())
    else:
        print(versao)


if __name__ == "__main__":
    main()

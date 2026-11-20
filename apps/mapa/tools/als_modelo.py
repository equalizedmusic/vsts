#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
als_modelo.py — gera apps/mapa/Recursos/modelo.als a partir de um set REAL do Live.

Origem usada: DefaultLiveSet.als do projeto logic2ableton (licença MIT, Copyright (c) 2025 evilander)
  https://github.com/Evilander/logic2ableton/blob/master/logic2ableton/data/DefaultLiveSet.als
  Creator="Ableton Live 12.1d1", MinorVersion="12.0_12117" (é o set padrão do Live 12, sem dispositivos nas faixas).

O que faz: mantém só a 1ª MidiTrack e a 1ª AudioTrack (moldes que o exportador clona) + as ReturnTracks
(o Live as espera: cada faixa tem um TrackSendHolder por return), tira os locators e grava em gzip.

Uso:  python -I als_modelo.py DefaultLiveSet.als ../Recursos/modelo.als
"""
import gzip
import sys
import xml.etree.ElementTree as ET


def main():
    origem, destino = sys.argv[1], sys.argv[2]
    dados = gzip.decompress(open(origem, "rb").read())
    raiz = ET.fromstring(dados)
    ls = raiz.find("LiveSet")
    faixas = ls.find("Tracks")
    tem_midi = tem_audio = False
    for fx in list(faixas):
        if fx.tag == "MidiTrack" and not tem_midi:
            tem_midi = True
        elif fx.tag == "AudioTrack" and not tem_audio:
            tem_audio = True
        elif fx.tag == "ReturnTrack":
            pass
        else:
            faixas.remove(fx)
    assert tem_midi and tem_audio, "o set de origem precisa de 1 MidiTrack e 1 AudioTrack"
    locs = ls.find("Locators/Locators")
    if locs is not None:
        for x in list(locs):
            locs.remove(x)
    xml = b'<?xml version="1.0" encoding="UTF-8"?>\n' + ET.tostring(raiz, encoding="unicode").encode("utf-8")
    with open(destino, "wb") as f:
        f.write(gzip.compress(xml, 9))
    print("gravado", destino, len(xml), "bytes de XML")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
als_check.py — confere um .als (Ableton Live Set, XML compactado com gzip).

Uso:  python -I als_check.py arquivo.als [--xml saida.xml]

Lista versão, tempo, compasso, locators, faixas e clipes do arranjo, e ACUSA:
  - Ids de alvos de automação/modulação repetidos (AutomationTarget, ModulationTarget,
    *Target, Pointee, ControllerTargets.N) — o Live chama o set de "corrupt" quando repete;
  - NextPointeeId menor ou igual ao maior Id de alvo;
  - Ids de faixa repetidos;
  - PointeeId (envelopes) apontando para alvo inexistente;
  - clipe com CurrentEnd <= CurrentStart, nota fora do clipe, NoteId repetido no clipe,
    NoteIdGenerator/NextId <= maior NoteId;
  - arquivo de áudio referenciado que não existe (Path absoluto e RelativePath).
Sai com código 1 se houver erro.
"""
import gzip
import os
import sys
import xml.etree.ElementTree as ET

PALETA_LIVE = [
    0xFF94A6, 0xFFA529, 0xCC9927, 0xF7F47C, 0xBFFB00, 0x1AFF2F, 0x25FFA8, 0x5CFFE8, 0x8BC5FF, 0x5480E4,
    0x92A7FF, 0xD86CE4, 0xE553A0, 0xFFFFFF, 0xFF3636, 0xF66C03, 0x99724B, 0xFFF034, 0x87FF67, 0x3DC300,
    0x00BFAF, 0x19E9FF, 0x10A4EE, 0x007DC0, 0x886CE4, 0xB677C6, 0xFF39D4, 0xD0D0D0, 0xE2675A, 0xFFA374,
    0xD3AD71, 0xEDFFAE, 0xD2E498, 0xBAD074, 0x9BC48D, 0xD4FDE1, 0xCDF1F8, 0xB9C1E3, 0xCDBBE4, 0xAE98E5,
    0xE5DCE1, 0xA9A9A9, 0xC6928B, 0xB78256, 0x99836A, 0xBFBA69, 0xA6BE00, 0x7DB04D, 0x88C2BA, 0x9BB3C4,
    0x85A5C2, 0x8393CC, 0xA595B5, 0xBF9FBE, 0xBC7196, 0x7B7B7B, 0xAF3333, 0xA95131, 0x724F41, 0xDBC300,
    0x85961F, 0x539F31, 0x0A9C8E, 0x236384, 0x1A2F96, 0x2F52A2, 0x624BAD, 0xA34BAD, 0xCC2E6E, 0x3C3C3C,
]

FAIXAS = ("AudioTrack", "MidiTrack", "GroupTrack", "ReturnTrack")


def eh_alvo(tag):
    """Elemento cujo Id vive no espaço global de 'pointees' (NextPointeeId)."""
    return tag.endswith("Target") or tag == "Pointee" or tag.startswith("ControllerTargets.")


def val(el, caminho, padrao=None):
    x = el.find(caminho)
    return x.get("Value") if x is not None else padrao


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except AttributeError:
        pass
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    caminho = sys.argv[1]
    dados = open(caminho, "rb").read()
    try:
        dados = gzip.decompress(dados)
    except OSError:
        print("AVISO: arquivo não está em gzip (lendo como XML puro)")
    if "--xml" in sys.argv:
        open(sys.argv[sys.argv.index("--xml") + 1], "wb").write(dados)

    raiz = ET.fromstring(dados)
    erros, avisos = [], []
    print("Arquivo :", caminho)
    print("Versão  :", raiz.get("Creator"), "| MinorVersion", raiz.get("MinorVersion"))
    ls = raiz.find("LiveSet")
    if ls is None:
        print("ERRO: sem <LiveSet>")
        return 1

    # ---- tempo e compasso (Live 12: MainTrack; Live <= 11: MasterTrack)
    principal = ls.find("MainTrack")
    if principal is None:
        principal = ls.find("MasterTrack")
    mixer = principal.find("DeviceChain/Mixer")
    tempo_alvo = mixer.find("Tempo/AutomationTarget").get("Id")
    ts_alvo_el = mixer.find("TimeSignature/AutomationTarget")
    ts_alvo = ts_alvo_el.get("Id") if ts_alvo_el is not None else None
    tempo_manual = val(mixer, "Tempo/Manual")
    ts_manual = val(mixer, "TimeSignature/Manual")
    tempo_auto, ts_auto = None, None
    for env in principal.findall("AutomationEnvelopes/Envelopes/AutomationEnvelope"):
        pid = val(env, "EnvelopeTarget/PointeeId")
        evs = env.find("Automation/Events")
        if evs is None:
            continue
        for ev in evs:
            if ev.get("Time") == "-63072000":
                if pid == tempo_alvo:
                    tempo_auto = ev.get("Value")
                elif pid == ts_alvo:
                    ts_auto = ev.get("Value")
    print("Tempo   : Manual=%s | evento inicial da automação=%s" % (tempo_manual, tempo_auto))
    if tempo_auto is not None and float(tempo_auto) != float(tempo_manual):
        erros.append("tempo da automação (%s) difere do Manual (%s)" % (tempo_auto, tempo_manual))
    if ts_manual is not None:
        v = int(ts_manual)
        num, den = v % 99 + 1, 1 << (v // 99)
        print("Compasso: %d/%d (Manual=%s, automação=%s)" % (num, den, ts_manual, ts_auto))
        if ts_auto is not None and ts_auto != ts_manual:
            erros.append("compasso da automação (%s) difere do Manual (%s)" % (ts_auto, ts_manual))

    # ---- locators
    locs = ls.findall("Locators/Locators/Locator")
    print("Locators: %d" % len(locs))
    for lo in locs:
        print("   t=%-8s %s" % (val(lo, "Time"), val(lo, "Name")))

    # ---- Ids de alvo (globais) e PointeeId
    contagem = {}
    for el in raiz.iter():
        if eh_alvo(el.tag) and "Id" in el.attrib:
            contagem.setdefault(el.get("Id"), []).append(el.tag)
    repetidos = {k: v for k, v in contagem.items() if len(v) > 1}
    maior = max((int(k) for k in contagem), default=0)
    proximo = int(val(ls, "NextPointeeId", "0"))
    print("Alvos   : %d Ids, maior=%d, NextPointeeId=%d" % (len(contagem), maior, proximo))
    if repetidos:
        erros.append("%d Ids de alvo REPETIDOS, ex.: %s" % (len(repetidos), list(repetidos.items())[:5]))
    if proximo <= maior:
        erros.append("NextPointeeId (%d) <= maior Id de alvo (%d)" % (proximo, maior))
    for pid in raiz.iter("PointeeId"):
        if pid.get("Value") not in contagem:
            erros.append("PointeeId %s aponta para alvo inexistente" % pid.get("Value"))

    # ---- faixas e clipes
    pasta_als = os.path.dirname(os.path.abspath(caminho))
    ids_faixa = {}
    for fx in ls.find("Tracks"):
        if fx.tag not in FAIXAS:
            continue
        tid = fx.get("Id")
        ids_faixa.setdefault(tid, 0)
        ids_faixa[tid] += 1
        cor = int(val(fx, "Color", "0"))
        rgb = "#%06X" % PALETA_LIVE[cor] if 0 <= cor < len(PALETA_LIVE) else "?"
        print("Faixa %-11s Id=%-4s cor=%-2d %s  %s" % (fx.tag, tid, cor, rgb, val(fx, "Name/EffectiveName")))
        if fx.tag == "MidiTrack":
            evs = fx.find("DeviceChain/MainSequencer/ClipTimeable/ArrangerAutomation/Events")
        else:
            evs = fx.find("DeviceChain/MainSequencer/Sample/ArrangerAutomation/Events")
        if evs is None:
            continue
        for c in evs:
            ini, fim = float(val(c, "CurrentStart")), float(val(c, "CurrentEnd"))
            info = "   %s Id=%s Time=%s [%g → %g] '%s' warp=%s loop=%s..%s" % (
                c.tag, c.get("Id"), c.get("Time"), ini, fim, val(c, "Name"), val(c, "IsWarped"),
                val(c, "Loop/LoopStart"), val(c, "Loop/LoopEnd"))
            if fim <= ini:
                erros.append("clipe '%s' com CurrentEnd <= CurrentStart" % val(c, "Name"))
            if float(c.get("Time")) != ini:
                avisos.append("clipe '%s': Time (%s) != CurrentStart (%g)" % (val(c, "Name"), c.get("Time"), ini))
            if c.tag == "MidiClip":
                notas = c.findall("Notes/KeyTracks/KeyTrack/Notes/MidiNoteEvent")
                nids = [int(n.get("NoteId")) for n in notas]
                info += " notas=%d" % len(notas)
                if len(set(nids)) != len(nids):
                    erros.append("clipe '%s': NoteId repetido" % val(c, "Name"))
                prox = int(val(c, "Notes/NoteIdGenerator/NextId", "0"))
                if nids and prox <= max(nids):
                    erros.append("clipe '%s': NoteIdGenerator/NextId <= maior NoteId" % val(c, "Name"))
                tam = fim - ini
                for n in notas:
                    t = float(n.get("Time"))
                    if t < 0 or t >= tam + 1e-9:
                        erros.append("clipe '%s': nota em %g fora do clipe (0..%g)" % (val(c, "Name"), t, tam))
                        break
                chaves = [val(k, "MidiKey") for k in c.findall("Notes/KeyTracks/KeyTrack")]
                if len(set(chaves)) != len(chaves):
                    erros.append("clipe '%s': KeyTrack com MidiKey repetido" % val(c, "Name"))
            else:
                p = val(c, "SampleRef/FileRef/Path", "")
                rel = val(c, "SampleRef/FileRef/RelativePath", "")
                info += "\n      Path=%s\n      RelativePath=%s (tipo %s)" % (
                    p, rel, val(c, "SampleRef/FileRef/RelativePathType"))
                if not os.path.exists(p):
                    avisos.append("Path absoluto não existe nesta máquina: %s" % p)
                if rel and not os.path.exists(os.path.join(pasta_als, rel)):
                    erros.append("RelativePath não existe ao lado do .als: %s" % rel)
            print(info)
    for tid, n in ids_faixa.items():
        if n > 1:
            erros.append("Id de faixa %s repetido (%d vezes)" % (tid, n))

    print()
    for a in avisos:
        print("AVISO:", a)
    for e in erros:
        print("ERRO :", e)
    print("RESULTADO:", "OK" if not erros else "%d ERRO(S)" % len(erros))
    return 1 if erros else 0


if __name__ == "__main__":
    sys.exit(main())

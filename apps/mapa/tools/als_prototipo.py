#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
als_prototipo.py — protótipo em Python do MESMO algoritmo de exportarAbleton() (Source/Exportar.cpp).
Serve para validar a lógica (com als_check.py) sem compilar o app.

Uso:  python -I als_prototipo.py ../Recursos/modelo.als PASTA_SAIDA [--sem-notas]

Gera um projeto de exemplo (128 BPM, 4/4, referência de 60 s com corte de 0.05 s, KICK/BAIXO/HATS),
grava PASTA_SAIDA/Audio/referencia.wav (silêncio) e PASTA_SAIDA/<nome>.als.

Algoritmo (espelhado no C++):
  1. descompacta o modelo; pega a 1ª AudioTrack e a 1ª MidiTrack como moldes; guarda as ReturnTracks;
     esvazia <Tracks>.
  2. próximo Id de alvo = max(NextPointeeId, maior Id de alvo + 1).
  3. para cada faixa do Projeto: clona o molde, RENUMERA os Ids de alvo (tag termina em "Target",
     tag == "Pointee", tag começa com "ControllerTargets.") com um mapa antigo->novo e corrige os
     PointeeId que apontavam para eles; Id da faixa = único (espaço próprio das faixas);
     nome (EffectiveName + UserName), cor (índice mais próximo na paleta do Live), clipes no arranjo.
  4. devolve as ReturnTracks depois das faixas; tempo (Manual + FloatEvent inicial da automação),
     compasso (Manual + EnumEvent), Locators (um por parte), NextPointeeId.
"""
import copy
import gzip
import os
import struct
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
SENTINELA = "-63072000"


def num(v):
    """Mesmo formato do C++ (fmtNum): inteiro sem casas, senão até 6 casas sem zeros à direita."""
    if abs(v - round(v)) < 1e-9:
        return str(int(round(v)))
    s = "%.6f" % v
    return s.rstrip("0").rstrip(".")


def cor_live(rgb):
    r, g, b = (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255
    melhor, dmin = 0, 1e18
    for i, c in enumerate(PALETA_LIVE):
        cr, cg, cb = (c >> 16) & 255, (c >> 8) & 255, c & 255
        # distância "redmean" (aproximação perceptual barata)
        rm = (r + cr) / 2.0
        d = (2 + rm / 256) * (r - cr) ** 2 + 4 * (g - cg) ** 2 + (2 + (255 - rm) / 256) * (b - cb) ** 2
        if d < dmin:
            melhor, dmin = i, d
    return melhor


def eh_alvo(tag):
    return tag.endswith("Target") or tag == "Pointee" or tag.startswith("ControllerTargets.")


def sub(pai, tag, valor=None, **attrs):
    e = ET.SubElement(pai, tag)
    if valor is not None:
        e.set("Value", str(valor))
    for k, v in attrs.items():
        e.set(k, str(v))
    return e


def time_signature(pai, num_, den):
    ts = sub(sub(pai, "TimeSignature"), "TimeSignatures")
    r = sub(ts, "RemoteableTimeSignature", Id=0)
    sub(r, "Numerator", num_)
    sub(r, "Denominator", den)
    sub(r, "Time", 0)


def clipe_audio(p, idx, nome, cor, caminho_abs, rel):
    ini, fim = p["audioInicioTempos"], p["audioFimTempos"]
    c = ET.Element("AudioClip", Id=str(idx), Time=num(ini))
    sub(c, "LomId", 0)
    sub(c, "LomIdView", 0)
    sub(c, "CurrentStart", num(ini))
    sub(c, "CurrentEnd", num(fim))
    lp = sub(c, "Loop")
    # clipe SEM warp: LoopStart/LoopEnd em SEGUNDOS do arquivo (verificado no Live 12.4.3 pelo logic2ableton)
    sub(lp, "LoopStart", num(p["audioCorteSeg"]))
    sub(lp, "LoopEnd", num(p["duracaoSeg"]))
    sub(lp, "StartRelative", 0)
    sub(lp, "LoopOn", "false")
    sub(c, "Name", nome)
    sub(c, "Annotation", "")
    sub(c, "Color", cor)
    sub(c, "LaunchMode", 0)
    sub(c, "LaunchQuantisation", 0)
    time_signature(c, p["numerador"], p["denominador"])
    sub(sub(c, "Envelopes"), "Envelopes")
    sub(c, "Legato", "false")
    sub(c, "Ram", "false")
    sub(sub(c, "GrooveSettings"), "GrooveId", -1)
    sub(c, "Disabled", "false")
    sub(c, "VelocityAmount", 0)
    sub(c, "FreezeStart", 0)
    sub(c, "FreezeEnd", 0)
    sub(c, "IsWarped", "false")
    sub(c, "TakeId", 1)
    sr = sub(c, "SampleRef")
    fr = sub(sr, "FileRef")
    sub(fr, "RelativePathType", 1)
    sub(fr, "RelativePath", rel)
    sub(fr, "Path", caminho_abs)
    sub(fr, "Type", 1)
    sub(fr, "LivePackName", "")
    sub(fr, "LivePackId", "")
    sub(fr, "OriginalFileSize", p["tamanhoArquivo"])
    sub(fr, "OriginalCrc", 0)
    sub(sr, "LastModDate", 0)
    sub(sr, "SourceContext")
    sub(sr, "SampleUsageHint", 0)
    sub(sr, "DefaultDuration", int(round(p["duracaoSeg"] * p["sampleRate"])))
    sub(sr, "DefaultSampleRate", int(round(p["sampleRate"])))
    sub(c, "WarpMode", 0)
    fades = sub(c, "Fades")
    sub(fades, "FadeInLength", 0)
    sub(fades, "FadeOutLength", 0)
    sub(fades, "IsDefaultFadeIn", "true")
    sub(fades, "IsDefaultFadeOut", "true")
    wm = sub(c, "WarpMarkers")
    sub(wm, "WarpMarker", Id=0, SecTime=0, BeatTime=0)
    sub(wm, "WarpMarker", Id=1, SecTime=num(p["duracaoSeg"]), BeatTime=num(p["duracaoSeg"] * p["bpm"] / 60.0))
    return c


def clipe_midi(p, idx, nome, cor, clipe, com_notas):
    ini, fim = clipe["inicio"], clipe["fim"]
    tam = fim - ini
    c = ET.Element("MidiClip", Id=str(idx), Time=num(ini))
    sub(c, "LomId", 0)
    sub(c, "LomIdView", 0)
    sub(c, "CurrentStart", num(ini))
    sub(c, "CurrentEnd", num(fim))
    lp = sub(c, "Loop")
    sub(lp, "LoopStart", 0)
    sub(lp, "LoopEnd", num(tam))
    sub(lp, "StartRelative", 0)
    sub(lp, "LoopOn", "false")
    sub(lp, "OutMarker", num(tam))
    sub(lp, "HiddenLoopStart", 0)
    sub(lp, "HiddenLoopEnd", num(tam))
    sub(c, "Name", nome)
    sub(c, "Annotation", "")
    sub(c, "Color", cor)
    sub(c, "LaunchMode", 0)
    sub(c, "LaunchQuantisation", 0)
    time_signature(c, p["numerador"], p["denominador"])
    sub(sub(c, "Envelopes"), "Envelopes")
    st = sub(c, "ScrollerTimePreserver")
    sub(st, "LeftTime", 0)
    sub(st, "RightTime", num(tam))
    ts = sub(c, "TimeSelection")
    sub(ts, "AnchorTime", 0)
    sub(ts, "OtherTime", 0)
    sub(c, "Legato", "false")
    sub(c, "Ram", "false")
    sub(sub(c, "GrooveSettings"), "GrooveId", -1)
    sub(c, "Disabled", "false")
    sub(c, "VelocityAmount", 0)
    sub(c, "FreezeStart", 0)
    sub(c, "FreezeEnd", 0)
    sub(c, "IsWarped", "true")
    sub(c, "TakeId", 1)
    notas_el = sub(c, "Notes")
    kts = sub(notas_el, "KeyTracks")
    proximo_note_id = 1
    if com_notas:
        por_tom = {}
        for n in clipe["notas"]:
            t = n["inicio"] - ini
            if t < 0 or t >= tam:
                continue  # nota fora do clipe: descartada
            d = min(max(n["dur"], 1.0 / 64), tam - t)
            por_tom.setdefault(max(0, min(127, n["tom"])), []).append((t, d, max(1, min(127, n["vel"]))))
        for k, tom in enumerate(sorted(por_tom)):
            kt = sub(kts, "KeyTrack", Id=k)
            ne = sub(kt, "Notes")
            for (t, d, v) in sorted(por_tom[tom]):
                sub(ne, "MidiNoteEvent", Time=num(t), Duration=num(d), Velocity=v, OffVelocity=64,
                    NoteId=proximo_note_id)
                proximo_note_id += 1
            sub(kt, "MidiKey", tom)
    sub(sub(notas_el, "PerNoteEventStore"), "EventLists")
    sub(notas_el, "NoteProbabilityGroups")
    sub(sub(notas_el, "ProbabilityGroupIdGenerator"), "NextId", 1)
    sub(sub(notas_el, "NoteIdGenerator"), "NextId", proximo_note_id)
    sub(c, "BankSelectCoarse", -1)
    sub(c, "BankSelectFine", -1)
    sub(c, "ProgramChange", -1)
    return c


def nome_parte(p, t):
    tpc = p["numerador"] * 4.0 / p["denominador"]
    for pa in p["partes"]:
        if pa["compIni"] * tpc <= t + 1e-6 < pa["compFim"] * tpc:
            return pa["nome"]
    return ""


def exportar(p, modelo, pasta, com_notas):
    raiz = ET.fromstring(gzip.decompress(open(modelo, "rb").read()))
    ls = raiz.find("LiveSet")
    tracks = ls.find("Tracks")
    molde_audio = next(t for t in tracks if t.tag == "AudioTrack")
    molde_midi = next(t for t in tracks if t.tag == "MidiTrack")
    returns = [t for t in tracks if t.tag == "ReturnTrack"]
    max_track_id = max(int(t.get("Id")) for t in tracks)
    for t in list(tracks):
        tracks.remove(t)

    maior = 0
    for e in raiz.iter():
        if eh_alvo(e.tag) and "Id" in e.attrib:
            maior = max(maior, int(e.get("Id")))
    prox = max(int(ls.find("NextPointeeId").get("Value")), maior + 1)
    prox_track = max_track_id + 1

    nome_audio = os.path.basename(p["caminhoAudio"])
    abs_audio = os.path.abspath(os.path.join(pasta, "Audio", nome_audio)).replace("\\", "/")

    for fi, f in enumerate(p["faixas"]):
        molde = molde_audio if f["tipo"] == "audio" else molde_midi
        t = copy.deepcopy(molde)
        mapa = {}
        for e in t.iter():
            if eh_alvo(e.tag) and "Id" in e.attrib:
                mapa[e.get("Id")] = str(prox)
                e.set("Id", str(prox))
                prox += 1
        for e in t.iter("PointeeId"):
            if e.get("Value") in mapa:
                e.set("Value", mapa[e.get("Value")])
        t.set("Id", str(prox_track))
        prox_track += 1
        t.find("Name/EffectiveName").set("Value", f["nome"])
        t.find("Name/UserName").set("Value", f["nome"])
        cor = cor_live(f["cor"])
        t.find("Color").set("Value", str(cor))
        gid = t.find("TrackGroupId")
        if gid is not None:
            gid.set("Value", "-1")
        if f["tipo"] == "audio":
            evs = t.find("DeviceChain/MainSequencer/Sample/ArrangerAutomation/Events")
            for x in list(evs):
                evs.remove(x)
            evs.append(clipe_audio(p, 0, p["nomeMusica"], cor, abs_audio, "Audio/" + nome_audio))
        else:
            evs = t.find("DeviceChain/MainSequencer/ClipTimeable/ArrangerAutomation/Events")
            for x in list(evs):
                evs.remove(x)
            for ci, cl in enumerate(sorted(f["clipes"], key=lambda c: c["inicio"])):
                if cl["fim"] <= cl["inicio"]:
                    continue
                nome = nome_parte(p, cl["inicio"]) or f["nome"]
                evs.append(clipe_midi(p, len(evs), nome, cor, cl, com_notas))
        tracks.append(t)
    for r in returns:
        tracks.append(r)

    # tempo e compasso no MainTrack (Live 12) / MasterTrack (Live 11)
    mt = ls.find("MainTrack")
    if mt is None:
        mt = ls.find("MasterTrack")
    mixer = mt.find("DeviceChain/Mixer")
    meter = 99 * (p["denominador"].bit_length() - 1) + p["numerador"] - 1
    for nome_par, valor in (("Tempo", num(p["bpm"])), ("TimeSignature", str(meter))):
        par = mixer.find(nome_par)
        par.find("Manual").set("Value", valor)
        alvo = par.find("AutomationTarget").get("Id")
        for env in mt.findall("AutomationEnvelopes/Envelopes/AutomationEnvelope"):
            if env.find("EnvelopeTarget/PointeeId").get("Value") == alvo:
                for ev in env.find("Automation/Events"):
                    ev.set("Value", valor)

    # locators
    lo_out = ls.find("Locators")
    los = lo_out.find("Locators")
    for x in list(los):
        los.remove(x)
    tpc = p["numerador"] * 4.0 / p["denominador"]
    for i, pa in enumerate(p["partes"]):
        lo = sub(los, "Locator", Id=i)
        sub(lo, "LomId", 0)
        sub(lo, "Time", num(pa["compIni"] * tpc))
        sub(lo, "Name", pa["nome"])
        sub(lo, "Annotation", "")
        sub(lo, "IsSongStart", "false")

    ls.find("NextPointeeId").set("Value", str(prox))

    xml = b'<?xml version="1.0" encoding="UTF-8"?>\n' + ET.tostring(raiz, encoding="unicode").encode("utf-8")
    destino = os.path.join(pasta, p["nomeMusica"] + ".als")
    with open(destino, "wb") as fh:
        fh.write(gzip.compress(xml))
    return destino


def projeto_exemplo(pasta):
    os.makedirs(os.path.join(pasta, "Audio"), exist_ok=True)
    wav = os.path.join(pasta, "Audio", "referencia.wav")
    sr, dur = 44100, 60.0
    n = int(sr * dur)
    with open(wav, "wb") as fh:
        dados = 4 * n
        fh.write(b"RIFF" + struct.pack("<I", 36 + dados) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 1, 2, sr, sr * 4, 4, 16))
        fh.write(b"data" + struct.pack("<I", dados) + b"\0" * dados)
    bpm = 128.0
    p = dict(nomeMusica="Exemplo MAPA", caminhoAudio=wav, duracaoSeg=dur, sampleRate=sr, bpm=bpm,
             numerador=4, denominador=4, audioInicioTempos=0.0, audioCorteSeg=0.05,
             tamanhoArquivo=os.path.getsize(wav))
    p["audioFimTempos"] = p["audioInicioTempos"] + (dur - p["audioCorteSeg"]) / (60.0 / bpm)
    p["partes"] = [dict(nome="INTRO", compIni=0, compFim=8), dict(nome="BUILD", compIni=8, compFim=16),
                   dict(nome="DROP", compIni=16, compFim=32)]
    kick = dict(nome="KICK", cor=0xFF5533, tipo="midi", clipes=[])
    baixo = dict(nome="BAIXO", cor=0x3366FF, tipo="midi", clipes=[])
    hats = dict(nome="HATS", cor=0xFFEE00, tipo="midi", clipes=[])
    for pa in p["partes"]:
        a, b = pa["compIni"] * 4.0, pa["compFim"] * 4.0
        kick["clipes"].append(dict(inicio=a, fim=b, notas=[dict(inicio=a + i, dur=0.25, tom=36, vel=110) for i in range(int(b - a))]))
        baixo["clipes"].append(dict(inicio=a, fim=b, notas=[dict(inicio=a + i + 0.5, dur=0.5, tom=41 + (i % 3) * 2, vel=100) for i in range(int(b - a))]))
        hats["clipes"].append(dict(inicio=a, fim=b, notas=[dict(inicio=a + i * 0.5 + 0.5, dur=0.125, tom=42, vel=80) for i in range(int((b - a) * 2) - 1)]))
    p["faixas"] = [dict(nome="REFERÊNCIA", cor=0xAAAAAA, tipo="audio", clipes=[]), kick, baixo, hats]
    return p


if __name__ == "__main__":
    modelo, pasta = sys.argv[1], sys.argv[2]
    p = projeto_exemplo(pasta)
    print(exportar(p, modelo, pasta, "--sem-notas" not in sys.argv))

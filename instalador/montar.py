#!/usr/bin/env python3
"""Monta o instalador de um pacote de plugins (linha criativa, linha de mix, instrumentos).

    python3 instalador/montar.py --sistema mac|win --pacote Mix --origem linha/mix --versao 0.1.15 --saida .

--origem é a pasta com VST3/ (e AU/ no Mac), do jeito que o passo "Empacotar" deixa.
Gera InsideStudioSP-<pacote>-win.exe (Inno Setup) ou InsideStudioSP-<pacote>-mac.dmg (com um .pkg dentro).

Os dois mostram a lista de plugins, todos marcados, e o usuário desmarca o que não quer.
Windows: a pasta vem preenchida com a padrão do VST3 (C:\\Program Files\\Common Files\\VST3) e pode ser trocada.
Mac: VST3 e AU vão para as pastas padrão (/Library/Audio/Plug-Ins/...); o usuário escolhe
"todos os usuários" ou "só para mim" (~/Library/...) no passo "Destino".
Sem assinatura: o Windows mostra o SmartScreen e o Mac pede "Abrir mesmo assim" na primeira vez.

Desinstalar: no Windows, "Aplicativos instalados" (InsideStudioSP <pacote>) ou o atalho "Desinstalar" no menu Iniciar;
no Mac, "Desinstalar InsideStudioSP <pacote>.command" dentro do .dmg (apaga os .vst3/.component do pacote nas
pastas do sistema e do usuário). Os presets do usuário ficam.

Bancos de presets do CHUTE (.chutebank):
    --bancos instrumentos/chute/bancos   o instalador do CHUTE leva também os bancos da pasta (só os *.chutebank da
                                         raiz; a fábrica fica em bancos/fabrica e vai dentro do plugin)
    --so-bancos --pacote ChuteBancos --bancos instrumentos/chute/bancos --versao 1.0
                                         pacote SÓ de presets (sem plugin): InsideStudioSP-ChuteBancos-win.exe / -mac.dmg
Os bancos vão para a pasta de bancos de todos os usuários (Windows: C:\\ProgramData\\InsideStudioSP\\CHUTE\\Bancos;
Mac: /Library/Application Support/InsideStudioSP/CHUTE/Bancos) e ficam lá ao desinstalar. O CHUTE lê a pasta quando
o DAW abre.
"""
import argparse
import json
import os
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
import time

AQUI = os.path.dirname(os.path.abspath(__file__))
MARCA = "InsideStudioSP"


def catalogo():
    with open(os.path.join(AQUI, "catalogo.json"), encoding="utf-8") as f:
        c = json.load(f)
    c.pop("_", None)
    return c


def plugins_da_origem(origem):
    """Nomes dos plugins (PRODUCT_NAME) na ordem do catálogo; os que não estão no catálogo vão no fim."""
    pasta = os.path.join(origem, "VST3")
    nomes = sorted(n[:-5] for n in os.listdir(pasta) if n.endswith(".vst3"))
    if not nomes:
        sys.exit(f"nenhum .vst3 em {pasta}")
    ordem = list(catalogo())
    return sorted(nomes, key=lambda n: (ordem.index(n) if n in ordem else len(ordem), n))


def rotulo(nome):
    titulo, desc = catalogo().get(nome, [nome, ""])
    return titulo, desc


# nomes antigos de um plugin (renomeado): o instalador novo apaga o bundle velho e o desinstalador também
ANTIGOS = {"MERCURIO": ["VACINA"]}


# pacote que já saiu com outro nome: no Windows o AppId e a pasta do desinstalador continuam os antigos, para a versão
# nova substituir a instalada (e não aparecer duas vezes em Aplicativos instalados); o atalho antigo é apagado
PACOTE_ANTIGO = {"Quimera": "Instrumentos"}


def com_antigos(plugins):
    return plugins + [a for p in plugins for a in ANTIGOS.get(p, [])]


def ident(nome):
    return re.sub(r"[^a-z0-9]", "", nome.lower())


def rodar(*cmd):
    print("+", " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True)


BANCOS_WIN = r"{commonappdata}\InsideStudioSP\CHUTE\Bancos"
BANCOS_MAC = "/Library/Application Support/InsideStudioSP/CHUTE/Bancos"


def bancos_da_pasta(pasta):
    """Os .chutebank da raiz da pasta (bancos/fabrica fica de fora: vai dentro do plugin)."""
    if not pasta:
        return []
    return sorted(os.path.join(pasta, n) for n in os.listdir(pasta) if n.lower().endswith(".chutebank"))


# ---------------------------------------------------------------- Windows (Inno Setup)

def achar_iscc():
    for c in (shutil.which("iscc"), r"C:\Program Files (x86)\Inno Setup 6\ISCC.exe", r"C:\Program Files\Inno Setup 6\ISCC.exe"):
        if c and os.path.exists(c):
            return c
    sys.exit("ISCC.exe (Inno Setup 6) não encontrado")


def iss(pacote, versao, origem, plugins, saida, nome_arquivo, bancos=()):
    origem = os.path.abspath(origem)
    antigo = PACOTE_ANTIGO.get(pacote)
    L = [
        "[Setup]",
        f"AppId={MARCA}.{antigo or pacote}",
        f"AppName={MARCA} {pacote}",
        f"AppVersion={versao}",
        f"AppVerName={MARCA} {pacote} {versao}",
        f"AppPublisher={MARCA}",
        r"DefaultDirName={commoncf64}\VST3",
        "AppendDefaultDirName=no",
        "DirExistsWarning=no",
        "UsePreviousAppDir=yes",
        "DisableProgramGroupPage=yes",
        "DisableReadyPage=no",
        # o desinstalador fica fora da pasta VST3, para o DAW não ver arquivo estranho lá
        rf"UninstallFilesDir={{commonpf64}}\{MARCA}\{antigo or pacote}",
        f"UninstallDisplayName={MARCA} {pacote}",
        "PrivilegesRequired=admin",
        "ArchitecturesAllowed=x64compatible",
        "ArchitecturesInstallIn64BitMode=x64compatible",
        "WizardStyle=modern",
        "Compression=lzma2/max",
        "SolidCompression=yes",
        f"OutputDir={os.path.abspath(saida)}",
        f"OutputBaseFilename={nome_arquivo}",
        "",
        "[Languages]",
        r'Name: "pt"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"',
        "",
        "[Messages]",
        "SelectDirLabel3=Os plugins VST3 vão para a pasta abaixo. Ela já é a pasta padrão do Windows, onde o Ableton e os outros DAWs procuram. Só troque se você usa outra pasta de VST3.",
        "SelectComponentsLabel2=Desmarque os plugins que você não quer instalar.",
        "",
        "[Types]",
        'Name: "todos"; Description: "Todos os plugins"',
        'Name: "escolher"; Description: "Escolher os plugins"; Flags: iscustom',
        "",
        "[Components]",
    ]
    for p in plugins:
        titulo, desc = rotulo(p)
        texto = f"{titulo} — {desc}" if desc else titulo
        L.append(f'Name: "{ident(p)}"; Description: "{texto}"; Types: todos escolher')
    antigos = [a for p in plugins for a in ANTIGOS.get(p, [])]
    if antigos or antigo:
        L += ["", "[InstallDelete]"]
        L += [rf'Type: filesandordirs; Name: "{{app}}\{a}.vst3"' for a in antigos]
        if antigo:
            L.append(rf'Type: files; Name: "{{autoprograms}}\{MARCA}\Desinstalar {MARCA} {antigo}.lnk"')
    L += ["", "[Icons]", rf'Name: "{{autoprograms}}\{MARCA}\Desinstalar {MARCA} {pacote}"; Filename: "{{uninstallexe}}"']
    L += ["", "[Files]"]
    for p in plugins:
        src = os.path.join(origem, "VST3", p + ".vst3")
        L.append(rf'Source: "{src}\*"; DestDir: "{{app}}\{p}.vst3"; Components: {ident(p)}; '
                 "Flags: ignoreversion recursesubdirs createallsubdirs")
    # bancos do CHUTE: junto com o componente CHUTE; ficam ao desinstalar (o pacote de presets pode ter posto o mesmo)
    comp = f"; Components: {ident('CHUTE')}" if "CHUTE" in plugins else ""
    for b in bancos:
        L.append(rf'Source: "{os.path.abspath(b)}"; DestDir: "{BANCOS_WIN}"{comp}; Flags: ignoreversion uninsneveruninstall')
    return "\r\n".join(L) + "\r\n"


def iss_bancos(pacote, versao, bancos, saida, nome_arquivo):
    """Pacote só de presets do CHUTE: sem pasta para escolher, sem lista; põe os bancos e pronto."""
    nomes = ", ".join(os.path.splitext(os.path.basename(b))[0] for b in bancos)
    L = [
        "[Setup]",
        f"AppId={MARCA}.{pacote}",
        f"AppName={MARCA} {pacote}",
        f"AppVersion={versao}",
        f"AppVerName={MARCA} {pacote} {versao}",
        f"AppPublisher={MARCA}",
        f"DefaultDirName={BANCOS_WIN}",
        "DisableDirPage=yes",
        "DisableProgramGroupPage=yes",
        "DisableReadyPage=no",
        rf"UninstallFilesDir={{commonpf64}}\{MARCA}\{pacote}",
        f"UninstallDisplayName={MARCA} {pacote}",
        "PrivilegesRequired=admin",
        "ArchitecturesAllowed=x64compatible",
        "ArchitecturesInstallIn64BitMode=x64compatible",
        "WizardStyle=modern",
        "Compression=lzma2/max",
        "SolidCompression=yes",
        f"OutputDir={os.path.abspath(saida)}",
        f"OutputBaseFilename={nome_arquivo}",
        "",
        "[Languages]",
        r'Name: "pt"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"',
        "",
        "[Messages]",
        f"WelcomeLabel2=Bancos de presets do CHUTE: {nomes}.%n%nEles aparecem no menu de presets do CHUTE na próxima vez que você abrir o DAW. Feche o DAW antes de continuar.",
        "",
        "[Icons]",
        rf'Name: "{{autoprograms}}\{MARCA}\Desinstalar {MARCA} {pacote}"; Filename: "{{uninstallexe}}"',
        "",
        "[Files]",
    ]
    L += [rf'Source: "{os.path.abspath(b)}"; DestDir: "{{app}}"; Flags: ignoreversion' for b in bancos]
    return "\r\n".join(L) + "\r\n"


def montar_win(a, plugins):
    nome = f"{MARCA}-{a.pacote}-win"
    bancos = bancos_da_pasta(a.bancos)
    if a.so_bancos:
        script = iss_bancos(a.pacote, a.versao, bancos, a.saida, nome)
    else:
        script = iss(a.pacote, a.versao, a.origem, plugins, a.saida, nome, bancos)
    caminho = os.path.join(tempfile.mkdtemp(), f"{nome}.iss")
    with open(caminho, "w", encoding="utf-8-sig") as f:  # Inno lê UTF-8 com BOM (acentos)
        f.write(script)
    print(script)
    if a.so_script:
        return
    rodar(achar_iscc(), "/Q", caminho)
    print("gerado:", os.path.join(a.saida, nome + ".exe"))


# ---------------------------------------------------------------- Mac (pkg dentro de dmg)

DESTINO = {"VST3": ("vst3", "/Library/Audio/Plug-Ins/VST3"), "AU": ("component", "/Library/Audio/Plug-Ins/Components")}


POSTINSTALL = """#!/bin/bash
# $2 = pasta onde este plugin acabou de ser instalado (/Library/... ou ~/Library/..., conforme o "Destino")
B="{bundle}"; SUB="{sub}"
AQUI="${{2%/}}"
case "$AQUI" in */"$SUB") ;; *) exit 0;; esac   # $2 inesperado: não apaga nada
[ -e "$AQUI/$B" ] || exit 0                       # só apaga a outra cópia se esta foi mesmo posta
U=$(stat -f%Su /dev/console 2>/dev/null)
H=$(dscl . -read "/Users/$U" NFSHomeDirectory 2>/dev/null | awk '{{print $2}}')
for d in "/Library/$SUB" "$H/Library/$SUB"; do
  [ "$d" = "$AQUI" ] && continue
  [ -e "$d/$B" ] && rm -rf "$d/$B"
done
killall -9 AudioComponentRegistrar 2>/dev/null
exit 0
"""


def pkg_componente(bundle, fmt, ident_pkg, versao, pasta_pkgs, trabalho):
    ext, local = DESTINO[fmt]
    raiz = os.path.join(trabalho, ident_pkg)
    os.makedirs(raiz)
    shutil.copytree(bundle, os.path.join(raiz, os.path.basename(bundle)), symlinks=True)
    plist = raiz + ".plist"
    rodar("pkgbuild", "--analyze", "--root", raiz, plist)
    with open(plist, "rb") as f:
        itens = plistlib.load(f)
    for i in itens:  # sem isso o Installer "segue" uma cópia antiga do bundle e instala em outro lugar
        i["BundleIsRelocatable"] = False
        i["BundleOverwriteAction"] = "upgrade"
        # o VST3 e o AU têm o MESMO CFBundleIdentifier (JUCE): com a checagem de versão ligada o Installer achava
        # "a mesma versão já instalada" (o .component recém-posto) e pulava o .vst3 — o VST3 ficava velho
        i["BundleIsVersionChecked"] = False
        i["BundleHasStrictIdentifier"] = False
    with open(plist, "wb") as f:
        plistlib.dump(itens, f)
    # postinstall: apaga a cópia do MESMO plugin no outro lugar (/Library x ~/Library). Com duas cópias o AU pega a
    # mais nova sozinho, mas o VST3 o DAW carrega a que achar primeiro — e o VST3 ficava na versão velha.
    scripts = raiz + "-scripts"
    os.makedirs(scripts)
    with open(os.path.join(scripts, "postinstall"), "w", encoding="utf-8", newline="\n") as f:
        f.write(POSTINSTALL.format(bundle=os.path.basename(bundle), sub=local[len("/Library/"):]))
    os.chmod(os.path.join(scripts, "postinstall"), 0o755)
    arq = f"{ident_pkg}.pkg"
    rodar("pkgbuild", "--root", raiz, "--component-plist", plist, "--identifier", ident_pkg, "--scripts", scripts,
          "--version", versao, "--install-location", local, os.path.join(pasta_pkgs, arq))
    return arq


def pkg_bancos(bancos, versao, pasta_pkgs, trabalho):
    """Os .chutebank num pkg que instala em /Library/Application Support/InsideStudioSP/CHUTE/Bancos."""
    ident_pkg = "com.insidestudiosp.pkg.chutebancos"
    raiz = os.path.join(trabalho, ident_pkg)
    os.makedirs(raiz)
    for b in bancos:
        shutil.copy2(b, raiz)
    arq = f"{ident_pkg}.pkg"
    rodar("pkgbuild", "--root", raiz, "--identifier", ident_pkg, "--version", versao,
          "--install-location", BANCOS_MAC, os.path.join(pasta_pkgs, arq))
    return ident_pkg, arq


def distribution(pacote, versao, escolhas):
    from xml.sax.saxutils import escape, quoteattr
    L = ['<?xml version="1.0" encoding="utf-8"?>',
         '<installer-gui-script minSpecVersion="2">',
         f"  <title>{escape(MARCA + ' ' + pacote)}</title>",
         # customize="always": abre direto na lista de plugins, todos marcados
         '  <options customize="always" require-scripts="false" hostArchitectures="x86_64,arm64"/>',
         # SÓ /Library (todos os usuários): com a opção "só para mim" o plugin ia para ~/Library e ficavam duas cópias
         # (a velha numa pasta, a nova na outra) — o DAW carregava o VST3 velho. Sem pergunta de destino.
         '  <domains enable_localSystem="true" enable_currentUserHome="false" enable_anywhere="false"/>',
         '  <allowed-os-versions><os-version min="11.0"/></allowed-os-versions>',
         "  <choices-outline>"]
    L += [f'    <line choice="{e["id"]}"/>' for e in escolhas]
    L.append("  </choices-outline>")
    for e in escolhas:
        L.append(f'  <choice id="{e["id"]}" title={quoteattr(e["titulo"])} description={quoteattr(e["desc"])} start_selected="true">')
        L += [f'    <pkg-ref id="{r}"/>' for r, _ in e["pkgs"]]
        L.append("  </choice>")
    for e in escolhas:
        for r, arq in e["pkgs"]:
            L.append(f'  <pkg-ref id="{r}" version="{versao}" onConclusion="none">#{arq}</pkg-ref>')
    L.append("</installer-gui-script>")
    return "\n".join(L) + "\n"


LEIAME = """{marca} {pacote} {versao}

1. Abra "Instalar {marca} {pacote}.pkg".
2. Na lista, desmarque os plugins que você não quer e clique em Instalar.
   Ele instala para todos os usuários do Mac (pede a senha de administrador) e apaga cópias antigas
   que tenham ficado na sua pasta de usuário (~/Library), para o DAW não carregar a versão velha.
3. Os plugins vão para as pastas padrão do Mac, onde o Ableton e os outros DAWs procuram:
   VST3: /Library/Audio/Plug-Ins/VST3
   AU:   /Library/Audio/Plug-Ins/Components

Se o Mac disser que não pode abrir o instalador (desenvolvedor não identificado):
Ajustes do Sistema > Privacidade e Segurança > role até o aviso do instalador > "Abrir Mesmo Assim".

Para desinstalar: abra "Desinstalar {marca} {pacote}.command" (clique com o botão direito > Abrir).
Ele apaga os plugins deste pacote das pastas do sistema e da sua pasta de usuário; os seus presets ficam.
"""

DESINSTALAR = """#!/bin/bash
# Desinstala o {marca} {pacote}: apaga os plugins deste pacote (VST3 e AU) das pastas do sistema e do usuário
# e esquece o registro do instalador. Os presets do usuário ficam.
echo "Desinstalar {marca} {pacote}"
echo "Plugins: {lista}"
echo
read -r -p "Confirma? (s/n) " r
case "$r" in s|S|sim|SIM) ;; *) echo "Nada foi apagado."; exit 0;; esac
echo "O Mac vai pedir a sua senha (para apagar de /Library)."
for p in {nomes}; do
  for d in "/Library/Audio/Plug-Ins/VST3" "$HOME/Library/Audio/Plug-Ins/VST3"; do
    [ -e "$d/$p.vst3" ] && sudo rm -rf "$d/$p.vst3" && echo "apagado: $d/$p.vst3"
  done
  for d in "/Library/Audio/Plug-Ins/Components" "$HOME/Library/Audio/Plug-Ins/Components"; do
    [ -e "$d/$p.component" ] && sudo rm -rf "$d/$p.component" && echo "apagado: $d/$p.component"
  done
done
for r in $(pkgutil --pkgs | grep -E '^com\.insidestudiosp\.pkg\.({ids})\.'); do sudo pkgutil --forget "$r" >/dev/null; done
killall -9 AudioComponentRegistrar 2>/dev/null   # o Mac esquece os AU apagados sem precisar reiniciar
echo
echo "Pronto. Feche e abra o DAW de novo."
read -r -p "Enter para fechar."
"""


def montar_mac(a, plugins):
    trabalho = tempfile.mkdtemp()
    pkgs = os.path.join(trabalho, "pkgs")
    os.makedirs(pkgs)
    escolhas = []
    for p in plugins:
        titulo, desc = rotulo(p)
        e = {"id": ident(p), "titulo": titulo, "desc": desc, "pkgs": []}
        for fmt, (ext, _) in DESTINO.items():
            bundle = os.path.join(a.origem, fmt, f"{p}.{ext}")
            if os.path.isdir(bundle):
                r = f"com.insidestudiosp.pkg.{ident(p)}.{ext}"
                e["pkgs"].append((r, pkg_componente(bundle, fmt, r, a.versao, pkgs, trabalho)))
        escolhas.append(e)
    bancos = bancos_da_pasta(a.bancos)
    if bancos:
        pb = pkg_bancos(bancos, a.versao, pkgs, trabalho)
        dono = next((e for e in escolhas if e["id"] == ident("CHUTE")), None)
        if dono is not None:   # junto com o CHUTE
            dono["pkgs"].append(pb)
        else:                  # pacote só de presets
            nomes = ", ".join(os.path.splitext(os.path.basename(b))[0] for b in bancos)
            escolhas.append({"id": "chutebancos", "titulo": "Bancos de presets do CHUTE", "desc": nomes, "pkgs": [pb]})
    dist = os.path.join(trabalho, "distribution.xml")
    with open(dist, "w", encoding="utf-8") as f:
        f.write(distribution(a.pacote, a.versao, escolhas))
    print(open(dist, encoding="utf-8").read())
    palco = os.path.join(trabalho, "dmg")
    os.makedirs(palco)
    rodar("productbuild", "--distribution", dist, "--package-path", pkgs, "--version", a.versao,
          os.path.join(palco, f"Instalar {MARCA} {a.pacote}.pkg"))
    if a.so_bancos:
        with open(os.path.join(palco, "LEIA-ME.txt"), "w", encoding="utf-8") as f:
            f.write(LEIAME_BANCOS.format(marca=MARCA, pacote=a.pacote, versao=a.versao, pasta=BANCOS_MAC))
        return gerar_dmg(a, palco)
    with open(os.path.join(palco, "LEIA-ME.txt"), "w", encoding="utf-8") as f:
        f.write(LEIAME.format(marca=MARCA, pacote=a.pacote, versao=a.versao))
    nomes = com_antigos(plugins)
    des = os.path.join(palco, f"Desinstalar {MARCA} {a.pacote}.command")
    with open(des, "w", encoding="utf-8", newline="\n") as f:
        f.write(DESINSTALAR.format(marca=MARCA, pacote=a.pacote, lista=", ".join(rotulo(p)[0] for p in plugins),
                                   nomes=" ".join(nomes), ids="|".join(ident(n) for n in nomes)))
    os.chmod(des, 0o755)
    gerar_dmg(a, palco)


def gerar_dmg(a, palco):
    dmg = os.path.join(a.saida, f"{MARCA}-{a.pacote}-mac.dmg")
    for tentativa in range(3):  # o hdiutil dos runners às vezes falha com "Resource busy"
        try:
            rodar("hdiutil", "create", "-volname", f"{MARCA} {a.pacote}", "-srcfolder", palco, "-ov", "-format", "UDZO", dmg)
            break
        except subprocess.CalledProcessError:
            if tentativa == 2:
                raise
            time.sleep(10)
    print("gerado:", dmg)


LEIAME_BANCOS = """{marca} {pacote} {versao}

Bancos de presets do CHUTE.

1. Feche o DAW.
2. Abra "Instalar {marca} {pacote}.pkg" e clique em Instalar (pede a senha de administrador).
3. Abra o DAW: os bancos novos aparecem no menu de presets do CHUTE.

Os bancos vão para {pasta}.
Para tirar um banco, apague o arquivo .chutebank dessa pasta.

Se o Mac disser que não pode abrir o instalador (desenvolvedor não identificado):
Ajustes do Sistema > Privacidade e Segurança > role até o aviso do instalador > "Abrir Mesmo Assim".
"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sistema", choices=["mac", "win"], required=True)
    ap.add_argument("--pacote", required=True, help="Criativo, Mix, Quimera…")
    ap.add_argument("--origem", help="pasta com VST3/ (e AU/); não precisa com --so-bancos")
    ap.add_argument("--versao", required=True)
    ap.add_argument("--saida", default=".")
    ap.add_argument("--bancos", help="pasta com os .chutebank do CHUTE (vão junto, ou sozinhos com --so-bancos)")
    ap.add_argument("--so-bancos", action="store_true", help="pacote só de presets do CHUTE, sem plugin")
    ap.add_argument("--so-script", action="store_true", help="Windows: só mostra o .iss, sem compilar")
    a = ap.parse_args()
    os.makedirs(a.saida, exist_ok=True)
    if a.so_bancos:
        if not bancos_da_pasta(a.bancos):
            sys.exit(f"nenhum .chutebank em {a.bancos}")
        print(f"{MARCA} {a.pacote} {a.versao} ({a.sistema}): só bancos")
        (montar_mac if a.sistema == "mac" else montar_win)(a, [])
        return
    if not a.origem:
        sys.exit("falta --origem")
    plugins = plugins_da_origem(a.origem)
    print(f"{MARCA} {a.pacote} {a.versao} ({a.sistema}): {', '.join(plugins)}")
    (montar_mac if a.sistema == "mac" else montar_win)(a, plugins)


if __name__ == "__main__":
    main()

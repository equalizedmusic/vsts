#!/usr/bin/env python3
"""Modelos do CHUTE cifrados no repositório (o repo fica público durante a compilação e os modelos são o coração do
produto: o Modelos.h em claro NUNCA vai para o git).

    CHUTE_MODELOS_KEY=<64 hex> python3 modelos_cifra.py fecha Source/Modelos.h Source/Modelos.h.cifrado
    CHUTE_MODELOS_KEY=<64 hex> python3 modelos_cifra.py abre  Source/Modelos.h.cifrado <saida>/Modelos.h

Cifra de fluxo com SHA-256 em modo contador (chave 256 bits + nonce aleatório de 16 bytes) e HMAC-SHA-256 sobre
nonce + texto cifrado: chave errada ou arquivo mexido = erro (não gera lixo). Só biblioteca padrão (roda no runner
do GitHub sem instalar nada). A chave fica no segredo CHUTE_MODELOS_KEY do repositório e, no servidor, em
D:\\Apps\\_tools\\gh\\chute-modelos.key.dpapi.
"""
import hashlib
import hmac
import os
import sys

MAGIA = b"CHTM1"


def fluxo(chave, nonce, n):
    saida = bytearray()
    i = 0
    while len(saida) < n:
        saida += hashlib.sha256(chave + nonce + i.to_bytes(8, "big")).digest()
        i += 1
    return bytes(saida[:n])


def chave():
    h = os.environ.get("CHUTE_MODELOS_KEY", "").strip()
    if len(h) != 64:
        sys.exit("CHUTE_MODELOS_KEY ausente ou inválida (64 hex)")
    return bytes.fromhex(h)


def fecha(ent, sai):
    k = chave()
    dados = open(ent, "rb").read()
    nonce = os.urandom(16)
    ct = bytes(a ^ b for a, b in zip(dados, fluxo(k, nonce, len(dados))))
    tag = hmac.new(k, MAGIA + nonce + ct, hashlib.sha256).digest()
    open(sai, "wb").write(MAGIA + nonce + ct + tag)


def abre(ent, sai):
    k = chave()
    b = open(ent, "rb").read()
    if not b.startswith(MAGIA) or len(b) < len(MAGIA) + 48:
        sys.exit("arquivo cifrado inválido")
    nonce, ct, tag = b[5:21], b[21:-32], b[-32:]
    if not hmac.compare_digest(tag, hmac.new(k, MAGIA + nonce + ct, hashlib.sha256).digest()):
        sys.exit("chave errada ou arquivo alterado")
    os.makedirs(os.path.dirname(os.path.abspath(sai)), exist_ok=True)
    open(sai, "wb").write(bytes(a ^ b for a, b in zip(ct, fluxo(k, nonce, len(ct)))))


if __name__ == "__main__":
    if len(sys.argv) != 4 or sys.argv[1] not in ("fecha", "abre"):
        sys.exit(__doc__)
    (fecha if sys.argv[1] == "fecha" else abre)(sys.argv[2], sys.argv[3])

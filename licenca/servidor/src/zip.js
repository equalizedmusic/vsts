// Zip montado na hora e entregue em streaming, sem compressão (método 0 = "store").
// Serve para juntar o instalador (que vem do GitHub sem passar inteiro pela memória) com um texto gerado para a pessoa.
//
// O CRC32 do instalador precisa vir pronto (arquivo .crc32 do release ou tabela conhecida): no plano grátis o Worker tem
// 10 ms de CPU por pedido e calcular o CRC de 5–9 MB em JS leva 11–22 ms. O CRC dos textos (poucos KB) é calculado aqui.

const TABELA = (() => {
    const t = new Uint32Array(256);
    for (let n = 0; n < 256; n++) {
        let c = n;
        for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1;
        t[n] = c >>> 0;
    }
    return t;
})();

export function crc32(bytes) {
    let c = 0xFFFFFFFF;
    for (let i = 0; i < bytes.length; i++) c = TABELA[(c ^ bytes[i]) & 255] ^ (c >>> 8);
    return (c ^ 0xFFFFFFFF) >>> 0;
}

// data/hora no formato do DOS, no horário de Brasília (UTC−3, sem horário de verão desde 2019)
function dataDos(ms) {
    const d = new Date(ms - 3 * 3600 * 1000);
    const hora = (d.getUTCHours() << 11) | (d.getUTCMinutes() << 5) | (d.getUTCSeconds() >> 1);
    const data = ((d.getUTCFullYear() - 1980) << 9) | ((d.getUTCMonth() + 1) << 5) | d.getUTCDate();
    return { hora, data };
}

function cabecalho(tam, preencher) {
    const b = new Uint8Array(tam);
    preencher(new DataView(b.buffer));
    return b;
}

// itens: [{ nome, bytes }] (texto já em bytes) ou [{ nome, tamanho, crc, corpo }] (corpo = ReadableStream)
// -> { tamanho, stream }   tamanho exato do zip (vai no Content-Length)
export function zipStream(itens, quando = Date.now()) {
    const { hora, data } = dataDos(quando);
    const enc = new TextEncoder();
    const partes = [];      // Uint8Array ou ReadableStream, na ordem
    const central = [];
    let pos = 0;
    for (const it of itens) {
        const nome = enc.encode(it.nome);
        const tamanho = it.bytes ? it.bytes.length : it.tamanho;
        const crc = it.bytes ? crc32(it.bytes) : it.crc >>> 0;
        const local = cabecalho(30 + nome.length, (v) => {
            v.setUint32(0, 0x04034b50, true); v.setUint16(4, 20, true); v.setUint16(6, 0x0800, true);   // nome em UTF-8
            v.setUint16(8, 0, true); v.setUint16(10, hora, true); v.setUint16(12, data, true);
            v.setUint32(14, crc, true); v.setUint32(18, tamanho, true); v.setUint32(22, tamanho, true);
            v.setUint16(26, nome.length, true); v.setUint16(28, 0, true);
        });
        local.set(nome, 30);
        central.push(cabecalho(46 + nome.length, (v) => {
            v.setUint32(0, 0x02014b50, true); v.setUint16(4, 20, true); v.setUint16(6, 20, true); v.setUint16(8, 0x0800, true);
            v.setUint16(10, 0, true); v.setUint16(12, hora, true); v.setUint16(14, data, true);
            v.setUint32(16, crc, true); v.setUint32(20, tamanho, true); v.setUint32(24, tamanho, true);
            v.setUint16(28, nome.length, true); v.setUint32(42, pos, true);
        }));
        central[central.length - 1].set(nome, 46);
        partes.push(local, it.bytes || it.corpo);
        pos += local.length + tamanho;
    }
    const tamCentral = central.reduce((s, c) => s + c.length, 0);
    const fim = cabecalho(22, (v) => {
        v.setUint32(0, 0x06054b50, true); v.setUint16(8, itens.length, true); v.setUint16(10, itens.length, true);
        v.setUint32(12, tamCentral, true); v.setUint32(16, pos, true);
    });
    partes.push(...central, fim);
    const tamanho = pos + tamCentral + fim.length;

    // FixedLengthStream (Workers): o navegador recebe o Content-Length e mostra o progresso do download
    const { readable, writable } = typeof FixedLengthStream === 'function' ? new FixedLengthStream(tamanho) : new TransformStream();
    (async () => {
        const w = writable.getWriter();
        try {
            for (const p of partes) {
                if (p instanceof Uint8Array) { await w.write(p); continue; }
                const r = p.getReader();
                for (;;) { const { done, value } = await r.read(); if (done) break; await w.write(value); }
            }
            await w.close();
        } catch (e) { console.error('zip', e); await w.abort(e).catch(() => {}); }
    })();
    return { tamanho, stream: readable };
}

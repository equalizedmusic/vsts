#!/usr/bin/env node
// Administração das licenças InsideStudioSP.
//
//   node isp-licenca.mjs criar <email> <pacote> [--venda --pedido X] [--nota "..."] [--maquinas 2]
//   node isp-licenca.mjs listar [email]
//   node isp-licenca.mjs revogar <id> [--nota "..."]
//   node isp-licenca.mjs reativar <id>
//   node isp-licenca.mjs liberar <id> <maquina|id da ativação>
//   node isp-licenca.mjs novo-token <id>
//
// Configuração (fora do repositório): ISP_LICENCA_CONFIG ou ../../../segredos/licenca-admin.json
//   { "url": "https://isp-licencas.<conta>.workers.dev", "admin_key": "..." }
// Pacotes: criativo, mix, knob, quimera, limao, mercurio

import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const aqui = dirname(fileURLToPath(import.meta.url));
const arqConfig = process.env.ISP_LICENCA_CONFIG || join(aqui, '..', '..', '..', 'segredos', 'licenca-admin.json');
const config = JSON.parse(readFileSync(arqConfig, 'utf8'));

const [cmd, ...resto] = process.argv.slice(2);
const SEM_VALOR = ['--venda'];
const pos = [], opcoes = {};
for (let i = 0; i < resto.length; i++) {
    if (!resto[i].startsWith('--')) pos.push(resto[i]);
    else if (SEM_VALOR.includes(resto[i])) opcoes[resto[i].slice(2)] = true;
    else opcoes[resto[i].slice(2)] = resto[++i];
}
const opc = (nome) => opcoes[nome];

async function chamar(metodo, caminho, corpo) {
    const r = await fetch(config.url.replace(/\/$/, '') + caminho, {
        method: metodo,
        headers: { authorization: 'Bearer ' + config.admin_key, 'content-type': 'application/json' },
        body: corpo ? JSON.stringify(corpo) : undefined,
    });
    const j = await r.json();
    if (!j.ok) { console.error(`erro (${r.status}): ${j.mensagem || j.erro}`); process.exit(1); }
    return j;
}

const data = (t) => (t ? new Date(t * 1000).toLocaleString('pt-BR') : '-');

switch (cmd) {
    case 'criar': {
        const [email, pacote] = pos;
        const j = await chamar('POST', '/admin/licencas', {
            email, pacote, origem: opc('venda') ? 'venda' : 'cortesia',
            pedido: opc('pedido'), nota: opc('nota'), max_maquinas: Number(opc('maquinas')) || undefined,
        });
        console.log(`licença ${j.id}: ${j.email} · ${j.pacote}\ntoken: ${j.token}\n(o token só aparece agora; o servidor guarda só o hash)`);
        break;
    }
    case 'listar': {
        const j = await chamar('GET', '/admin/licencas' + (pos[0] ? '?email=' + encodeURIComponent(pos[0]) : ''));
        for (const l of j.licencas) {
            console.log(`#${l.id} ${l.email} · ${l.pacote} · ${l.status} · ${l.origem}${l.pedido ? ' ' + l.pedido : ''} · criada ${data(l.criada_em)}` +
                ` · ${l.maquinas.length}/${l.max_maquinas} máquinas${l.nota ? ' · ' + l.nota : ''}`);
            for (const m of l.maquinas)
                console.log(`    [${m.id}] ${m.nome_maquina || '(sem nome)'} · ativada ${data(m.criada_em)} · visto ${data(m.visto_em)}`);
        }
        if (!j.licencas.length) console.log('nenhuma licença');
        break;
    }
    case 'revogar': case 'reativar':
        await chamar('POST', '/admin/' + cmd, { id: Number(pos[0]), nota: opc('nota') });
        console.log('ok');
        break;
    case 'liberar':
        await chamar('POST', '/admin/liberar-maquina', { id: Number(pos[0]), maquina: pos[1] });
        console.log('ok');
        break;
    case 'novo-token': {
        const j = await chamar('POST', '/admin/novo-token', { id: Number(pos[0]) });
        console.log(`licença ${j.id}: ${j.email} · ${j.pacote}\ntoken novo: ${j.token}`);
        break;
    }
    default:
        console.log(readFileSync(fileURLToPath(import.meta.url), 'utf8').split('\n').slice(1, 17).join('\n'));
}

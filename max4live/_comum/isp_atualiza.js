// © Rogerio Cardoso / InsideStudioSP — todos os direitos reservados. Proibida a cópia, redistribuição ou obra derivada.
// Atualização automática dos devices (25/11/2026). Roda no [node.script <dev>_atualiza.js] (Node do Max), porque o
// [v8]/[js] do Max não consegue rodar um instalador. Gerado pelo build como <dev>_atualiza.js.
//
// O motor recebe do servidor (GET /v1/versao, a mesma rota dos VST) os dados do instalador ASSINADOS com a chave Ed25519
// das licenças: { v, tipo:"instalador", pacote, sistema, versao, sha256, tamanho, url }. No clique em UPDATE ele manda:
//     atualizar <pacote> <versão atual> <dados base64> <assinatura base64>
// Aqui: confere a assinatura e os campos, baixa o instalador para a pasta temporária, confere o SHA-256 e instala
// sem janela (só o usuário, sem senha de administrador, igual ao instalador do site):
//   Windows: InsideStudioSP-<Nome>-win.exe /VERYSILENT (Inno, PrivilegesRequired=lowest; usa a pasta da instalação anterior)
//   Mac:     monta o .dmg (hdiutil) e roda o .pkg com `installer -target CurrentUserHomeDirectory`
// A pasta do device é trocada na hora; o device que está aberto continua o antigo até ser carregado de novo.
// Responde ao motor:  atu_estado <baixando|conferindo|instalando|pronto|erro> <texto>
// Qualquer falha: o motor abre o link do instalador no navegador (o caminho antigo).
const maxApi = require('max-api');
const crypto = require('crypto');
const fs = require('fs');
const os = require('os');
const path = require('path');
const https = require('https');
const { spawn } = require('child_process');

// chave PÚBLICA das licenças (a mesma de isp_licenca.js / licenca.cmake)
const CHAVE = 'e318ad516db6cd3f9d6963fe4952aad30c17f33f1981c4005c283a0cfdef8978';
const HOSTS = /^https:\/\/(insidestudiosp\.com\.br|isp-licencas\.insidestudiosp\.workers\.dev|github\.com|objects\.githubusercontent\.com|release-assets\.githubusercontent\.com|[a-z0-9-]+\.githubusercontent\.com)\//;
const MAC = process.platform === 'darwin';

// o processo do Node nasce na pasta do device: sair dela para o instalador conseguir apagar a pasta velha
try { process.chdir(os.tmpdir()); } catch (e) { }

let ocupado = false;

function estado(e, texto) { maxApi.outlet('atu_estado', e, texto || ''); }
function log(t) { try { maxApi.post('atualiza: ' + t); } catch (e) { } }

function partes(v) { const m = String(v || '').match(/^(\d+)\.(\d+)\.(\d+)$/); return m ? m.slice(1).map(Number) : null; }
function maior(a, b) {
	const x = partes(a), y = partes(b);
	if (!x || !y) return false;
	for (let i = 0; i < 3; i++) if (x[i] !== y[i]) return x[i] > y[i];
	return false;
}

function confereAssinatura(dadosB64, assB64) {
	const spki = Buffer.concat([Buffer.from('302a300506032b6570032100', 'hex'), Buffer.from(CHAVE, 'hex')]);
	const chave = crypto.createPublicKey({ key: spki, format: 'der', type: 'spki' });
	const dados = Buffer.from(String(dadosB64), 'base64');
	const ass = Buffer.from(String(assB64), 'base64');
	if (ass.length !== 64 || !crypto.verify(null, dados, chave, ass)) return null;
	return JSON.parse(dados.toString('utf8'));
}

// GET com redirecionamentos (o /baixar do site manda para o link temporário do GitHub)
function baixar(url, destino, tamanho, saltos) {
	return new Promise((ok, falha) => {
		if (!HOSTS.test(url)) return falha(new Error('endereço inesperado: ' + url.split('?')[0]));
		const req = https.get(url, { headers: { 'user-agent': 'InsideStudioSP-M4L' }, timeout: 30000 }, (res) => {
			if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
				res.resume();
				if ((saltos || 0) >= 8) return falha(new Error('redirecionamentos demais'));
				return ok(baixar(new URL(res.headers.location, url).toString(), destino, tamanho, (saltos || 0) + 1));
			}
			if (res.statusCode !== 200) { res.resume(); return falha(new Error('HTTP ' + res.statusCode)); }
			const out = fs.createWriteStream(destino);
			const total = tamanho || Number(res.headers['content-length']) || 0;
			let lido = 0, ultimo = 0;
			res.on('data', (b) => {
				lido += b.length;
				const pct = total ? Math.floor(lido * 100 / total) : 0;
				if (pct >= ultimo + 10) { ultimo = pct; estado('baixando', pct + '%'); }
			});
			res.pipe(out);
			out.on('finish', () => out.close(() => ok(lido)));
			out.on('error', falha);
			res.on('error', falha);
		});
		req.on('timeout', () => req.destroy(new Error('sem resposta do servidor')));
		req.on('error', falha);
	});
}

function sha256(arquivo) {
	return new Promise((ok, falha) => {
		const h = crypto.createHash('sha256');
		fs.createReadStream(arquivo).on('data', (b) => h.update(b)).on('end', () => ok(h.digest('hex'))).on('error', falha);
	});
}

function rodar(cmd, args, opcoes) {
	return new Promise((ok) => {
		let saida = '';
		let p;
		try { p = spawn(cmd, args, Object.assign({ cwd: os.tmpdir(), windowsHide: true }, opcoes || {})); }
		catch (e) { return ok({ codigo: -1, saida: String(e) }); }
		p.stdout && p.stdout.on('data', (b) => { saida += b; });
		p.stderr && p.stderr.on('data', (b) => { saida += b; });
		p.on('error', (e) => ok({ codigo: -1, saida: saida + String(e) }));
		p.on('close', (c) => ok({ codigo: c, saida }));
	});
}

async function instalarWin(exe, pasta) {
	const args = ['/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/NOCLOSEAPPLICATIONS', '/NORESTARTAPPLICATIONS',
		'/LOG=' + path.join(pasta, 'instalador.log')];
	// só para teste (máquina de desenvolvimento): instala numa pasta de teste em vez da User Library
	if (process.env.ISP_ATUALIZA_DIR) args.push('/DIR=' + process.env.ISP_ATUALIZA_DIR);
	const r = await rodar(exe, args);
	if (r.codigo !== 0) throw new Error('o instalador terminou com código ' + r.codigo);
}

async function instalarMac(dmg, pasta) {
	const mnt = path.join(pasta, 'mnt');
	fs.mkdirSync(mnt, { recursive: true });
	const a = await rodar('/usr/bin/hdiutil', ['attach', '-nobrowse', '-readonly', '-noautoopen', '-mountpoint', mnt, dmg]);
	if (a.codigo !== 0) throw new Error('não montou o dmg (' + a.saida.trim().slice(0, 200) + ')');
	try {
		const pkg = fs.readdirSync(mnt).find((n) => /\.pkg$/i.test(n));
		if (!pkg) throw new Error('o dmg não tem o .pkg');
		const r = await rodar('/usr/sbin/installer', ['-pkg', path.join(mnt, pkg), '-target', 'CurrentUserHomeDirectory']);
		if (r.codigo !== 0) throw new Error('installer: ' + r.saida.trim().slice(0, 300));
	} finally {
		await rodar('/usr/bin/hdiutil', ['detach', mnt, '-force']);
	}
}

async function atualizar(pacote, versaoAtual, dadosB64, assB64) {
	if (ocupado) return;
	ocupado = true;
	try {
		let d = null;
		try { d = confereAssinatura(dadosB64, assB64); } catch (e) { d = null; }
		if (!d) throw new Error('assinatura do servidor não conferiu');
		const sistema = MAC ? 'mac' : 'win';
		if (d.v !== 1 || d.tipo !== 'instalador' || d.pacote !== String(pacote) || d.sistema !== sistema)
			throw new Error('dados do instalador não são deste device/sistema');
		if (!maior(d.versao, versaoAtual)) throw new Error('a versão ' + d.versao + ' não é mais nova que a ' + versaoAtual);
		if (!/^[0-9a-f]{64}$/.test(String(d.sha256))) throw new Error('sem SHA-256');

		const pasta = path.join(os.tmpdir(), 'InsideStudioSP-atualizacao', pacote + '-' + d.versao);
		fs.rmSync(pasta, { recursive: true, force: true });
		fs.mkdirSync(pasta, { recursive: true });
		const arquivo = path.join(pasta, MAC ? 'instalador.dmg' : 'instalador.exe');

		estado('baixando', '0%');
		const lido = await baixar(String(d.url), arquivo, Number(d.tamanho) || 0);
		if (d.tamanho && lido !== Number(d.tamanho)) throw new Error('download incompleto');
		estado('conferindo', '');
		if ((await sha256(arquivo)) !== d.sha256) { fs.rmSync(arquivo, { force: true }); throw new Error('o arquivo baixado não conferiu'); }

		estado('instalando', '');
		if (MAC) await instalarMac(arquivo, pasta); else await instalarWin(arquivo, pasta);
		try { fs.rmSync(arquivo, { force: true }); } catch (e) { }
		log(pacote + ' ' + d.versao + ' instalado');
		estado('pronto', d.versao);
	} catch (e) {
		log(pacote + ': ' + (e && e.message ? e.message : e));
		estado('erro', e && e.message ? e.message : String(e));
	} finally {
		ocupado = false;
	}
}

maxApi.addHandler('atualizar', (pacote, versaoAtual, dados, ass) => { atualizar(String(pacote), String(versaoAtual), dados, ass); });
maxApi.addHandler('ping', () => maxApi.outlet('atu_estado', 'vivo', ''));

// Renderiza video.html quadro a quadro e grava o MP4 (sem áudio; audio.py + mux depois).
// uso: node render.js <mix|criativo|synth> <saida.mp4>     |  node render.js <p> --frames 1,5.2,12 <pasta>  (só PNGs para conferir)
const puppeteer = require('puppeteer');
const { spawn } = require('child_process');
const path = require('path');
const FF = process.env.FFMPEG || 'ffmpeg';
const [p, a2, a3, a4] = process.argv.slice(2);
(async () => {
  const b = await puppeteer.launch({ headless: 'new', args: ['--no-sandbox', '--allow-file-access-from-files'],
    executablePath: process.env.CHROME || undefined });
  const pg = await b.newPage();
  await pg.setViewport({ width: 1080, height: 1920, deviceScaleFactor: 1 });
  const errs = []; pg.on('pageerror', e => errs.push(e.message));
  await pg.goto('file:///' + path.join(__dirname, 'video.html').replace(/\\/g, '/') + '?p=' + p);
  await pg.waitForFunction(() => window.render && [...document.images].every(i => i.complete && i.naturalWidth));
  await pg.evaluate(() => document.fonts.ready);
  if (a2 === '--frames') {
    for (const t of a3.split(',')) { await pg.evaluate(t => render(t), +t); await pg.screenshot({ path: path.join(a4, `${p}-${t}.png`) }); }
  } else {
    const { DUR, FPS } = await pg.evaluate(() => ({ DUR, FPS }));
    const ff = spawn(FF, ['-y', '-loglevel', 'error', '-f', 'image2pipe', '-framerate', String(FPS), '-c:v', 'mjpeg', '-i', '-',
      '-c:v', 'libx264', '-preset', 'slow', '-crf', '17', '-pix_fmt', 'yuv420p', '-r', String(FPS), a2]);
    ff.stderr.on('data', d => process.stderr.write(d));
    const n = DUR * FPS, t0 = Date.now();
    for (let i = 0; i < n; i++) {
      await pg.evaluate(t => render(t), i / FPS);
      const buf = await pg.screenshot({ type: 'jpeg', quality: 95 });
      if (!ff.stdin.write(buf)) await new Promise(r => ff.stdin.once('drain', r));
      if (i % 150 === 0) console.log(p, i, '/', n, Math.round((Date.now() - t0) / 1000) + 's');
    }
    ff.stdin.end(); await new Promise(r => ff.on('close', r));
  }
  console.log('erros:', errs);
  await b.close();
})();

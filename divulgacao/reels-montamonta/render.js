// Renderiza reels.html quadro a quadro em MP4 sem áudio. uso: node render.js x <saida.mp4>  |  node render.js x --frames 1,5.2 <pasta>
const puppeteer = require('D:/Apps/dou/node_modules/puppeteer');
const { spawn } = require('child_process');
const path = require('path');
const FF = process.env.FFMPEG || 'D:/Apps/welcome/node_modules/ffmpeg-static/ffmpeg.exe';
const [p, a2, a3, a4] = process.argv.slice(2);
(async () => {
  const b = await puppeteer.launch({ headless: 'new', args: ['--no-sandbox', '--allow-file-access-from-files'],
    executablePath: process.env.CHROME || 'C:/Users/rogerio.cardoso/.cache/puppeteer/chrome/win64-148.0.7778.97/chrome-win64/chrome.exe' });
  const pg = await b.newPage();
  await pg.setViewport({ width: 1080, height: 1920, deviceScaleFactor: 1 });
  const errs = []; pg.on('pageerror', e => errs.push(e.message));
  await pg.goto('file:///' + path.join(__dirname, 'reels.html').replace(/\\/g, '/') );
  await pg.waitForFunction(() => window.render && [...document.images].every(i => i.complete && i.naturalWidth));
  await pg.evaluate(() => document.fonts.ready);
  if (a2 === '--frames') {
    for (const t of a3.split(',')) { await pg.evaluate(t => render(t), +t); await pg.screenshot({ path: path.join(a4, `f-${t}.png`) }); }
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

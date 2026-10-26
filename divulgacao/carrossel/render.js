// Gera os slides do carrossel (1080×1350) a partir de carrossel.html + fotos em ../shots-img/
// uso: node render.js <pasta de saída> [analog]   (analog = só os 3 slides da série analógica)
const puppeteer = require('puppeteer');
const path = require('path');
const out = process.argv[2] || __dirname;
const analog = process.argv[3] === 'analog';
const names = analog ? [null, 'analog-01-f1', 'analog-02-ringer', 'analog-03-repeteco']
                     : ['00-capa', '01-grao', '02-caleido', '03-espiral', '04-fita', '05-chama', '06-navalha',
                        '07-f1', '08-ringer', '09-repeteco'];

(async () => {
  const b = await puppeteer.launch({ headless: 'new', args: ['--no-sandbox', '--allow-file-access-from-files'],
    executablePath: process.env.CHROME || undefined });
  const p = await b.newPage();
  await p.setViewport({ width: 1080, height: 1350, deviceScaleFactor: 1 });
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  const base = 'file:///' + path.join(__dirname, 'carrossel.html').replace(/\\/g, '/');
  for (let i = 0; i < names.length; i++) {
    if (!names[i]) continue;
    await p.goto(base + (analog ? '?s=analog' : '') + '#' + i);
    await p.reload();
    await p.waitForSelector('.foot');
    await p.waitForFunction(() => document.images.length && [...document.images].every(im => im.complete));
    const broken = await p.evaluate(() => [...document.images].filter(im => !im.naturalWidth).map(im => im.src));
    const over = await p.evaluate(() => { const f = document.querySelector('.foot').getBoundingClientRect();
      const els = [...document.querySelectorAll('.chips, .grid, .desc')]; return els.map(e => Math.round(e.getBoundingClientRect().bottom)).concat([Math.round(f.top)]); });
    await p.screenshot({ path: path.join(out, `insidestudiosp-carrossel-${names[i]}.png`) });
    console.log(names[i], 'img quebrada:', broken.length, 'bottoms/foot:', JSON.stringify(over));
  }
  console.log('erros:', errs);
  await b.close();
})();

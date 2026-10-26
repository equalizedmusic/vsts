const puppeteer = require('puppeteer');
(async () => {
  const b = await puppeteer.launch({ headless: 'new', args:['--no-sandbox'], executablePath: process.env.CHROME || undefined });
  const p = await b.newPage();
  await p.setViewport({ width: 1000, height: 580, deviceScaleFactor: 2 });
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  for (const n of ['f1', 'ringer', 'repeteco']) {
    await p.goto(require('url').pathToFileURL(require('path').join(__dirname, 'analogicos-layout-v1.html')).href + '#' + n); await p.reload();
    await new Promise(r => setTimeout(r, 500));
    await p.screenshot({ path: 'previa-' + n + '.png', clip: { x: 0, y: 0, width: 1000, height: 580 } });
    const over = await p.evaluate(() => [...document.querySelectorAll('.card, .desc, .ttl, .info')].filter(e => e.scrollWidth > e.clientWidth + 1 || e.scrollHeight > e.clientHeight + 1).map(e => e.className + ':' + e.textContent.slice(0, 30)));
    console.log(n, 'overflow:', JSON.stringify(over));
  }
  console.log('erros:', errs);
  await b.close();
})();

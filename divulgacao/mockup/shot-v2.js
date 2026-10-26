const puppeteer = require('puppeteer');
(async () => {
  const b = await puppeteer.launch({ headless: 'new', args:['--no-sandbox'], executablePath: process.env.CHROME || undefined });
  const p = await b.newPage();
  await p.setViewport({ width: 1060, height: 640, deviceScaleFactor: 2 });
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto(require('url').pathToFileURL(require('path').join(__dirname, 'grao-layout-v2.html')).href);
  await new Promise(r => setTimeout(r, 2500));
  await p.screenshot({ path: 'grao-v2-normal.png' });
  await p.evaluate(() => { document.querySelector('[data-t=reverse]').click(); document.querySelector('[data-t=freeze]').click(); });
  await new Promise(r => setTimeout(r, 1200));
  await p.screenshot({ path: 'grao-v2-congelado.png' });
  const m = await p.evaluate(() => [...document.querySelectorAll('.card')].map(c => { const r = c.getBoundingClientRect(); return [c.className, Math.round(r.width), Math.round(r.height)]; }));
  console.log(JSON.stringify(m));
  console.log('erros:', errs);
  await b.close();
})();

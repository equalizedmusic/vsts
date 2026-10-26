const puppeteer = require('puppeteer');
(async () => {
  const b = await puppeteer.launch({ headless: 'new', args:['--no-sandbox'], executablePath: process.env.CHROME || undefined });
  const p = await b.newPage();
  await p.setViewport({ width: 1080, height: 620, deviceScaleFactor: 2 });
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto(require('url').pathToFileURL(require('path').join(__dirname, 'nebula-layout.html')).href);
  await new Promise(r => setTimeout(r, 2500));
  await p.screenshot({ path: 'macro.png' });
  await p.evaluate(() => document.querySelector('[data-v=adv]').click());
  await new Promise(r => setTimeout(r, 300));
  await p.screenshot({ path: 'avancado.png' });
  console.log('erros:', errs);
  await b.close();
})();

// Renders every <section class="ad"> in ads.html to a 1080x1350 PNG in ../ads/.
// Usage: node marketing/facebook/src/render.cjs
const { chromium } = require("playwright");
const { pathToFileURL } = require("node:url");
const path = require("node:path");
const fs = require("node:fs");

const here = __dirname;
const outDir = path.join(here, "..", "ads");
fs.mkdirSync(outDir, { recursive: true });

(async () => {
const browser = await chromium.launch();
const page = await browser.newPage({ viewport: { width: 1200, height: 1400 }, deviceScaleFactor: 1 });
await page.goto(pathToFileURL(path.join(here, "ads.html")).href, { waitUntil: "networkidle" });
await page.evaluate(() => document.fonts.ready);

const ids = await page.$$eval("section.ad", els => els.map(e => e.id));
for (const id of ids) {
  const file = path.join(outDir, `Gradelux_FB_${id.replace(/^ad/, "")}.png`);
  await page.locator(`#${id}`).screenshot({ path: file });
  console.log("wrote", path.relative(process.cwd(), file));
}
await browser.close();
})();

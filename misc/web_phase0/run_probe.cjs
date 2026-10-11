// Phase 0 runner: serves an exported build and drives it in headless Chromium.
//   NODE_PATH=$(npm root -g) node run_probe.cjs build/nothreads [--threads] [--gesture=click|key]
// Prints every FMODPROBE console line, page errors, and a final verdict.
const { chromium } = require("playwright");
const http = require("http");
const fs = require("fs");
const path = require("path");

const root = path.resolve(process.argv[2] || "build/nothreads");
const threads = process.argv.includes("--threads");
const gesture = (process.argv.find((a) => a.startsWith("--gesture=")) || "--gesture=click").split("=")[1];
const types = { ".html": "text/html", ".js": "application/javascript", ".wasm": "application/wasm", ".pck": "application/octet-stream", ".png": "image/png" };

const server = http.createServer((req, res) => {
  const file = path.join(root, decodeURIComponent(req.url.split("?")[0]).replace(/^\/$/, "/index.html"));
  if (!file.startsWith(root) || !fs.existsSync(file)) { res.writeHead(404); return res.end(); }
  const headers = { "Content-Type": types[path.extname(file)] || "application/octet-stream" };
  if (threads) { // SharedArrayBuffer needs cross-origin isolation.
    headers["Cross-Origin-Opener-Policy"] = "same-origin";
    headers["Cross-Origin-Embedder-Policy"] = "require-corp";
  }
  res.writeHead(200, headers);
  fs.createReadStream(file).pipe(res);
});

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

(async () => {
  await new Promise((r) => server.listen(0, "127.0.0.1", r));
  const url = `http://127.0.0.1:${server.address().port}/index.html`;
  const browser = await chromium.launch({ args: ["--autoplay-policy=user-gesture-required"] });
  const page = await browser.newPage();
  const lines = [];
  const errors = [];
  let criticalSectionWarnings = 0;
  page.on("console", (m) => {
    const t = m.text();
    if (t.includes("CriticalSection")) { criticalSectionWarnings++; return; }
    if (t.includes("FMODPROBE") || m.type() === "error" || /error|abort|fmod/i.test(t)) { lines.push(t); console.log(`[console.${m.type()}] ${t}`); }
  });
  page.on("pageerror", (e) => { errors.push(String(e)); console.log(`[pageerror] ${e}`); });

  await page.goto(url);
  const lastStatus = () => [...lines].reverse().find((l) => l.includes("FMODPROBE STATUS")) || "";
  const rmsOf = (l) => parseFloat((l.match(/ rms=([0-9.]+)/) || [])[1] || "0");

  for (let i = 0; i < 60 && !lastStatus(); i++) await sleep(500);
  await sleep(3000);
  const before = lastStatus();
  const audioCtxBefore = await page.evaluate(() => window.__fmodProbeCtxStates || null).catch(() => null);
  console.log("== before gesture:", before || "(no status)");

  if (gesture === "key") {
    await page.locator("canvas").focus();
    await page.keyboard.press("Space");
  } else {
    await page.mouse.click(100, 100);
  }
  await sleep(5000);
  const after = lastStatus();
  console.log("== after gesture:", after || "(no status)");

  const verdict = {
    extension_loaded: !lines.some((l) => l.includes("extension not loaded")),
    gesture,
    probe_ready: lines.some((l) => l.includes("FMODPROBE READY")),
    probe_error: lines.find((l) => l.includes("FMODPROBE ERROR")) || null,
    rms_before_gesture: rmsOf(before),
    rms_after_gesture: rmsOf(after),
    page_errors: errors,
    fmod_critical_section_warnings: criticalSectionWarnings,
  };
  console.log("== VERDICT " + JSON.stringify(verdict, null, 2));
  await browser.close();
  server.close();
})().catch((e) => { console.error(e); process.exit(1); });

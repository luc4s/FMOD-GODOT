// Phase 0 runtime check (questions 2 and 4): load the exported spike in Chromium, record the
// "SPIKE <json>" lines printed by main.gd before and after a user gesture, and summarize.
//
// Usage: NODE_PATH=$(npm root -g) node browser_test.cjs <url> <out.json>
const { chromium } = require("playwright");
const fs = require("fs");

const [url, outPath] = process.argv.slice(2);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

(async () => {
    const browser = await chromium.launch({
        // Full Chromium in new headless mode: the old headless shell ignores the autoplay policy.
        channel: "chromium",
        // Behave like a real browser: audio may only start after a user gesture.
        args: ["--autoplay-policy=user-gesture-required"],
    });
    const page = await browser.newPage();
    // Track every AudioContext the page creates (Godot's and FMOD's) to report their state.
    await page.addInitScript(() => {
        window.__ctxs = [];
        for (const name of ["AudioContext", "webkitAudioContext"]) {
            const Orig = window[name];
            if (!Orig) continue;
            window[name] = class extends Orig {
                constructor(...a) {
                    super(...a);
                    window.__ctxs.push(this);
                }
            };
        }
    });

    const log = [];
    const spike = [];
    let phase = "load";
    page.on("console", (m) => {
        const text = m.text();
        log.push(`[${phase}] ${m.type()}: ${text}`);
        const i = text.indexOf("SPIKE ");
        if (i !== -1) {
            try {
                spike.push({ phase, ...JSON.parse(text.slice(i + 6)) });
            } catch (_) {}
        }
    });
    page.on("pageerror", (e) => log.push(`[${phase}] pageerror: ${e.message}`));

    await page.goto(url);
    const deadline = Date.now() + 60000;
    while (Date.now() < deadline && !spike.some((s) => s.step === "play" || s.error)) await sleep(250);
    await sleep(3000);
    const ctxBefore = await page.evaluate(() => window.__ctxs.map((c) => c.state));

    phase = "after_click";
    const box = await page.locator("canvas").boundingBox().catch(() => null);
    if (box) await page.mouse.click(box.x + box.width / 2, box.y + box.height / 2);
    else await page.mouse.click(10, 10);
    await sleep(4000);
    const ctxAfter = await page.evaluate(() => window.__ctxs.map((c) => c.state));
    await browser.close();

    const statuses = (p) => spike.filter((s) => s.step === "status" && s.phase === p).map((s) => s.status);
    const maxRms = (p) => Math.max(0, ...statuses(p).map((s) => s.master_rms || 0));
    const errors = spike.filter((s) => s.error);
    const summary = {
        extension_loaded: spike.some((s) => s.step === "extension_loaded" && !s.error),
        steps: spike.filter((s) => s.step !== "status").map((s) => ({ step: s.step, phase: s.phase, error: s.error })),
        errors,
        audio_contexts: { before_click: ctxBefore, after_click: ctxAfter },
        max_master_rms: { before_click: maxRms("load"), after_click: maxRms("after_click") },
        last_status: statuses("after_click").slice(-1)[0] || statuses("load").slice(-1)[0] || null,
        console_errors: log
            .filter((l) => !l.includes("SPIKE "))
            .filter((l) => /error|abort|undefined symbol|missing function|LinkError/i.test(l))
            .slice(0, 40),
    };
    summary.verdict = {
        q2_runtime_links: summary.extension_loaded && !errors.some((e) => e.step === "init"),
        q4_sound_after_gesture: summary.max_master_rms.after_click > 0,
        // main.gd suspends the mixer until input (headless Chromium auto-allows audio, so the real
        // autoplay block can only be checked manually in a normal browser).
        silent_while_suspended: summary.max_master_rms.before_click === 0,
    };
    fs.writeFileSync(outPath, JSON.stringify({ summary, console: log }, null, 2));
    console.log(JSON.stringify(summary, null, 2));
})().catch((e) => {
    console.error(e);
    process.exit(2);
});

import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { pathToFileURL } from "node:url";

const repository = resolve(import.meta.dirname, "..");
const executable = resolve(repository, "build/codex-debug/terminal-music-player.exe");
const output = resolve(repository, "docs/images/terminal-music-player-actual.png");
const htmlFile = resolve(repository, "build/ui-snapshot.html");
const edge = "C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe";

if (!existsSync(executable)) {
  throw new Error(`Build the Debug executable first: ${executable}`);
}
if (!existsSync(edge)) {
  throw new Error(`Microsoft Edge was not found: ${edge}`);
}

const environment = {
  ...process.env,
  PATH: `C:/msys64/ucrt64/bin;${process.env.PATH ?? ""}`,
};
const captured = spawnSync(
  executable,
  ["--snapshot", "--data-dir", resolve(repository, "Data")],
  { cwd: repository, encoding: "utf8", env: environment },
);
if (captured.status !== 0) {
  throw new Error(`Snapshot command failed (${captured.status}): ${captured.stderr}`);
}

const escapeHtml = (value) => value
  .replaceAll("&", "&amp;")
  .replaceAll("<", "&lt;")
  .replaceAll(">", "&gt;");

let bold = false;
let dim = false;
let inverse = false;
let html = "";
let offset = 0;
const sgr = /\x1b\[([0-9;]*)m/g;
const append = (text) => {
  if (!text) return;
  const classes = [bold ? "bold" : "", dim ? "dim" : "", inverse ? "inverse" : ""]
    .filter(Boolean).join(" ");
  html += classes ? `<span class="${classes}">${escapeHtml(text)}</span>` : escapeHtml(text);
};
for (const match of captured.stdout.matchAll(sgr)) {
  append(captured.stdout.slice(offset, match.index));
  const codes = (match[1] || "0").split(";").map(Number);
  for (const code of codes) {
    if (code === 0) { bold = false; dim = false; inverse = false; }
    else if (code === 1) bold = true;
    else if (code === 2) dim = true;
    else if (code === 7) inverse = true;
    else if (code === 22) { bold = false; dim = false; }
    else if (code === 27) inverse = false;
  }
  offset = match.index + match[0].length;
}
append(captured.stdout.slice(offset));

const document = `<!doctype html>
<meta charset="utf-8">
<style>
  html, body { margin: 0; width: 100%; height: 100%; overflow: hidden; background: #080b12; }
  body { display: flex; align-items: flex-start; justify-content: center; }
  pre {
    box-sizing: border-box; margin: 0; padding: 18px 22px; width: 100%; min-height: 100%;
    background: linear-gradient(135deg, #0b1020 0%, #10101c 60%, #111827 100%);
    color: #dbe7f3; font: 13px/1.1 "Cascadia Mono", "Consolas", monospace;
    white-space: pre; letter-spacing: 0;
  }
  .bold { color: #74e4e8; font-weight: 700; }
  .dim { color: #7f8ea3; }
  .inverse { color: #071018; background: #75e6c3; }
</style>
<pre>${html.replaceAll("\r", "")}</pre>`;

mkdirSync(dirname(output), { recursive: true });
writeFileSync(htmlFile, document, "utf8");
const rendered = spawnSync(edge, [
  "--headless=new",
  "--disable-gpu",
  "--hide-scrollbars",
  "--window-size=1220,680",
  `--screenshot=${output}`,
  pathToFileURL(htmlFile).href,
], { cwd: repository, encoding: "utf8" });
if (rendered.status !== 0 || !existsSync(output)) {
  throw new Error(`Edge screenshot failed (${rendered.status}): ${rendered.stderr}`);
}
console.log(`Captured actual application UI: ${output}`);

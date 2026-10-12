#!/usr/bin/env node
import { spawn } from "node:child_process";
import { resolve } from "node:path";
import { fileURLToPath } from "node:url";
import {assertReactFrontendEnvironment} from "../compat/frontend-environment.mjs";
import { assertSupportedNode, ensureNodeDependencies } from "./lib/node-environment.mjs";
import { run } from "./lib/process.mjs";
import { parseOptions } from "./lib/cli.mjs";

const projectRoot = resolve(fileURLToPath(new URL("..", import.meta.url)));


function openBrowser(url) {
  let command;
  let args;
  if (process.platform === "win32") {
    command = "cmd"; args = ["/c", "start", "", url];
  } else if (process.platform === "darwin") {
    command = "open"; args = [url];
  } else {
    command = "xdg-open"; args = [url];
  }
  try {
    const child = spawn(command, args, { detached: true, stdio: "ignore", windowsHide: true });
    child.once("error", () => {});
    child.unref();
  } catch {
    // Browser opening is a convenience only; the printed URL is authoritative.
  }
}

const args = parseOptions(process.argv.slice(2), {"values": ["root", "music", "python", "site-url", "bind", "port"], "flags": ["build-only", "no-open"]});
const hostRoot = resolve(String(args.root || projectRoot));
const music = String(args.music || "midi,ogg");
const python = String(args.python || process.env.PYTHON || (process.platform === "win32" ? "python" : "python3"));
const buildOnly = !!args["build-only"];
const noOpen = !!args["no-open"];
const bind = String(args.bind || "127.0.0.1");
const port = Number(args.port ?? "8130");
if (!Number.isInteger(port) || port < 1 || port > 65535) throw new Error(`invalid --port=${args.port}`);

assertSupportedNode();
const siteUrl = args["site-url"];
assertReactFrontendEnvironment();
console.log("[Build] Validating self-host inputs and configuration");
await ensureNodeDependencies(projectRoot);
const { inspectHostWorkspace } = await import("../lib/host-workspace.mjs");
await inspectHostWorkspace(hostRoot, { music });
const { resolveHostFrontend } = await import("./lib/site-builder.mjs");
await resolveHostFrontend({ projectRoot, siteUrl, serve: !buildOnly });
const { buildHostedSite } = await import("./lib/site-builder.mjs");
const { layout } = await buildHostedSite({ projectRoot, hostRoot, music, python, siteUrl });
await run(process.execPath, [
  resolve(projectRoot, "scripts", "inspect-host.mjs"),
  `--root=${hostRoot}`, `--music=${music}`, `--python=${python}`, "--post-build=1",
], { cwd: projectRoot });

if (buildOnly) process.exit(0);
if (bind === "0.0.0.0" || bind === "::") {
  console.warn("[Host] The built-in HTTP server is intended for trusted local/LAN use; use your normal HTTPS web server/CDN for public hosting.");
}
const displayHost = ["0.0.0.0", "::"].includes(bind) ? "127.0.0.1" : bind;
const url = `http://${displayHost}:${port}/`;
console.log(`[Host] Serving verified site: ${url}`);
if (!noOpen && ["127.0.0.1", "localhost", "::1"].includes(bind)) openBrowser(url);
await run(process.execPath, [resolve(projectRoot, "scripts", "serve-static.mjs"), layout.site, String(port)], {
  cwd: projectRoot,
  env: { EAGLER_TOUHOU_HOST: bind },
});

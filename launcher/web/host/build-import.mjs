#!/usr/bin/env node
import { resolve } from "node:path";
import { fileURLToPath } from "node:url";
import {assertReactFrontendEnvironment} from "../compat/frontend-environment.mjs";
import { assertSupportedNode, ensureNodeDependencies } from "./lib/node-environment.mjs";
import { run } from "./lib/process.mjs";
import { parseOptions } from "./lib/cli.mjs";

const projectRoot = resolve(fileURLToPath(new URL("..", import.meta.url)));


const args = parseOptions(process.argv.slice(2), {"values": ["root", "music", "python", "site-url"], "flags": ["test-build", "rebuild-hosted-base"]});
const hostRoot = resolve(String(args.root || projectRoot));
const music = String(args.music || "midi,ogg");
const python = String(args.python || process.env.PYTHON || (process.platform === "win32" ? "python" : "python3"));
const testBuild = !!args["test-build"];
const rebuildHostedBase = !!args["rebuild-hosted-base"];

assertSupportedNode();
const siteUrl = args["site-url"];
assertReactFrontendEnvironment();
console.log("[Import] Validating self-host inputs and configuration");
await ensureNodeDependencies(projectRoot);
const { inspectHostWorkspace } = await import("../lib/host-workspace.mjs");
await inspectHostWorkspace(hostRoot, { music });
const { resolveHostFrontend } = await import("./lib/site-builder.mjs");
await resolveHostFrontend({ projectRoot, siteUrl });
const { buildImportArtifacts } = await import("./lib/site-builder.mjs");
await buildImportArtifacts({ projectRoot, hostRoot, music, python, rebuildHostedBase, testBuild, siteUrl });
await run(process.execPath, [
  resolve(projectRoot, "scripts", "inspect-host.mjs"),
  `--root=${hostRoot}`, `--music=${music}`, `--python=${python}`, "--post-import=1",
], { cwd: projectRoot });

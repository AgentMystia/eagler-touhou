import { spawn } from "node:child_process";
import { existsSync } from "node:fs";
import { dirname, resolve } from "node:path";

export function run(command, args = [], { cwd, env, capture = false, allowFailure = false } = {}) {
  return new Promise((resolve, reject) => {
    const child = spawn(command, args, {
      cwd,
      env: env ? { ...process.env, ...env } : process.env,
      windowsHide: true,
      stdio: capture ? ["ignore", "pipe", "pipe"] : "inherit",
    });
    const stdout = [];
    const stderr = [];
    if (capture) {
      child.stdout.on("data", chunk => stdout.push(chunk));
      child.stderr.on("data", chunk => stderr.push(chunk));
    }
    child.once("error", error => reject(error));
    child.once("close", code => {
      const result = {
        code: code ?? 1,
        stdout: capture ? Buffer.concat(stdout).toString("utf8").trim() : "",
        stderr: capture ? Buffer.concat(stderr).toString("utf8").trim() : "",
      };
      if (result.code === 0 || allowFailure) {
        resolve(result);
        return;
      }
      const detail = result.stderr || result.stdout;
      reject(new Error(`${command} exited with code ${result.code}${detail ? `: ${detail}` : ""}`));
    });
  });
}

export async function commandExists(command) {
  const probe = process.platform === "win32"
    ? await run("where", [command], { capture: true, allowFailure: true })
    : await run("sh", ["-c", `command -v -- "$1"`, "sh", command], { capture: true, allowFailure: true });
  return probe.code === 0 && !!probe.stdout;
}

/** npm's Windows .cmd shim cannot be spawned as a native executable. */
export async function runNpm(args, options) {
  const configured = process.env.npm_execpath;
  if (configured && existsSync(configured)) return run(process.execPath, [configured, ...args], options);
  if (process.platform !== 'win32') return run('npm', args, options);
  const found = await run('where.exe', ['npm.cmd'], { capture: true, allowFailure: true });
  for (const shim of found.stdout.split(/\r?\n/).filter(Boolean)) {
    const cli = resolve(dirname(shim), 'node_modules/npm/bin/npm-cli.js');
    if (existsSync(cli)) return run(process.execPath, [cli, ...args], options);
  }
  throw new Error('npm CLI is unavailable; install Node.js with npm or run this entry through npm');
}

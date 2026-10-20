/** Replay audio activation contract (issue #99). The multiplayer Replay
 * workspace reuses the permanent game iframe, and every transport control is
 * portalled into the launcher document. The Runtime document therefore never
 * sees the gesture that unlocks its own AudioContext, so playback starts
 * silent under the browser autoplay policy. This contract pins the two fix
 * layers: the optional activate-audio command forwarded with playback clicks,
 * and the in-document tap gate a Runtime shows while its context is still
 * suspended. */
import fs from "node:fs";
import path from "node:path";

const root = path.resolve(import.meta.dirname, "..", "..");
const read = rel => fs.readFileSync(path.join(root, rel), "utf8");
const requireText = (source, needle, label) => {
  if (!source.includes(needle)) throw new Error(`missing ${label}: ${needle}`);
};

const protocol = read("launcher/src/contracts/runtime-protocol.mts");
requireText(protocol, '"activate-audio",', "optional command vocabulary");
requireText(protocol, '"activate-audio": Object.freeze({ phase: "live", response: "required", requirement: "conditional"',
  "activate-audio behavior entry");
requireText(protocol, '"activate-audio": Record<string, never>;', "activate-audio payload");

const runtime = read("launcher/app/services/runtime.ts");
requireText(runtime, "'playback' | 'activate-audio' | 'spectator-playback'", "request command type");
requireText(runtime, "['playback', 'activate-audio', 'spectator-playback',", "request whitelist");

const session = read("launcher/app/session/browser-session.ts");
requireText(session, "await activateRuntimeAudio(owner);", "watch channel forwards activation");
requireText(session, "await owner.send('activate-audio', {})", "watch sends activate-audio");
requireText(session, "activateRuntimeAudio: () => activateRuntimeAudio(runtime),", "session activation surface");

const workspace = read("launcher/app/components/player/ReplayWorkspace.tsx");
requireText(workspace, "if (action === 'play' || action === 'seek') await session.activateRuntimeAudio();",
  "play/seek clicks forward activation");

// Every shell that owns the local replay transport must answer the optional
// command and fall back to a real in-document tap when policy still refuses.
const shells = [
  "ports/th08/web/sdl-runtime/shell.mjs",
  "ports/th08-mp/web/sdl-runtime/shell.mjs",
  "ports/th09/web/sdl-runtime/managed.mjs",
  "ports/th10/web/sdl-runtime/shell.mjs",
  "ports/th10-mp/web/sdl-runtime/shell.mjs",
  "ports/th11/web/sdl-runtime/shell.mjs",
  "ports/th11-mp/web/sdl-runtime/shell.mjs",
  "ports/th15/web/sdl-runtime/shell.mjs",
  "ports/th15-mp/web/sdl-runtime/shell.mjs",
];
for (const shell of shells) {
  const source = read(shell);
  requireText(source, "case 'activate-audio'", `${shell}: command dispatch`);
  requireText(source, "async function activateAudio(", `${shell}: activation owner`);
  requireText(source, "className='eagler-audio-gate'", `${shell}: in-document gate`);
  requireText(source, "点击开始播放", `${shell}: gate label`);
  requireText(source, "context.onstatechange=", `${shell}: gate dismisses itself once running`);
}
// The SDL shells that disconnect their stream on suspend must reuse their
// foreground resume owner; the direct-context shells must resume the context.
for (const shell of shells) {
  const source = read(shell);
  if (shell.includes("th09") || shell.includes("th15")) requireText(source, "await context.resume();", `${shell}: context resume retry`);
  else requireText(source, "await resumeForegroundAudio();", `${shell}: foreground resume retry`);
}

console.log(JSON.stringify({ replayAudioActivationContract: "PASS", shells: shells.length }));

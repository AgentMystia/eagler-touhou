/** Issue #102: TH15 multiplayer stuck keys. A key release the launcher drops
 * leaves the Runtime's held bit stranded until a stage transition clears all
 * inputs. These cases pin the ownership rules that keep releases flowing. */
import test, {before, after} from 'node:test';
import assert from 'node:assert/strict';
import {readFile, mkdir, mkdtemp, writeFile, rm} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {resolve} from 'node:path';
import {fileURLToPath, pathToFileURL} from 'node:url';
import {buildOriginalComponentFixture} from './support/original-component-fixture.mjs';
import {installMountedDom} from './react-main/mounted-dom-environment.mjs';

const project = fileURLToPath(new URL('../', import.meta.url));
const sources = ['app/services/player-keyboard.ts', 'src/launcher/hosted-keyboard.mts', 'src/launcher/touch-runtime-protocol.mts'];
const fingerprint = async path => createHash('sha256').update(await readFile(resolve(project, path))).digest('hex');
let fixture, adapter, work, env;

before(async () => {
  await mkdir(resolve(project, '.cache'), {recursive: true});
  work = await mkdtemp(resolve(project, '.cache/keyboard-keyup-'));
  const beforeHashes = await Promise.all(sources.map(fingerprint));
  fixture = await buildOriginalComponentFixture('keyboard-ownership');
  assert.deepEqual(await Promise.all(sources.map(fingerprint)), beforeHashes, 'fixture compilation must not change its input owners');
  const file = resolve(work, 'fixture.mjs');
  await writeFile(file, fixture.module);
  env = installMountedDom();
  adapter = await import(pathToFileURL(file).href);
});
after(async () => {try {if (env) {assert.deepEqual(env.errors, []); env.close();}} finally {if (work) await rm(work, {recursive: true, force: true});}});

function installCase() {
  const {document, window} = env;
  document.body.innerHTML = '<div id="player" class="open"><iframe id="runtime"></iframe></div>';
  const player = document.getElementById('player'), frame = document.getElementById('runtime');
  const state = {launched: true, game: 'th15'};
  const context = () => ({target: frame.contentWindow, targetOrigin: window.location.origin,
    protocol: 'eagler-touhou/1', game: 'th15', epoch: 1, launched: state.launched, ready: state.launched, spectator: false});
  // Simplified mirror of the th15-mp shell bridge: forwards canonical keyboard
  // commands into the launcher's mapping port and records both layers.
  const submitted = [], delivered = [];
  frame.contentWindow.__eaglerKeyboardInputBridge = {
    schema: 'eagler-touhou/keyboard-input/1', protocol: 'eagler-touhou/1', game: 'th15', epoch: 1, origin: window.location.origin,
    submit(message) {
      if (message.command === 'keyboard-clear') {frame.contentWindow.__eaglerKeyboardMapping?.clear(); return true;}
      if (message.command !== 'keyboard' || typeof message.down !== 'boolean') return false;
      submitted.push({command: 'keyboard', code: message.code, down: message.down});
      const port = frame.contentWindow.__eaglerKeyboardMapping;
      if (!port) return false;
      const events = port.map(message, message.down, 'physical', 'standard');
      if (events === null) return false;
      delivered.push(...events);
      return true;
    },
  };
  const owner = adapter.installKeyboardOwnershipFixture({state, frame, player, touchRuntimeMessageContext: context,
    releaseHeldTouchFire() {}, touchFunctionOwner: adapter.createFunctionKeyOwner(() => {})});
  const key = (type, changes = {}) => document.body.dispatchEvent(new window.KeyboardEvent(type,
    {bubbles: true, cancelable: true, ...changes}));
  return {player, frame, state, owner, key, submitted, delivered};
}

test('press and release both cross the bridge in order', () => {
  const c = installCase();
  try {
    c.key('keydown', {code: 'ArrowUp', key: 'ArrowUp', keyCode: 38, location: 0});
    c.key('keyup', {code: 'ArrowUp', key: 'ArrowUp', keyCode: 38, location: 0});
    assert.deepEqual(c.submitted, [{command: 'keyboard', code: 'ArrowUp', down: true}, {command: 'keyboard', code: 'ArrowUp', down: false}]);
    assert.deepEqual(c.delivered.filter(e => e.code === 'ArrowUp').map(e => [e.code, e.down]), [['ArrowUp', true], ['ArrowUp', false]]);
  } finally {c.owner.dispose();}
});

test('an unrecognizable release is forwarded instead of dropped', () => {
  const c = installCase();
  try {
    // F13 is outside every mapping table: its press is rejected, and before the
    // fix its release was dropped with it. Releases must always reach Runtime.
    c.key('keydown', {code: 'F13', key: 'F13', keyCode: 0, location: 0});
    assert.deepEqual(c.submitted, [], 'unmapped press stays rejected');
    c.key('keyup', {code: 'F13', key: '', keyCode: 0, location: 0});
    assert.deepEqual(c.submitted, [{command: 'keyboard', code: 'F13', down: false}]);
  } finally {c.owner.dispose();}
});

test('modifier-chord releases are forwarded; only presses are gated', () => {
  const c = installCase();
  try {
    c.key('keydown', {code: 'ArrowUp', key: 'ArrowUp', keyCode: 38, location: 0});
    c.key('keydown', {code: 'AltLeft', key: 'Alt', keyCode: 18, location: 1});
    c.key('keyup', {code: 'ArrowUp', key: 'ArrowUp', keyCode: 38, location: 0, altKey: true});
    c.key('keyup', {code: 'AltLeft', key: 'Alt', keyCode: 18, location: 1});
    assert.deepEqual(c.submitted.filter(m => m.code === 'ArrowUp'), [{command: 'keyboard', code: 'ArrowUp', down: true}, {command: 'keyboard', code: 'ArrowUp', down: false}]);
    assert.deepEqual(c.delivered.filter(e => e.code === 'ArrowUp').map(e => e.down), [true, false]);
  } finally {c.owner.dispose();}
});

test('the mapping port retires a release while the player surface is closed', () => {
  const c = installCase();
  try {
    c.key('keydown', {code: 'ArrowUp', key: 'ArrowUp', keyCode: 38, location: 0});
    const port = c.frame.contentWindow.__eaglerKeyboardMapping;
    assert.ok(port, 'mapping port installed');
    c.player.classList.remove('open');
    // A release arriving once the surface closed (async transport) must not be
    // silently swallowed: it still has to retire the held mapping.
    const transitions = port.map({code: 'ArrowUp', key: 'ArrowUp', keyCode: 38, location: 0}, false, 'physical', 'standard');
    assert.deepEqual(transitions.filter(e => e.code === 'ArrowUp').map(e => [e.code, e.down]), [['ArrowUp', false]]);
    // The press side keeps its original silent rejection while closed.
    assert.deepEqual(port.map({code: 'ArrowDown', key: 'ArrowDown', keyCode: 40, location: 0}, true, 'physical', 'standard'), []);
  } finally {c.owner.dispose();}
});

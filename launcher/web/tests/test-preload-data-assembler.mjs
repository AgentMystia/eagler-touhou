/** Workspace integration: reconstruct current TH06/TH07 Emscripten DATA from
 * their declared source files and require exact bytes against known builds. */
import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { resolve } from "node:path";
import { assemblePreloadData } from "../lib/preload-data-assembler.mjs";

const workspace = resolve(new URL("../..", import.meta.url).pathname.replace(/^\/(?:([A-Za-z]:))/, "$1"));
for (const test of [
  { game: "th06", build: "th06-eagler/build-web-eagler-thprac-test", assets: "th06-eagler/assets" },
  { game: "th07", build: "th07-eagler/build-web-eagler-thprac", assets: "th07-eagler/assets" },
]) {
  const runtime = resolve(workspace, test.build, `${test.game}.js`);
  const expected = await readFile(resolve(workspace, test.build, `${test.game}.data`));
  const { data, layout } = await assemblePreloadData({
    game: test.game,
    runtimeScript: runtime,
    sourceDirectory: resolve(workspace, test.assets),
  });
  assert.deepEqual(data, expected, `${test.game}: assembled DATA must exactly match the Emscripten build artifact`);
  assert.equal(data.length, layout.bytes);
}
console.log("Preload DATA assembly: PASS (TH06/TH07 exact bytes)");

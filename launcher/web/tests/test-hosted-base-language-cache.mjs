// Module regression: a structurally valid old hosted site must not suppress
// preparation of newly enabled default language packs. No game data is used.
import assert from "node:assert/strict";
import { PRODUCT_GAMES } from "../lib/contracts/product-catalog.mjs";
import { hostedLanguagesComplete } from "../host/lib/site-builder.mjs";

const games = Object.keys(PRODUCT_GAMES);
const manifest = { games: Object.fromEntries(games.map(game => [game, {
  languageOptions: ["ja", "lang_zh-hans", "lang_en"].map(id => ({
    id, pack: id === "ja" ? null : { url: `games/${game}/language/${id}.zip` },
  })),
}])) };
assert.equal(hostedLanguagesComplete(manifest), true);
const full = manifest.games.th09.languageOptions;
manifest.games.th09.languageOptions = full.slice(0, 1);
assert.equal(hostedLanguagesComplete(manifest), false, "Japanese-only TH09 cache must rebuild");
manifest.games.th09.languageOptions = full.map(option => ({ ...option, pack: null }));
assert.equal(hostedLanguagesComplete(manifest), false, "labels without packs must rebuild");
manifest.games.th09.languageOptions = full;
assert.equal(hostedLanguagesComplete(manifest), true);
console.log(JSON.stringify({ hostedLanguageCache: "validated", th09MissingPacks: "rebuild" }));

manifest.games.th09.languageOptions = [{id:'ja'}];
assert.equal(hostedLanguagesComplete(manifest, ['th09'], {games:{th09:{languages:['ja']}}}), true);
assert.equal(hostedLanguagesComplete(manifest, ['th09'], {games:{th09:{languages:['ja','lang_en']}}}), false);

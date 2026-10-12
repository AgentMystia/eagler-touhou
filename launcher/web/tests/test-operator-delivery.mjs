import assert from "node:assert/strict";

import {
  assertHostedSiteMatchesOperatorProfile,
  validateOperatorProfile,
} from "../tools/maintainer/operator-delivery.mjs";

const profile = validateOperatorProfile({
  schema: 'eagler-touhou/internal-operator-profile/1',
  resourceMode: 'hosted', requireGameDataFallback: true,
  originMigration: 'http-to-https', netplayRelay: 'absent',
  games: {
    th06: { languages: ['ja', 'lang_zh-hans', 'lang_en'], music: ['midi', 'ogg'], thprac: true, runtimeVariants: ['normal', 'multiplayer'] },
    th15: { languages: ['ja'], music: ['ogg'], thprac: true, runtimeVariants: ['normal'] },
  },
});
const manifest = {
  schema: "eagler-touhou/site-manifest/1",
  shared: {
    resourceMode: "hosted",
    gameDataFallback: { url: "https://example.invalid/games" },
    originMigration: { mode: "http-to-https" },
  },
  games: Object.fromEntries(Object.entries(profile.games).map(([game, expected]) => [game, {
    features: { thprac: expected.thprac },
    music: Object.fromEntries(expected.music.map(mode => [mode, { files: [] }])),
    runtime: `runtime/${game}/${game}.html`,
    ...(expected.runtimeVariants.includes("multiplayer")
      ? { multiplayerRuntime: `runtime/${game}/multiplayer/${game}.html` }
      : {}),
    languageOptions: expected.languages.map(id => ({ id, pack: id === "ja" ? null : { url: `${id}.zip` } })),
    package: { descriptor: `${game}.package.json` },
  }])),
};

// The manifest keeps a disabled MIDI declaration for titles without synthesis.
manifest.games.th15.music.midi = { supported: false, files: [] };
assert.equal(assertHostedSiteMatchesOperatorProfile(manifest, profile), profile);
const reordered = structuredClone(manifest);
reordered.games.th06.music = { ogg: { files: [] }, midi: { files: [] } };
assert.equal(assertHostedSiteMatchesOperatorProfile(reordered, profile), profile);
const unsupported = structuredClone(profile);
unsupported.games.th15.music = ['midi'];
assert.throws(() => validateOperatorProfile(unsupported), /invalid internal Operator game profile: th15/);


const missingLanguage = structuredClone(manifest);
missingLanguage.games.th06.languageOptions = missingLanguage.games.th06.languageOptions.filter(entry => entry.id !== "lang_en");
assert.throws(
  () => assertHostedSiteMatchesOperatorProfile(missingLanguage, profile),
  /th06: Operator language profile mismatch/,
);

const configuredRelay = structuredClone(manifest);
configuredRelay.shared.netplayRelay = "wss://example.invalid/eagler-netplay/";
assert.throws(
  () => assertHostedSiteMatchesOperatorProfile(configuredRelay, profile),
  /requires Netplay Relay to be absent/,
);

console.log("Internal Operator delivery profile: PASS");

import {
  React,
  acceptHeldHost,
  api,
  assert,
  back,
  capturePreparationTimers,
  click,
  commands,
  currentUrl,
  env,
  importPackageFile,
  mountBrowser,
  node,
  playerOpen,
  selectImport,
  siteManifest,
  start,
  subsetHost,
  test,
  tick,
  until,
  visibleOptions,
} from './browser-session-fixture.mjs';

for (const exitStatus of ['success', 'error'])
  test(`actual BrowserLauncher: original warning, fullscreen, ready/configure/ACK launch and native ${exitStatus} Exit preserve one frame`, async () => {
    const f = await mountBrowser(),
      frame = node('#gameFrame'),
      before = currentUrl();
    await start();
    await until(() => commands().includes('launch'), 'actual Runtime issues launch');
    assert.equal(playerOpen(), true);
    assert.equal(node('#gameFrame'), frame);
    assert.equal(env.document.querySelectorAll('iframe').length, 1);
    assert.equal(currentUrl(), before, 'Start reuses the selected product entry');
    assert.ok(f.order.indexOf('fullscreen-enter') < f.order.indexOf('native-navigation'));
    assert.ok(f.order.indexOf('configure') < f.order.indexOf('launch'));
    const nativeDocument = f.native.document,
      source = f.native.location.href,
      locationKey = f.router.state.location.key,
      commandsBeforeEdit = commands();
    await React.act(async () => {
      node('#touchLayoutEdit').click();
    });
    assert.equal(
      node('#toastText').textContent,
      'Exit the running game before editing the touch layout',
      'main app7187/8711 and i18n317 keep the launched guard at the real Settings action',
    );
    assert.equal(
      node('#status').textContent,
      'Error: Exit the running game before editing the touch layout',
    );
    assert.equal(node('#toast').classList.contains('show'), true);
    assert.equal(env.document.querySelector('#touchLayoutEditor'), null);
    assert.equal(playerOpen(), true);
    assert.equal(currentUrl(), before);
    assert.equal(f.router.state.location.key, locationKey);
    assert.equal(f.native.document, nativeDocument);
    assert.equal(f.native.location.href, source);
    assert.deepEqual(commands(), commandsBeforeEdit);
    await React.act(async () => {
      f.native.emit('exit', {
        status: exitStatus,
      });
    });
    await until(
      () => !playerOpen() && currentUrl() === before,
      'native Exit retires the completed engine and returns to its options',
    );
    assert.equal(
      commands().includes('sync'),
      false,
      'native Exit has no surviving sync receiver (main app5493–5522)',
    );
    assert.equal(node('#gameFrame'), frame);
    assert.equal(
      node('#decisionDialog').open,
      false,
      'main app5502–5506 skips sync and save decisions for either native exit status',
    );
    assert.equal(f.navigations.at(-1), 'about:blank');
  });

test('actual BrowserLauncher: Back during native preparation cancels before configure or launch', async () => {
  const f = await mountBrowser({
    autoReady: false,
  });
  await start();
  await until(
    () => f.navigations.some((url) => url !== 'about:blank'),
    'actual native preparation started',
  );
  await back();
  await until(() => !playerOpen(), 'Back closes unlaunched Player');
  await React.act(async () => {
    f.native.emit('ready');
  });
  await tick();
  assert.equal(commands().includes('configure'), false);
  assert.equal(commands().includes('launch'), false);
  assert.equal(f.native.location.href, 'about:blank');
  assert.equal(currentUrl(), '/?game=th06&keep=a%20b');
});

test('actual BrowserLauncher: warning cancel has no fullscreen/native side effect, repeated Start owns one launch', async () => {
  const f = await mountBrowser();
  await click('#launch');
  await click('#decisionCancel');
  assert.equal(playerOpen(), false);
  assert.equal(f.navigations.length, 0);
  assert.equal(f.order.includes('fullscreen-enter'), false);
  assert.equal(node('#launch').disabled, false);
  await React.act(async () => {
    node('#launch').click();
    node('#launch').click();
  });
  await tick();
  assert.equal(node('#decisionDialog').open, true);
  await click('#decisionConfirm');
  await until(() => commands().includes('launch'), 'one confirmed Start launches');
  assert.equal(commands().filter((command) => command === 'launch').length, 1);
  await back();
  await until(() => !playerOpen(), 'cleanup through actual close');
});

test('actual BrowserLauncher: mounted player input is gated by launch and removed after native Exit', async () => {
  const f = await mountBrowser();
  const key = (type) =>
    env.window.dispatchEvent(
      new env.window.KeyboardEvent(type, {
        code: 'KeyZ',
        key: 'z',
        keyCode: 90,
        bubbles: true,
        cancelable: true,
      }),
    );
  await React.act(async () => {
    key('keydown');
    key('keyup');
  });
  assert.equal(commands().includes('keyboard'), false);
  await start();
  await until(() => commands().includes('launch'), 'launch ACK');
  await React.act(async () => {
    key('keydown');
    key('keyup');
  });
  assert.deepEqual(
    f.outbound
      .filter((message) => message.command === 'keyboard')
      .map((message) => [message.code, message.down]),
    [
      ['KeyZ', true],
      ['KeyZ', false],
    ],
  );
  await React.act(async () => {
    f.native.emit('exit', {
      status: 'success',
    });
  });
  await until(() => !playerOpen(), 'native Exit removed input owner');
  const count = f.outbound.filter((message) => message.command === 'keyboard').length;
  await React.act(async () => {
    key('keydown');
    key('keyup');
  });
  assert.equal(f.outbound.filter((message) => message.command === 'keyboard').length, count);
});

test('actual BrowserLauncher: same-card reopen preserves session-only settings and disclosure when durable storage is blocked', async () => {
  const f = await mountBrowser({
    blockStorage: true,
  });
  assert.equal(node('#focusHitboxToggle').getAttribute('aria-checked'), 'false');
  await click('#focusHitboxToggle');
  await click('#mobileOptionsToggle');
  assert.equal(node('#focusHitboxToggle').getAttribute('aria-checked'), 'true');
  assert.equal(node('#mobileOptionsToggle').getAttribute('aria-expanded'), 'true');
  await click('#libraryBack');
  await until(() => !env.document.querySelector('.tools[aria-hidden="false"]'), 'options close');
  await click('.game[data-game="th06"]:not([data-product])');
  await until(
    () => !!env.document.querySelector('.tools[aria-hidden="false"]'),
    'same selected product reopens',
  );
  assert.equal(
    node('#status').textContent,
    'Selected 東方紅魔郷',
    'main app1790/8652 uses the product title for selection status',
  );
  assert.equal(
    node('#toast').classList.contains('show'),
    false,
    'status-only selection must not open an empty toast',
  );
  assert.equal(node('#toastText').textContent, '');
  assert.equal(
    node('#focusHitboxToggle').getAttribute('aria-checked'),
    'true',
    'main same-product lifetime never rereads stale blocked storage',
  );
  assert.equal(node('#mobileOptionsToggle').getAttribute('aria-expanded'), 'true');
  assert.equal(
    JSON.parse(f.storage.getItem(api.gamePreferenceStorageKey('th06'))).options.focusHitboxEnabled,
    undefined,
  );
});

for (const editing of [false, true])
  test(`actual BrowserLauncher: late Host fallback preserves ${editing ? 'dirty editor/fullscreen/artwork' : 'selected options'} and raw address`, async () => {
    const initial = '/?game=th06&keep=a%20b#retained';
    const f = await mountBrowser({
      initial,
      holdHost: true,
      host: subsetHost(['th09', 'th07']),
      seedStorage(storage) {
        storage.values.set(
          api.gamePreferenceStorageKey('th07'),
          JSON.stringify({
            music: 'none',
            options: {
              focusHitboxEnabled: true,
            },
          }),
        );
      },
    });
    const frame = node('#gameFrame');
    let editor, image, bombStyle, fullscreen;
    if (editing) {
      await click('#mobileOptionsToggle');
      await click('#touchLayoutEdit');
      await until(
        () => env.document.querySelector('#touchLayoutSave')?.disabled === false,
        'actual editor draft initialized before Host',
      );
      editor = node('#touchLayoutEditor');
      image = node('#player').style.getPropertyValue('--touch-preview-image');
      const bomb = node('#touchBomb');
      await React.act(async () => {
        bomb.dispatchEvent(
          new env.window.PointerEvent('pointerdown', {
            bubbles: true,
            pointerId: 7,
            pointerType: 'mouse',
            button: 0,
            clientX: 175,
            clientY: 225,
          }),
        );
        env.document.dispatchEvent(
          new env.window.PointerEvent('pointermove', {
            bubbles: true,
            pointerId: 7,
            pointerType: 'mouse',
            clientX: 207,
            clientY: 241,
          }),
        );
        env.document.dispatchEvent(
          new env.window.PointerEvent('pointerup', {
            bubbles: true,
            pointerId: 7,
            pointerType: 'mouse',
            clientX: 207,
            clientY: 241,
          }),
        );
      });
      bombStyle = bomb.getAttribute('style');
      fullscreen = f.fullscreen;
      assert.equal(fullscreen, node('#player'));
      assert.match(image, /th06-card\.webp/);
    }
    const locationKey = f.router.state.location.key,
      routes = f.routeEvents.length;
    await acceptHeldHost(f);
    assert.equal(
      node('#gameId').dataset.game,
      'th07',
      'fallback follows PRODUCT_GAMES order rather than Host property insertion',
    );
    assert.equal(
      node('#focusHitboxToggle').getAttribute('aria-checked'),
      'true',
      'fallback restores the selected game preferences',
    );
    assert.ok(visibleOptions());
    assert.equal(currentUrl(), initial);
    assert.equal(f.router.state.location.key, locationKey);
    assert.equal(f.routeEvents.length, routes);
    assert.equal(node('#gameFrame'), frame);
    assert.deepEqual(f.navigations, []);
    assert.deepEqual(commands(), []);
    if (editing) {
      assert.equal(node('#touchLayoutEditor'), editor);
      assert.equal(f.fullscreen, fullscreen);
      assert.equal(node('#player').style.getPropertyValue('--touch-preview-image'), image);
      assert.equal(
        node('#touchBomb').getAttribute('style'),
        bombStyle,
        'draft placement survives metadata fallback',
      );
      assert.equal(f.order.filter((value) => value === 'fullscreen-enter').length, 1);
      assert.equal(f.order.includes('fullscreen-exit'), false);
      assert.equal(
        f.storage.getItem(api.touchLayoutStorageKey),
        null,
        'draft is still uncommitted',
      );
      await click('#touchLayoutExit');
      assert.equal(node('#decisionDialog').open, true, 'draft remains dirty after Host fallback');
      await click('#decisionCancel');
      assert.equal(node('#touchLayoutEditor'), editor);
      await click('#touchLayoutSave');
      assert.ok(JSON.parse(f.storage.getItem(api.touchLayoutStorageKey)).profiles.landscape);
    } else assert.equal(playerOpen(), false);
  });

test('actual BrowserLauncher: absent default base falls back inside Host without opening home options', async () => {
  const initial = '/?keep=a%20b#retained',
    f = await mountBrowser({
      initial,
      holdHost: true,
      host: subsetHost(['th09', 'th07']),
    });
  const frame = node('#gameFrame');
  assert.equal(visibleOptions(), null);
  await acceptHeldHost(f);
  assert.equal(visibleOptions(), null);
  assert.equal(playerOpen(), false);
  assert.equal(currentUrl(), initial);
  assert.equal(f.routeEvents.length, f.initialRouteCount);
  assert.equal(node('#gameFrame'), frame);
  assert.deepEqual(f.navigations, []);
  await click('.game[data-game="th07"]:not([data-product])');
  assert.equal(
    visibleOptions(),
    null,
    'main game-library350–357: first different-cover click only previews it',
  );
  await click('.game[data-game="th07"]:not([data-product])');
  assert.equal(
    node('#status').textContent,
    'Selected 東方妖々夢',
    'Host fallback already selected TH07 business preferences while home stayed closed',
  );
  assert.equal(node('#gameId').dataset.game, 'th07');
});

test('actual BrowserLauncher: canonical installed hint preserves session-only language/options after first Host', async () => {
  const host = siteManifest(),
    language = {
      id: 'en',
      title: 'English',
      pack: {
        url: 'lang/en.zip',
        bytes: 3,
        sha256: 'c'.repeat(64),
        runtimeVersion: 'fixture',
        files: 1,
      },
    };
  host.games.th06.languages.push(language);
  host.games.th06.languageOptions.push(language);
  const f = await mountBrowser({
    host,
    seedStorage(storage) {
      storage.values.set(api.languagePreferenceStorageKey('th06'), 'ja');
    },
  });
  f.storage.blocked = true;
  await React.act(async () => {
    const language = node('#languageSelect');
    language.value = 'en';
    language.dispatchEvent(
      new env.window.Event('change', {
        bubbles: true,
      }),
    );
  });
  await click('#focusHitboxToggle');
  assert.equal(node('#languageSelect').value, 'en');
  assert.equal(node('#focusHitboxToggle').getAttribute('aria-checked'), 'true');
  await click('#gamePackageImport');
  await until(() => !node('#gameDataImportWindow').hidden, 'manual canonical import window opens');
  await selectImport(importPackageFile());
  await until(() => node('#gameDataImportWindow').hidden, 'manual import completes without launch');
  await React.act(async () => {
    assert.equal(
      (await api.readCurrentPackageGeneration('th06')).generation.descriptor.revision,
      'synthetic-import-r1',
    );
  });
  await tick();
  assert.equal(node('#languageSelect').value, 'en');
  assert.equal(node('#focusHitboxToggle').getAttribute('aria-checked'), 'true');
  assert.equal(f.storage.getItem(api.languagePreferenceStorageKey('th06')), 'ja');
  assert.equal(playerOpen(), false);
  assert.deepEqual(f.navigations, []);
});

// Main1499–1521 may replace business selection while Start's existing input
// warning is open. Main9560–9569 then opens/starts the current selected game.
for (const boundary of ['warning', 'fullscreen'])
  test(`actual BrowserLauncher: late Host fallback during Start ${boundary} launches the current selected product`, async () => {
    const initial = '/?game=th07&keep=a%20b#retained';
    const f = await mountBrowser({
      initial,
      holdHost: true,
      host: subsetHost(['th06']),
      seedStorage(storage) {
        storage.values.set(
          api.gamePreferenceStorageKey('th07'),
          JSON.stringify({
            music: 'none',
            options: {
              touchEnabled: false,
            },
          }),
        );
      },
    });
    const frame = node('#gameFrame');
    await click('#launch');
    assert.equal(node('#decisionDialog').open, true);
    assert.equal(playerOpen(), false);
    let releaseFullscreen;
    if (boundary === 'fullscreen') {
      f.fullscreenReady = new Promise((resolve) => {
        releaseFullscreen = resolve;
      });
      await click('#decisionConfirm');
      assert.equal(f.order.includes('fullscreen-enter'), true);
      assert.deepEqual(f.navigations, []);
    }
    await acceptHeldHost(f);
    assert.equal(node('#gameId').dataset.game, 'th06');
    assert.equal(currentUrl(), initial);
    if (boundary === 'warning') {
      assert.equal(
        node('#decisionDialog').open,
        true,
        'Host arrival preserves the already-open warning',
      );
      await click('#decisionConfirm');
    } else
      await React.act(async () => {
        releaseFullscreen();
      });
    await until(() => commands().includes('launch'), 'Start continues with Host-selected TH06');
    assert.match(f.native.location.href, /\/runtime\/th06\/th06\.html/);
    assert.equal(f.outbound.find((message) => message.command === 'configure').game, 'th06');
    assert.equal(node('#gameFrame'), frame);
    assert.equal(playerOpen(), true);
    assert.equal(f.order.filter((value) => value === 'fullscreen-enter').length, 1);
    await back();
    await until(() => !playerOpen(), 'selected fallback Runtime closes normally');
  });

// Main4928–4949 reads music after the touch-input confirmation, not before it.

test('actual BrowserLauncher hydrates shared static cards into one live session without duplicate IDs or initial owners', async () => {
  const f = await mountBrowser({
    initial: '/?keep=a%20b',
    hydrate: true,
    noticeEnabled: true,
  });
  await tick();
  assert.ok(f.initialNodes.length > 0);
  assert.ok(f.initialNodes.every((node) => !node.isConnected));
  const ids = [...env.document.querySelectorAll('[id]')].map((node) => node.id);
  assert.equal(new Set(ids).size, ids.length);
  assert.equal(env.document.querySelectorAll('#gameFrame').length, 1);
  assert.equal(f.order.filter((value) => value === 'boot-ready').length, 1);
  assert.equal(
    f.fetches.filter((item) => new URL(item.url).pathname.endsWith('/NOTICE.txt')).length,
    1,
  );
  assert.equal(typeof env.window.__eaglerPrepareManagedRuntimeDataV1, 'function');
  const frame = node('#gameFrame');
  await click('.shelf-lobby-link');
  await tick();
  assert.equal(node('#gameFrame'), frame, 'shared route navigation retains the sole live iframe');
  assert.equal(env.document.querySelectorAll('#mastheadMenu').length, 1);
});

test('actual BrowserLauncher: direct timeout refreshes dismissed recovery reason without reopening it', async () => {
  const timers = capturePreparationTimers();
  try {
    const f = await mountBrowser({
        autoReady: false,
      }),
      session = api.getTestSession();
    await React.act(async () => {
      session.setLocale('zh-CN');
    });
    await start();
    await until(() => timers.live(120_000).length === 1, 'real direct Runtime ready wait');
    await React.act(async () => {
      f.native.emit('transfer', {
        kind: 'game',
        loaded: 1,
        total: 100,
      });
      timers.fire(timers.live(20_000)[0]);
    });
    assert.equal(node('#gameDataImportWindow').hidden, false);
    const originalReason = node('#gameDataImportReason').textContent;
    await click('#gameDataImportClose');
    assert.equal(node('#gameDataImportWindow').hidden, true);
    await React.act(async () => {
      timers.fire(timers.live(120_000)[0]);
    });
    await tick();
    assert.equal(session.gameData.getSnapshot().attempt.unlocked, true);
    assert.equal(session.gameData.getSnapshot().attempt.dialogDismissed, true);
    assert.equal(node('#gameDataImportWindow').hidden, true);
    assert.notEqual(node('#gameDataImportReason').textContent, originalReason);
    assert.match(node('#gameDataImportReason').textContent, /^游戏运行组件加载已超时。/);
    assert.equal(session.startupError.getSnapshot().open, true);
  } finally {
    timers.restore();
  }
});

for (const testBuild of [false, true])
  test(`BrowserSession preserves entry test-build flag for acquisition (${testBuild})`, async () => {
    await mountBrowser({
      initial: `/?game=th06${testBuild ? '&test' : ''}`,
    });
    assert.equal(api.getAcquisitionOptions().testBuild, testBuild);
    assert.equal(new URL(api.getAcquisitionOptions().baseUrl).search, '');
  });

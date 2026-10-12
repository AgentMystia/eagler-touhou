import {
  React,
  api,
  assert,
  back,
  click,
  commands,
  currentUrl,
  env,
  mountBrowser,
  node,
  playerOpen,
  start,
  subsetHost,
  test,
  tick,
  until,
  visibleOptions,
} from './browser-session-fixture.mjs';

test('actual BrowserLauncher: Back waits for native sync; repeated Back does not create a second close', async () => {
  const f = await mountBrowser();
  await start();
  await until(() => commands().includes('launch'), 'launch ACK');
  f.syncMode = 'hold';
  const previous = currentUrl();
  await back();
  await until(() => f.pendingSync.length === 1, 'actual session requested one save sync');
  assert.equal(playerOpen(), true);
  assert.equal(currentUrl(), previous);
  assert.notEqual(f.native.location.href, 'about:blank');
  await back();
  assert.equal(f.pendingSync.length, 1, 'repeated navigation shares the current close');
  await React.act(async () => {
    f.native.respond(f.pendingSync[0]);
  });
  await until(
    () => !playerOpen() && currentUrl() === previous,
    'successful native sync returns to its options',
  );
});

test('actual BrowserLauncher: manual close exits fullscreen before retiring the native writer', async () => {
  const f = await mountBrowser();
  await start();
  const index = f.order.length;
  await React.act(async () => {
    assert.equal(await api.getTestSession().requestRuntimeClose(), true);
  });
  const closing = f.order.slice(index);
  assert.ok(closing.indexOf('sync') < closing.indexOf('fullscreen-exit'));
  assert.ok(closing.indexOf('fullscreen-exit') < closing.indexOf('native-retired'));
  assert.equal(playerOpen(), false);
});

test('actual BrowserLauncher: StrictMode initialization owns one notice request, iframe, DATA callback and player input lifetime', async () => {
  const f = await mountBrowser({
    strictMode: true,
    noticeEnabled: true,
  });
  await tick();
  assert.equal(f.order.filter((value) => value === 'boot-ready').length, 1);
  assert.equal(
    f.fetches.filter((item) => new URL(item.url).pathname.endsWith('/NOTICE.txt')).length,
    1,
  );
  assert.equal(env.document.querySelectorAll('#gameFrame').length, 1);
  assert.equal(typeof env.window.__eaglerPrepareManagedRuntimeDataV1, 'function');
  await start();
  await until(() => commands().includes('launch'), 'StrictMode real session launches');
  await React.act(async () => {
    env.window.dispatchEvent(
      new env.window.KeyboardEvent('keydown', {
        code: 'KeyZ',
        key: 'z',
        keyCode: 90,
      }),
    );
    env.window.dispatchEvent(
      new env.window.KeyboardEvent('keyup', {
        code: 'KeyZ',
        key: 'z',
        keyCode: 90,
      }),
    );
  });
  assert.equal(
    f.outbound.filter((message) => message.command === 'keyboard').length,
    2,
    'strict remount does not duplicate input forwarding',
  );
  await back();
  await until(() => !playerOpen(), 'StrictMode owner closes normally');
});

// These are supplemental owner-integration checks derived from main1493–1521,
// 4840–4845 and9742–9751. The original browser scenarios remain unchanged.

for (const close of ['Exit', 'Back'])
  test(`actual BrowserLauncher: original touch-settings144–151 hidden retained editor opens from home and ${close} returns home`, async () => {
    const f = await mountBrowser({
        host: subsetHost(['th06', 'th07']),
      }),
      frame = node('#gameFrame');
    await click('#mobileOptionsToggle');
    await click('#touchLayoutEdit');
    await until(
      () => env.document.querySelector('#touchLayoutSave')?.disabled === false,
      'first retained-product editor initialized',
    );
    await click('#touchLayoutExit');
    if (node('#decisionDialog').open) await click('#decisionConfirm');
    await until(
      () => !env.document.querySelector('#touchLayoutEditor'),
      'first editor closes, as original lines136–143',
    );

    // Preserve the original sequence, including its programmatic hidden-button
    // click. A single different-cover click previews TH07; it does not actually
    // select TH07 business settings (main game-library350–357).
    await click('#libraryBack');
    await until(() => !visibleOptions(), 'Back returns to home');
    const home = currentUrl(),
      homeKey = f.router.state.location.key,
      retainedButton = node('#touchLayoutEdit');
    await click('.game[data-game="th07"]:not([data-product])');
    assert.equal(visibleOptions(), null);
    assert.equal(currentUrl(), home);
    assert.equal(
      node('#gameId').dataset.game,
      'th06',
      'the original sequence still owns the previously selected TH06 settings',
    );
    if (!node('#mobileOptions').classList.contains('open')) await click('#mobileOptionsToggle');
    assert.equal(node('#mobileOptions').classList.contains('open'), true);
    await React.act(async () => {
      node('#touchLayoutEdit').click();
    });
    await until(
      () => env.document.querySelector('#touchLayoutSave')?.disabled === false,
      'original programmatic hidden-button click opens its retained editor',
    );
    assert.equal(
      node('#touchLayoutEditor').hidden,
      false,
      'original test-touch-settings-browser.py151 assertion',
    );
    assert.equal(node('#touchLayoutEdit'), retainedButton);
    assert.equal(node('#gameId').dataset.game, 'th06');
    assert.match(
      node('#player').style.getPropertyValue('--touch-preview-image'),
      /th06-card\.webp/,
    );
    assert.equal(
      visibleOptions(),
      null,
      'home-parent editor does not invent an open options surface behind itself',
    );
    assert.equal(currentUrl(), home);
    assert.notEqual(f.router.state.location.key, homeKey);
    assert.equal(node('#gameFrame'), frame);
    assert.deepEqual(f.navigations, []);
    if (close === 'Exit') await click('#touchLayoutExit');
    else await back();
    if (node('#decisionDialog').open) await click('#decisionConfirm');
    await until(
      () => !env.document.querySelector('#touchLayoutEditor') && !playerOpen(),
      `${close} closes only the editor`,
    );
    assert.equal(visibleOptions(), null);
    assert.equal(currentUrl(), home);
    assert.equal(f.router.state.location.key, homeKey);
    assert.equal(node('#gameFrame'), frame);
    assert.equal(node('#gameId').dataset.game, 'th06');
  });

import {
  React,
  acceptHeldHost,
  actualTerminalRoom,
  api,
  assert,
  back,
  captureTerminalFeedback,
  click,
  commands,
  currentUrl,
  developmentAcquisitionHost,
  env,
  liveActualDirectory,
  mountBrowser,
  node,
  playerOpen,
  subsetHost,
  test,
  tick,
  until,
} from './browser-session-fixture.mjs';

for (const preview of ['touch', 'touch-hud'])
  test(`actual BrowserLauncher: boot ${preview} is a one-time preference override before Host, with no native or history side effects`, async () => {
    const initial = `/?game=th07&preview=${preview}&keep=a%20b#retained`;
    const f = await mountBrowser({
      initial,
      holdHost: true,
      host: subsetHost(['th07']),
      seedStorage(storage) {
        storage.values.delete('eagler-touch-help-seen-v8');
        storage.values.set(
          api.gamePreferenceStorageKey('th07'),
          JSON.stringify({
            music: 'none',
            options: {
              touchEnabled: false,
              focusHitboxEnabled: true,
            },
          }),
        );
      },
    });
    const frame = node('#gameFrame'),
      player = node('#player'),
      help = node('#touchHelp'),
      image = player.style.getPropertyValue('--touch-preview-image');
    assert.equal(node('#touchToggle').getAttribute('aria-checked'), 'true');
    assert.equal(
      node('#focusHitboxToggle').getAttribute('aria-checked'),
      'true',
      'routed TH07 preferences precede preview',
    );
    assert.equal(playerOpen(), true);
    assert.equal(player.classList.contains('touch-preview'), true);
    assert.equal(player.classList.contains('touch-enabled'), true);
    assert.match(image, /th07-card\.webp/);
    assert.equal(help.hidden, preview !== 'touch');
    assert.equal(currentUrl(), initial);
    assert.equal(f.routeEvents.length, f.initialRouteCount);
    assert.deepEqual(f.navigations, []);
    assert.deepEqual(commands(), []);
    assert.equal(f.fullscreen, null);
    assert.equal(f.storage.getItem('eagler-touch-help-seen-v8'), null);
    assert.equal(
      f.storage.writes.some(
        ([key, value]) =>
          key === api.sharedTouchPreferenceStorageKey && JSON.parse(value).touchEnabled,
      ),
      false,
      'preview never persists the enabled override',
    );
    await acceptHeldHost(f);
    assert.equal(
      node('#touchToggle').getAttribute('aria-checked'),
      'false',
      'first Host restores the saved disabled preference',
    );
    assert.equal(node('#gameFrame'), frame);
    assert.equal(node('#player'), player);
    assert.equal(node('#touchHelp'), help);
    assert.equal(playerOpen(), true);
    assert.equal(player.classList.contains('touch-preview'), true);
    assert.equal(player.classList.contains('touch-enabled'), false);
    assert.equal(player.style.getPropertyValue('--touch-preview-image'), image);
    assert.equal(help.hidden, preview !== 'touch');
    assert.equal(currentUrl(), initial);
    assert.equal(f.routeEvents.length, f.initialRouteCount);
    assert.deepEqual(f.navigations, []);
    assert.deepEqual(commands(), []);
    assert.equal(f.order.includes('fullscreen-enter'), false);
    assert.equal(f.storage.getItem('eagler-touch-help-seen-v8'), null);
    await React.act(async () => {
      for (const type of ['keydown', 'keyup'])
        env.window.dispatchEvent(
          new env.window.KeyboardEvent(type, {
            code: 'KeyZ',
            key: 'z',
            keyCode: 90,
            bubbles: true,
          }),
        );
    });
    assert.deepEqual(commands(), [], 'static preview never creates a gameplay input owner');
  });

// Main lobby.mts113 gives the directory guide its filter product. The Launcher
// Host's independently retained business selection is not the directory filter.
for (const timing of ['together', 'after-directory'])
  test(`actual BrowserLauncher: lobby guide follows directory filter and settings stay explicit (Host ${timing})`, async () => {
    const f = await mountBrowser({
      initial: '/lobby.html?game=th07mp&keep=a%20b',
      holdHost: timing === 'after-directory' ? 'launcher' : true,
      host: subsetHost(['th06', 'th07']),
    });
    if (timing === 'after-directory')
      await until(
        () => env.document.querySelector('#lobbyGameRail .game.nav-preview[data-product="th07mp"]'),
        'actual directory applies TH07 filter before Launcher Host',
      );
    await React.act(async () => {
      f.releaseHost();
    });
    await tick();
    await until(
      () => env.document.querySelector('#lobbyGameRail .game.nav-preview[data-product="th07mp"]'),
      'actual directory applies TH07 filter',
    );
    assert.equal(
      f.fetches.filter((item) => new URL(item.url).pathname.endsWith('/site-manifest.json')).length,
      2,
    );
    assert.equal(
      node('#lobbyOptionsDialog').open,
      false,
      'the directory filter is not an instruction to open game settings',
    );
    await click('#lobbyGuideOpen');
    await until(
      () => env.document.querySelector('#mpGuideContent .multiplayer-rule-game-tab.selected'),
      'authored guide content loads',
    );
    assert.equal(node('#mpGuideContent .multiplayer-rule-game-tab.selected').dataset.game, 'th07');
    assert.equal(
      node('#mpGuideContent .multiplayer-rule-panel:not([hidden])').dataset.game,
      'th07',
    );
    assert.equal(
      new URL(currentUrl(), env.window.location.href).searchParams.get('game'),
      'th07mp',
    );
    assert.equal(playerOpen(), false);
    assert.deepEqual(f.navigations, []);
    await click('#mpGuideClose');
    await until(() => !node('#mpGuideDialog').open, 'guide closes its own entry');
    await click('#lobbyGameRail .game[data-product="th07mp"]');
    await until(
      () => node('#lobbyOptionsDialog').open,
      'explicit card click still opens its original settings carrier',
    );
    assert.equal(node('#gameId').textContent, 'TH07 MP');
    assert.equal(
      new URL(currentUrl(), env.window.location.href).searchParams.get('game'),
      'th07mp',
      'options product does not replace the independent filter',
    );
    await click('#libraryBack');
    await until(
      () => !node('#lobbyOptionsDialog').open,
      'settings Back returns to the same directory',
    );
    await click('#lobbyGameRail .game[data-product="th06mp"]');
    assert.equal(
      node('#lobbyOptionsDialog').open,
      false,
      'browsing another cover changes only the filter',
    );
    await click('#lobbyGuideOpen');
    await until(
      () => node('#mpGuideDialog').open,
      'directory guide reopens after another product settings',
    );
    assert.equal(node('#mpGuideContent .multiplayer-rule-game-tab.selected').dataset.game, 'th06');
    assert.equal(
      f.fetches.filter((item) => new URL(item.url).pathname.endsWith('/content/MULTIPLAYER.html'))
        .length,
      1,
      'cached authored guide follows the filter on reopen',
    );
  });

test('directory TH11MP settings stay available without a hosted multiplayer Runtime', async () => {
  const host = developmentAcquisitionHost(['th11']);
  delete host.games.th11.multiplayerRuntime;
  host.shared.testBuild = true;
  await mountBrowser({
    initial: '/lobby.html?game=th11mp',
    host,
    actualHistory: true,
  });
  await until(
    () => env.document.querySelector('#lobbyGameRail .game.nav-preview[data-product="th11mp"]'),
    'directory advertises TH11MP',
  );
  const session = api.getTestSession();
  assert.equal(session.metadata.getSnapshot().products.includes('th11mp'), false);
  assert.equal(session.directory.getSnapshot().products.includes('th11mp'), true);
  await click('#lobbyGameRail .game[data-product="th11mp"]');
  await until(
    () => node('#lobbyOptionsDialog').open,
    'TH11MP card opens settings independently of hosted Runtime',
  );
  assert.equal(node('#gameId').textContent, 'TH11 MP');
  assert.equal(session.settings.getSnapshot().context.productId, 'th11mp');
  await click('#libraryBack');
  await until(() => !node('#lobbyOptionsDialog').open, 'Back closes TH11MP settings');
});

// Main lobby.mts has no automatic first-use presentation. A route-owned shared
// component must not let the previous Launcher request appear over that page.

// Main lobby.mts has no automatic first-use presentation. A route-owned shared
// component must not let the previous Launcher request appear over that page.
test('actual BrowserLauncher: delayed automatic first-use cannot open or mark seen after entering lobby', async () => {
  const f = await mountBrowser({
    initial: '/?keep=a%20b',
    holdFirstUse: true,
    host: subsetHost(['th06', 'th07']),
    seedStorage(storage) {
      storage.values.delete('eagler-touhou-first-use-notice-seen-v1');
    },
  });
  await until(
    () =>
      f.fetches.some((item) =>
        new URL(item.url).pathname.endsWith('/content/FIRST_USE_NOTICE.html'),
      ),
    'Launcher begins automatic first-use read',
  );
  await click('.game[data-product="th07mp"]');
  await until(
    () => currentUrl().startsWith('/lobby.html'),
    'MP card enters the original directory surface',
  );
  await React.act(async () => {
    f.releaseFirstUse();
  });
  await tick();
  await tick();
  assert.equal(
    node('#firstUseNoticeDialog').open,
    false,
    'retired Launcher request cannot open a modal on the directory',
  );
  assert.equal(
    f.storage.getItem('eagler-touhou-first-use-notice-seen-v1'),
    null,
    'unshown content is not marked seen',
  );
  await click('#firstUseNoticeOpen');
  await until(
    () => node('#firstUseNoticeDialog').open,
    'manual directory help still opens the already-loaded content',
  );
  assert.equal(f.storage.getItem('eagler-touhou-first-use-notice-seen-v1'), '1');
  assert.equal(
    f.fetches.filter((item) =>
      new URL(item.url).pathname.endsWith('/content/FIRST_USE_NOTICE.html'),
    ).length,
    1,
  );
});

// Main app9784–9793 applies first-use gating on the Launcher independently of
// the lobby's site-notice-only entry. An interrupted load remains unseen.

// Main app9784–9793 applies first-use gating on the Launcher independently of
// the lobby's site-notice-only entry. An interrupted load remains unseen.
test('actual BrowserLauncher: unseen first-use resumes automatically on library return after lobby', async () => {
  const f = await mountBrowser({
    initial: '/?keep=a%20b',
    holdFirstUse: true,
    host: subsetHost(['th06', 'th07']),
    seedStorage(storage) {
      storage.values.delete('eagler-touhou-first-use-notice-seen-v1');
    },
  });
  await until(
    () =>
      f.fetches.some((item) =>
        new URL(item.url).pathname.endsWith('/content/FIRST_USE_NOTICE.html'),
      ),
    'Launcher begins first-use read',
  );
  await click('.game[data-product="th07mp"]');
  await until(
    () => currentUrl().startsWith('/lobby.html'),
    'directory replaces the pending Launcher startup context',
  );
  await React.act(async () => {
    f.releaseFirstUse();
  });
  await tick();
  await tick();
  assert.equal(node('#firstUseNoticeDialog').open, false);
  assert.equal(f.storage.getItem('eagler-touhou-first-use-notice-seen-v1'), null);
  await back();
  await until(
    () => node('#firstUseNoticeDialog').open,
    'returning Launcher automatically presents the still-unseen cached notice',
  );
  assert.equal(currentUrl(), '/?keep=a%20b');
  assert.equal(f.storage.getItem('eagler-touhou-first-use-notice-seen-v1'), '1');
  assert.equal(
    f.fetches.filter((item) =>
      new URL(item.url).pathname.endsWith('/content/FIRST_USE_NOTICE.html'),
    ).length,
    1,
  );
  await click('#firstUseNoticeClose');
  await until(() => !node('#firstUseNoticeDialog').open, 'closing consumes only the notice entry');
  await click('#firstUseNoticeOpen');
  await until(
    () => node('#firstUseNoticeDialog').open,
    'manual reopening still works after automatic presentation',
  );
  assert.equal(
    f.fetches.filter((item) =>
      new URL(item.url).pathname.endsWith('/content/FIRST_USE_NOTICE.html'),
    ).length,
    1,
  );
});

for (const strictMode of [false, true])
  for (const action of ['create', 'join by code', 'join row']) {
    test(`actual BrowserLauncher directory ${action} closes before pinned entry and returns (StrictMode=${strictMode})`, async () => {
      const { f, session, directorySocket } = await liveActualDirectory({
          strictMode,
        }),
        frame = node('#gameFrame');
      const navigationObservations = [],
        unsubscribe = f.router.subscribe((state) => {
          if (
            new URLSearchParams(state.location.search).has('j') &&
            !navigationObservations.some((item) => item.location.key === state.location.key)
          )
            navigationObservations.push({
              location: state.location,
              dialogOpen: env.document.querySelector('#roomDialog')?.open === true,
            });
        });
      if (action === 'join row') {
        await React.act(async () => {
          session.directory.selectProduct('th07mp');
          directorySocket.message({
            type: 'directory',
            version: 1,
            product: 'th07mp',
            total: 1,
            rooms: [
              {
                product: 'th07mp',
                code: '4321',
                capacity: 2,
                phase: 'lobby',
                joinable: true,
                seats: [
                  {
                    initial: 'R',
                  },
                  null,
                ],
                difficulty: 2,
              },
            ],
          });
        });
        await click('.lobby-join');
      } else {
        await click(action === 'create' ? '#createButton' : '#codeButton');
        assert.equal(node('#roomDialog').open, true);
        await React.act(async () =>
          session.directory.updateForm({
            product: 'th07mp',
            code: '4321',
            difficulty: 2,
            visibility: 'private',
            disableCheatMovement: true,
          }),
        );
        await click('#submitRoom');
      }
      await until(
        () => f.sockets.some((socket) => new URL(socket.url).searchParams.has('lobby')),
        'actual room transport follows accepted entry',
      );
      unsubscribe();
      const roomSocket = f.sockets.find((socket) => new URL(socket.url).searchParams.has('lobby'));
      const endpoint = new URL(roomSocket.url),
        state = session.room.service.getSnapshot(),
        code = state.room.code;
      assert.equal(
        new Set(navigationObservations.map((item) => item.location.pathname + item.location.search))
          .size,
        1,
        'entry and original room-history seeding share one canonical invite target',
      );
      assert.equal(
        navigationObservations[0].dialogOpen,
        false,
        'main closes the dialog before navigation',
      );
      assert.equal(endpoint.searchParams.get('room'), `th07mp-${code}`);
      assert.equal(endpoint.searchParams.get('intent'), action === 'create' ? 'create' : 'join');
      assert.equal(
        session.settings.getSnapshot().context.productId,
        'th07mp',
        'form/row selection replaces the different directory filter for room settings',
      );
      assert.equal(state.fromDirectory, true);
      assert.equal(
        env.document.querySelector('#roomDialog'),
        null,
        'main room document owns no directory form',
      );
      assert.deepEqual(directorySocket.closes, [
        {
          code: 1000,
          reason: 'leave directory',
        },
      ]);
      if (action === 'create') {
        assert.equal(endpoint.searchParams.get('visibility'), 'private');
        assert.equal(endpoint.searchParams.get('disableCheatMovement'), '1');
      } else assert.equal(code, '4321');
      const client = endpoint.searchParams.get('lobby');
      await React.act(async () => {
        roomSocket.open();
        roomSocket.message({
          type: 'state',
          roomDirectory: {
            version: 1,
            controlModes: true,
          },
          room: {
            code,
            playerCount: 2,
            difficulty: 2,
            phase: 'lobby',
            inputDelay: 0,
            predictionLimit: 8,
            startSerial: 0,
            seats: [
              {
                clientId: action === 'create' ? client : 'other-host',
                name: 'R',
                loadout: 0,
                ready: false,
              },
              null,
            ],
            spectators: [],
          },
        });
      });
      if (action !== 'create')
        assert.equal(roomSocket.sent.findLast((message) => message.type === 'take-seat')?.seat, 1);
      await click('#mpLeaveRoom');
      await until(() => currentUrl().startsWith('/lobby.html'), 'full leave returns to directory');
      assert.equal(currentUrl(), '/lobby.html?game=th07mp');
      assert.equal(session.room.service.getSnapshot().room, null);
      assert.equal(session.directory.getSnapshot().selectedProduct, 'th07mp');
      assert.equal(node('#gameFrame'), frame);
      assert.equal(
        f.sockets.filter((socket) => new URL(socket.url).searchParams.has('lobby')).length,
        1,
      );
      assert.equal(node('#roomDialog').open, false);
    });
  }

for (const strictMode of [false, true])
  for (const dismissal of [
    'cancel',
    'Back',
    'newer destination',
    'newer lobby',
    'newer same product',
    'unmount',
  ]) {
    test(`actual BrowserLauncher directory ${dismissal} never consumes a stale form intent (StrictMode=${strictMode})`, async (t) => {
      const { f, session } = await liveActualDirectory({
        strictMode,
      });
      await click('#codeButton');
      await React.act(async () =>
        session.directory.updateForm({
          code: '4321',
        }),
      );
      if (dismissal === 'cancel') await click('#closeDialog');
      else if (dismissal === 'Back') await back();
      else if (dismissal.startsWith('newer')) {
        await React.act(async () => {
          node('#submitRoom').click();
          void f.router.navigate(
            dismissal === 'newer lobby'
              ? '/lobby.html?game=th07mp&newer=1'
              : dismissal === 'newer same product'
                ? '/lobby.html?game=th06mp&newer=1'
                : '/en.html?game=th06&newer=1',
          );
        });
        await tick();
        assert.equal(
          new URL(currentUrl(), env.window.location.href).searchParams.get('newer'),
          '1',
        );
      } else {
        // Keep the submitted navigation pending for every stale-intent/unmount
        // assertion below. Only afterward drain its known jsdom history traversal
        // so it cannot reach the next test's Router on this shared window.
        let onPop;
        const traversal = new Promise((resolve) => {
          onPop = resolve;
          env.window.addEventListener('popstate', onPop, {
            once: true,
          });
        });
        t.after(async () => {
          let deadline;
          try {
            await Promise.race([
              traversal,
              new Promise((_, reject) => {
                deadline = setTimeout(
                  () =>
                    reject(new Error('Submitted history traversal did not finish after unmount')),
                  10000,
                );
              }),
            ]);
          } finally {
            clearTimeout(deadline);
            env.window.removeEventListener('popstate', onPop);
          }
        });
        await React.act(async () => {
          node('#submitRoom').click();
          f.root.unmount();
        });
        f.root = null;
        f.router.dispose();
        assert.throws(
          () => session.setLocale('en'),
          /Launcher Router is not attached/,
          'actual unmount releases the last committed Router port',
        );
        assert.equal(env.document.querySelector('#gameFrame'), null);
        assert.deepEqual(env.errors, []);
      }
      assert.equal(session.room.service.getSnapshot().room, null);
      assert.equal(
        f.sockets.filter((socket) => new URL(socket.url).searchParams.has('lobby')).length,
        0,
      );
      if (dismissal !== 'unmount' && dismissal !== 'newer destination') {
        assert.equal(node('#roomDialog').open, false);
        await click('#codeButton');
        await click('#closeDialog');
        assert.equal(
          f.sockets.filter((socket) => new URL(socket.url).searchParams.has('lobby')).length,
          0,
          'later close cannot replay a retired form',
        );
        if (dismissal === 'newer lobby' || dismissal === 'newer same product') {
          await click('#codeButton');
          await React.act(async () =>
            session.directory.updateForm({
              code: '5678',
            }),
          );
          await click('#submitRoom');
          await until(
            () => f.sockets.some((socket) => new URL(socket.url).searchParams.has('lobby')),
            'reopened form accepts its own newer submission',
          );
          const roomSocket = f.sockets.find((socket) =>
            new URL(socket.url).searchParams.has('lobby'),
          );
          assert.equal(
            new URL(roomSocket.url).searchParams.get('room'),
            `${dismissal === 'newer lobby' ? 'th07mp' : 'th06mp'}-5678`,
          );
          assert.equal(session.room.service.getSnapshot().room.code, '5678');
        }
      }
    });
  }

// Main app747–774 owns terminal lobby closure. Execute that pinned listener,
// then compare the actual document-lived session, BrowserRouter and feedback.
// Transport/Runtime endpoints remain synthetic; no native browser is exercised.

for (const locale of ['en', 'zh-CN'])
  for (const code of [4004, 4007, 4008, 4009, 4010])
    for (const fromDirectory of [false, true]) {
      test(`actual BrowserLauncher terminal ${code} ${locale} ${fromDirectory ? 'directory notice' : 'single toast'} matches pinned close`, async () => {
        const { f, session, socket } = await actualTerminalRoom({
          fromDirectory,
          locale,
        });
        const feedback = captureTerminalFeedback(session);
        const writesBefore = f.sessionStorage.writes.length,
          removesBefore = f.sessionStorage.removals.length;
        await React.act(async () => {
          socket.close(code);
        });
        await until(
          () =>
            session.room.service.getSnapshot().room === null &&
            currentUrl().startsWith('/lobby.html'),
          'terminal departure returns to its fixed lobby parent',
        );
        await tick();
        feedback.unsubscribe();
        assert.equal(session.room.service.getSnapshot().connected, false);
        assert.equal(f.sessionStorage.getItem('eagler-touhou-th06mp-room-v1'), null);
        assert.equal(
          f.sessionStorage.removals
            .slice(removesBefore)
            .filter((key) => key === 'eagler-touhou-th06mp-room-v1').length,
          1,
        );
        assert.equal(node('#notice').hidden, false);
        assert.equal(
          f.sessionStorage.getItem('eagler-lobby-message'),
          null,
          'directory consumes the committed return message once',
        );
        assert.equal(session.feedback.getSnapshot().toastOpen, feedback.initial.toastOpen);
        const socketCount = f.sockets.filter((value) =>
          new URL(value.url).searchParams.has('lobby'),
        ).length;
        await React.act(async () => {
          env.window.dispatchEvent(new env.window.Event('online'));
          env.window.dispatchEvent(
            new env.window.PageTransitionEvent('pageshow', {
              persisted: true,
            }),
          );
          env.document.dispatchEvent(new env.window.Event('visibilitychange'));
          socket.message({
            type: 'state',
            room: {
              code: '1234',
            },
          });
        });
        assert.equal(session.room.service.getSnapshot().room, null);
        assert.equal(
          f.sockets.filter((value) => new URL(value.url).searchParams.has('lobby')).length,
          socketCount,
          'terminal closes cannot be revived by reconnect wakes or stale socket messages',
        );
      });
    }

for (const fromDirectory of [false, true])
  test(`actual BrowserLauncher terminal launched ${fromDirectory ? 'directory' : 'direct'} room retains one pinned toast and membership`, async () => {
    const { f, session, socket } = await actualTerminalRoom({
        fromDirectory,
      }),
      code = 4009;
    await click('[data-mp-seat="0"] .mp-seat-edit');
    await click('#mpCheckGame');
    await until(
      () => session.getRuntime().getSnapshot().launched,
      'actual multiplayer preflight Runtime has launched',
    );
    const feedback = captureTerminalFeedback(session),
      before = currentUrl();
    const room = session.room.service.getSnapshot().room,
      saved = f.sessionStorage.getItem('eagler-touhou-th06mp-room-v1'),
      writesBefore = f.sessionStorage.writes.length;
    await React.act(async () => {
      socket.close(code);
    });
    feedback.unsubscribe();
    assert.equal(currentUrl(), before);
    assert.equal(session.getRuntime().getSnapshot().launched, true);
    assert.equal(playerOpen(), true);
    assert.equal(session.room.service.getSnapshot().room.code, room.code);
    assert.equal(session.room.service.getSnapshot().connected, false);
    assert.equal(f.sessionStorage.getItem('eagler-touhou-th06mp-room-v1'), saved);
    assert.equal(session.feedback.getSnapshot().toastRevision - feedback.initial.toastRevision, 1);
    const sockets = f.sockets.length;
    await React.act(async () => {
      session.room.service.reconnect();
      session.room.service.pageShow(true);
    });
    assert.equal(f.sockets.length, sockets, 'retained terminal room still has reconnect stopped');
    await React.act(async () => {
      f.native.emit('exit', {
        status: 'error',
      });
    });
    await until(() => !playerOpen(), 'authenticated Runtime Exit finishes the synthetic preflight');
  });

for (const strictMode of [false, true])
  test(`actual BrowserLauncher terminal warm directory return preserves notice (StrictMode=${strictMode})`, async () => {
    const { f, session, directorySocket } = await liveActualDirectory({
      strictMode,
    });
    assert.equal(session.directory.getSnapshot().initialized, true);
    await click('#createButton');
    await click('#submitRoom');
    await until(
      () => f.sockets.some((value) => new URL(value.url).searchParams.has('lobby')),
      'actual directory Create enters its room',
    );
    const socket = f.sockets.find((value) => new URL(value.url).searchParams.has('lobby')),
      roomCode = session.room.service.getSnapshot().room.code;
    await React.act(async () => {
      socket.open();
      socket.message({
        type: 'state',
        room: {
          code: roomCode,
          playerCount: 2,
          difficulty: 1,
          phase: 'lobby',
          inputDelay: 0,
          predictionLimit: 8,
          startSerial: 0,
          seats: [
            {
              clientId: new URL(socket.url).searchParams.get('lobby'),
              name: 'A',
              loadout: 0,
              ready: false,
            },
            null,
          ],
          spectators: [],
        },
      });
    });
    const feedback = captureTerminalFeedback(session),
      writesBefore = f.sessionStorage.writes.length;
    await React.act(async () => {
      socket.close(4010);
    });
    await until(
      () => currentUrl().startsWith('/lobby.html') && !node('#notice').hidden,
      'retained directory consumes the terminal return notice',
    );
    feedback.unsubscribe();
    assert.equal(session.room.service.getSnapshot().room, null);
    assert.equal(f.sessionStorage.getItem('eagler-lobby-message'), null);
    assert.deepEqual(directorySocket.closes, [
      {
        code: 1000,
        reason: 'leave directory',
      },
    ]);
    assert.equal(
      f.sockets.filter((value) => new URL(value.url).searchParams.has('lobby')).length,
      1,
    );
  });

test('actual BrowserLauncher: leaving a directly loaded TH07 room selects TH07 after the directory manifest arrives', async () => {
  const host = subsetHost(['th06', 'th07']);
  host.shared.netplayRelay = 'wss://relay.invalid/socket';
  const f = await mountBrowser({
    initial: `/?j=${api.encodeRoomInvite({
      g: 'th07mp',
      r: '1234',
    })}`,
    host,
    socketBoundary: true,
    directoryTransport: true,
    actualHistory: true,
    seedPackages: ['th07'],
  });
  const session = api.getTestSession();
  await until(
    () => f.sockets.some((value) => new URL(value.url).searchParams.has('lobby')),
    'direct TH07 room transport',
  );
  const socket = f.sockets.find((value) => new URL(value.url).searchParams.has('lobby'));
  await React.act(async () => {
    socket.open();
    socket.message({
      type: 'state',
      roomDirectory: {
        version: 1,
        controlModes: true,
      },
      room: {
        code: '1234',
        playerCount: 2,
        difficulty: 1,
        phase: 'lobby',
        inputDelay: 0,
        predictionLimit: 8,
        startSerial: 0,
        seats: [
          {
            clientId: new URL(socket.url).searchParams.get('lobby'),
            name: 'A',
            loadout: 0,
            ready: false,
          },
          null,
        ],
        spectators: [],
      },
    });
  });
  assert.equal(
    session.directory.getSnapshot().initialized,
    false,
    'room entry has no initialized directory',
  );
  await click('#mpLeaveRoom');
  await until(
    () => session.directory.getSnapshot().initialized,
    'returned directory loads its own manifest',
  );
  assert.equal(currentUrl(), '/lobby.html?game=th07mp');
  assert.equal(session.directory.getSnapshot().selectedProduct, 'th07mp');
  assert.ok(
    node('#lobbyGameRail .game.nav-preview[data-product="th07mp"]'),
    'visible card matches the return URL',
  );
});

for (const origin of ['directory create', 'directory join', 'direct'])
  test(`actual BrowserLauncher: ${origin} native Exit keeps its room, then Leave returns to lobby`, async () => {
    let f, session, socket, code;
    const fromDirectory = origin !== 'direct';
    if (fromDirectory) {
      ({ f, session } = await liveActualDirectory());
      await click(origin === 'directory create' ? '#createButton' : '#codeButton');
      if (origin === 'directory join')
        await React.act(async () =>
          session.directory.updateForm({
            code: '5678',
          }),
        );
      await click('#submitRoom');
      await until(
        () => f.sockets.some((value) => new URL(value.url).searchParams.has('lobby')),
        'directory action enters one room',
      );
      socket = f.sockets.find((value) => new URL(value.url).searchParams.has('lobby'));
      code = session.room.service.getSnapshot().room.code;
      await React.act(async () => {
        socket.open();
        socket.message({
          type: 'state',
          roomDirectory: {
            version: 1,
            controlModes: true,
          },
          room: {
            code,
            playerCount: 2,
            difficulty: 1,
            phase: 'lobby',
            inputDelay: 0,
            predictionLimit: 8,
            startSerial: 0,
            seats: [
              {
                clientId: new URL(socket.url).searchParams.get('lobby'),
                name: 'A',
                loadout: 0,
                ready: false,
              },
              null,
            ],
            spectators: [],
          },
        });
      });
      await until(
        () => session.room.service.getSnapshot().preparation?.status === 'ready',
        'room resources prepared',
      );
    } else {
      ({ f, session, socket } = await actualTerminalRoom({
        fromDirectory: false,
      }));
      code = '1234';
    }
    const frame = node('#gameFrame'),
      savedRoom = session.room.service.getSnapshot().room;
    await click('[data-mp-seat="0"] .mp-seat-edit');
    await click('#mpCheckGame');
    await until(
      () => session.getRuntime().getSnapshot().launched,
      'actual multiplayer preflight Runtime launched',
    );
    const socketCount = f.sockets.length,
      sentCount = socket.sent.length;
    await React.act(async () =>
      f.native.emit('exit', {
        status: 'success',
      }),
    );
    await until(() => !playerOpen(), 'authenticated Runtime Exit closes Player');
    const invite = api.resolveRoomInvite(new URL(currentUrl(), env.window.location.href));
    assert.equal(invite.g, 'th06mp');
    assert.equal(invite.r, code);
    assert.equal(
      invite.f === true,
      fromDirectory,
      'authorized correction: same-room native return retains directory origin',
    );
    assert.equal(invite.a, undefined, 'native return never repeats create/join');
    for (const key of ['p', 'd', 'v']) assert.equal(invite[key], undefined);
    assert.equal(invite.c, false);
    assert.equal(session.room.service.getSnapshot().room.code, savedRoom.code);
    assert.equal(session.room.service.getSnapshot().fromDirectory, fromDirectory);
    assert.equal(f.sockets.length, socketCount);
    assert.equal(socket.sent.length, sentCount);
    assert.equal(node('#gameFrame'), frame);
    assert.equal(session.getRuntime().getSnapshot().launched, false);
    await click('#mpLeaveRoom');
    await until(
      () => session.room.service.getSnapshot().room === null,
      'subsequent Leave clears membership',
    );
    assert.equal(
      currentUrl(),
      '/lobby.html?game=th06mp',
      'every room leaves to its fixed lobby parent',
    );
    assert.equal(
      f.sockets.filter((value) => new URL(value.url).searchParams.has('lobby')).length,
      1,
    );
  });

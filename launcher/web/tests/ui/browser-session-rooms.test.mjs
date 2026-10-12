import {
  NativeFile,
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
  env,
  importPackageFile,
  mountBrowser,
  node,
  playerOpen,
  siteManifest,
  subsetHost,
  test,
  tick,
  until,
  visibleOptions,
} from './browser-session-fixture.mjs';

for (const room of [false, true])
  test(`actual BrowserLauncher: same-base missing MP Runtime ${room ? 'retains the restored room' : 'hides options but retains catalog content'}`, async () => {
    const initial = `/?game=th06mp${room ? '&mpRoom=1234' : ''}&keep=a%20b#retained`;
    const f = await mountBrowser({
      initial,
      holdHost: true,
      host: subsetHost(['th06'], {
        multiplayer: false,
      }),
    });
    const frame = node('#gameFrame');
    if (room)
      await until(() => !!env.document.querySelector('#mpRoomCode'), 'room restored before Host');
    else assert.ok(visibleOptions());
    const code = env.document.querySelector('#mpRoomCode'),
      address = currentUrl(),
      routes = f.routeEvents.length;
    if (room) {
      assert.equal(code.textContent, '1234');
      assert.deepEqual(
        f.warnings,
        [],
        'main app7604–7618 must seed direct-room history after Router activation',
      );
      assert.equal(
        f.router.state.location.state?.eaglerTouhouMpRoom,
        '1234',
        'main direct-room history marker is committed before Host',
      );
    }
    await acceptHeldHost(f);
    assert.equal(visibleOptions(), null);
    assert.equal(
      node('#gameId').textContent,
      'TH06 MP',
      'removed card does not substitute a different retained panel',
    );
    assert.equal(currentUrl(), address);
    assert.equal(f.routeEvents.length, routes);
    assert.equal(node('#gameFrame'), frame);
    assert.deepEqual(f.navigations, []);
    assert.equal(playerOpen(), false);
    if (room) {
      assert.equal(node('#mpRoomCode'), code);
      assert.equal(code.textContent, '1234');
      assert.equal(node('#mpRoomView').hidden, false);
      assert.equal(env.document.body.classList.contains('mp-room-active'), true);
    }
  });

for (const game of ['th06', 'th07'])
  test(`actual BrowserLauncher: ${game} MP stale/current native Exit preserves the canonical room, then room Back leaves once`, async () => {
    const product = `${game}mp`,
      host = subsetHost([game]);
    host.shared.netplayRelay = 'wss://relay.invalid/socket';
    host.games[game].multiplayerRuntime =
      `runtime/${game}/multiplayer/${game}.html?hosted=1&v=test-multiplayer`;
    const f = await mountBrowser({
      initial: `/?game=${product}&keep=a%20b`,
      host,
      socketBoundary: true,
      seedPackages: [game],
    });
    const frame = node('#gameFrame');
    // Original test-mp-runtime-exit-room.py152–159 expects an old visible home
    // card/online-fold route that final main no longer exposes. This fixture
    // enters direct MP options and invokes the retained native DOM action only;
    // it does not certify that superseded original browser setup as passing.
    assert.equal(node('#mpCreateRoom').disabled, false);
    await click('#mpCreateRoom');
    await until(
      () => f.sockets.length === 1 && !!env.document.querySelector('#mpRoomCode'),
      'actual room owner creates its lobby transport',
    );
    const socket = f.sockets[0],
      endpoint = new URL(socket.url),
      code = node('#mpRoomCode').textContent;
    assert.equal(endpoint.searchParams.get('room'), `${product}-${code}`);
    const localClientId = endpoint.searchParams.get('lobby');
    assert.ok(localClientId);
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
              clientId: localClientId,
              name: 'A',
              loadout: 0,
              ready: false,
              offline: false,
            },
            null,
          ],
          spectators: [],
        },
      });
    });
    assert.equal(node('#mpLocalPlayer').hidden, false);
    assert.equal(socket.sent.filter((message) => message.type === 'take-seat').length, 1);
    const roomElement = node('#mpRoomView');
    await click('[data-mp-seat="0"] .mp-seat-edit');
    await until(() => node('#mpRoomPanel').open, 'actual personal panel opens');
    assert.equal(node('#mpCheckGame').disabled, false, 'original MP Exit171');
    await click('#mpCheckGame');
    await until(
      () => commands().includes('launch'),
      'canonical preparation and actual Runtime preflight reach launch ACK',
    );
    const configure = f.outbound.find((message) => message.command === 'configure');
    assert.equal(
      configure.options.multiplayerPreflight === true,
      false,
      'original181–183: TH06/TH07 omit TH08-only preflight role',
    );
    assert.equal(
      !!configure.options.netplayMode,
      false,
      'original184–186: game check cannot join gameplay transport',
    );
    assert.match(f.native.location.href, new RegExp(`/runtime/${game}/multiplayer/${game}\\.html`));
    assert.match(f.native.location.href, /runtimeVariant=multiplayer/);
    assert.deepEqual(
      f.dataReads,
      [[1, 2, 3]],
      'actual managed preparation reads canonical installed DATA',
    );
    const epoch = f.native.envelope.epoch,
      documentBefore = f.native.document;
    await React.act(async () => {
      f.native.emit('exit', {
        epoch: epoch - 1,
        status: 'error',
      });
    });
    await tick();
    assert.equal(playerOpen(), true, 'original188–190: stale native exit is ignored');
    assert.equal(f.native.document, documentBefore);
    assert.equal(node('#gameFrame'), frame);
    const roomKey = `eagler-touhou-${product}-room-v1`,
      removals = f.sessionStorage.removals.filter((key) => key === roomKey).length;
    await React.act(async () => {
      f.native.emit('exit', {
        status: 'error',
      });
    });
    await until(() => !playerOpen(), 'current authenticated native error Exit closes Player');
    assert.equal(
      commands().includes('sync'),
      false,
      'main5502–5506 native Exit has no surviving save receiver',
    );
    assert.equal(node('#decisionDialog').open, false);
    assert.equal(node('#mpRoomView').hidden, false);
    assert.equal(node('#mpRoomCode').textContent, code);
    assert.equal(node('#mpLocalPlayer').hidden, false);
    assert.equal(node('#gameId').textContent, `${game.toUpperCase()} MP`);
    // Original201 requires has-selection, but pinned main4569/4593 expressly
    // removes it for index349's library-layout. Keep that original assertion
    // unchanged and unresolved for paired browser execution; do not add a class
    // to production merely to satisfy its obsolete presentation premise.
    assert.equal(
      node('#main').classList.contains('has-selection'),
      false,
      'pinned main library-layout presentation',
    );
    assert.equal(env.document.body.classList.contains('mp-room-active'), true);
    // Original202–203 assert obsolete raw game/mpRoom query keys. Keep those
    // original bytes unchanged; here read the actual main token via its parser.
    const invite = api.resolveRoomInvite(new URL(currentUrl(), env.window.location.href));
    assert.equal(invite?.g, product);
    assert.equal(invite?.r, code);
    assert.equal(
      JSON.parse(f.sessionStorage.getItem(roomKey)).room.code,
      code,
      'original204–210 retains room session',
    );
    assert.equal(f.sessionStorage.removals.filter((key) => key === roomKey).length, removals);
    assert.equal(socket.closes.length, 0);
    assert.equal(f.sockets.length, 1);
    assert.equal(node('#gameFrame'), frame);

    // Preserve the original212–213 personal-panel layer before exercising the
    // next system Back as a supplemental room-leave variant of original214–216.
    if (node('#mpRoomPanel').open) await click('#mpRoomPanelClose');
    await until(() => !node('#mpRoomPanel').open, 'only the personal panel closes');
    assert.equal(socket.closes.length, 0);
    assert.equal(node('#mpRoomCode').textContent, code);
    await back();
    await until(
      () => api.resolveRoomInvite(new URL(currentUrl(), env.window.location.href)) === null,
      'next Back clears the room route',
    );
    assert.equal(api.resolveRoomInvite(new URL(currentUrl(), env.window.location.href)), null);
    assert.equal(f.sessionStorage.getItem(roomKey), null);
    assert.equal(f.sessionStorage.removals.filter((key) => key === roomKey).length, removals + 1);
    assert.deepEqual(socket.closes, [
      {
        code: 1000,
        reason: 'leave room',
      },
    ]);
    assert.equal(f.sockets.length, 1);
    assert.equal(node('#gameFrame'), frame);
    assert.equal(commands().includes('sync'), false);
    assert.ok(
      !env.document.querySelector('#mpRoomView') || node('#mpRoomView').hidden,
      `original215: room is hidden after leave; DOM=${JSON.stringify({
        count: env.document.querySelectorAll('#mpRoomView').length,
        sameNode: env.document.querySelector('#mpRoomView') === roomElement,
        connected: roomElement.isConnected,
        hidden: roomElement.hidden,
        parent: roomElement.parentElement?.id,
        parentClass: roomElement.parentElement?.className,
        roomActive: env.document.body.classList.contains('mp-room-active'),
        saved: f.sessionStorage.getItem(roomKey),
        closes: socket.closes,
      })}`,
    );
    await click('#mpCreateRoom');
    await until(
      () => f.sockets.length === 2 && !!env.document.querySelector('#mpRoomCode'),
      'new room can enter after actual leave',
    );
    const nextSocket = f.sockets[1],
      nextCode = node('#mpRoomCode').textContent;
    await React.act(async () => {
      nextSocket.open();
      nextSocket.message({
        type: 'state',
        roomDirectory: {
          version: 1,
          controlModes: true,
        },
        room: {
          code: nextCode,
          playerCount: 2,
          difficulty: 1,
          phase: 'lobby',
          inputDelay: 0,
          predictionLimit: 8,
          startSerial: 0,
          seats: [
            {
              clientId: new URL(nextSocket.url).searchParams.get('lobby'),
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
    assert.equal(env.document.querySelectorAll('#mpRoomView').length, 1);
    assert.notEqual(node('#mpRoomView'), roomElement);
    assert.equal(roomElement.isConnected, false);
    assert.equal(node('#mpRoomView').hidden, false);
    assert.equal(node('#mpLocalPlayer').hidden, false);
    assert.equal(JSON.parse(f.sessionStorage.getItem(roomKey)).room.code, nextCode);
    assert.equal(socket.closes.length, 1);
    assert.equal(nextSocket.closes.length, 0);
    assert.equal(node('#gameFrame'), frame);
  });

for (const locale of ['en', 'zh-CN'])
  test(`actual BrowserLauncher: room Cancel/Retry owns manual recovery and preserves ${locale} main copy`, async () => {
    const releases = [];
    api.setPackageReadGate(
      () =>
        new Promise((resolve) => {
          releases.push(resolve);
        }),
    );
    const host = siteManifest();
    host.shared.netplayRelay = 'wss://relay.invalid/socket';
    host.shared.gameDataFallback = {
      url: 'https://downloads.invalid/package.zip',
      hint: 'original hint',
    };
    const f = await mountBrowser({
      initial: '/?game=th06mp&mpRoom=1234',
      host,
      socketBoundary: true,
      seedStorage(storage) {
        storage.values.set('eagler-touhou-ui-locale-v1', locale);
      },
    });
    const session = api.getTestSession();
    await React.act(async () => {
      session.setLocale(locale);
    });
    await until(
      () => releases.length === 1 && !node('#mpRoomResourceCancel').hidden,
      'canonical room preparation waits on its first Package Store read',
    );
    await click('#mpRoomResourceCancel');
    // Pinned app6824 calls beginManualGamePackageImport's cancelled-download
    // default (app3377 / i18n346–347), not the generic Import button's intro.
    const reason =
      locale === 'en'
        ? 'Game-data download from the server was cancelled.\nChoose a local game package to import. If the server provides a fallback download address, you can also use Open link to get the package.'
        : '已取消从服务器下载游戏资源。\n请选择本地游戏包导入；如果服务器提供了备用下载地址，也可以点击「打开链接」取得游戏包。';
    assert.equal(node('#gameDataImportReason').textContent, reason);
    assert.equal(node('#mpRoomResourceProgress').dataset.status, 'cancelled');
    assert.equal(session.gameData.getSnapshot().attempt.continuation.kind, 'install-only');
    await click('#transferDownload');
    assert.equal(node('#gameDataLinkWindow').hidden, false);
    const retryStates = [],
      unsubscribe = session.room.service.subscribe(() => {
        if (session.room.service.getSnapshot().preparation?.status === 'preparing')
          retryStates.push(session.gameData.getSnapshot().attempt);
      });
    await click('#mpRoomResourceRetry');
    await until(() => releases.length === 2, 'accepted Retry starts a new canonical preparation');
    unsubscribe();
    assert.ok(retryStates.length);
    assert.ok(
      retryStates.every((attempt) => attempt === null),
      'main6830 clears recovery before publishing the replacement preparation',
    );
    assert.equal(session.gameData.getSnapshot().attempt, null);
    assert.equal(node('#gameDataImportWindow').hidden, true);
    assert.equal(node('#gameDataLinkWindow').hidden, true);
    await React.act(async () => {
      releases[0]();
    });
    await tick();
    assert.equal(
      node('#mpRoomResourceProgress').dataset.status,
      'preparing',
      'old cancelled read cannot settle the replacement',
    );
    assert.equal(session.gameData.getSnapshot().attempt, null);
    await React.act(async () => {
      session.gameData.openManual();
    });
    const manual = session.gameData.getSnapshot().attempt;
    await React.act(async () => {
      session.room.service.retryPreparation();
    });
    assert.deepEqual(
      session.gameData.getSnapshot().attempt,
      manual,
      'ineligible Retry while preparing cannot clear a valid manual window',
    );
    assert.equal(node('#gameDataImportWindow').hidden, false);
    assert.equal(releases.length, 2);
    await click('#mpRoomResourceCancel');
    assert.equal(node('#gameDataImportReason').textContent, reason);
    await React.act(async () => {
      session.gameData.beginManual({
        kind: 'launch',
      });
    });
    const launchAttempt = session.gameData.getSnapshot().attempt;
    await click('#mpRoomResourceRetry');
    await until(() => releases.length === 3, 'second accepted Retry starts its own preparation');
    assert.deepEqual(
      session.gameData.getSnapshot().attempt,
      launchAttempt,
      'main6830 does not clear a launch continuation',
    );
    assert.equal(node('#gameDataImportWindow').hidden, false);
    assert.deepEqual(f.navigations, [], 'resource preparation never launches or replaces Runtime');
    await React.act(async () => {
      session.room.service.leave();
      for (const release of releases) release();
    });
  });

for (const status of ['ready', 'preparing', 'cancelled', 'failed'])
  for (const outcome of ['success', 'failure']) {
    test(`actual BrowserLauncher: manual package ${outcome} preserves pinned ${status} room eligibility`, async () => {
      let releaseRead,
        reads = 0;
      if (status === 'preparing' || status === 'cancelled') {
        const pending = new Promise((resolve) => {
          releaseRead = resolve;
        });
        api.setPackageReadGate(() => (++reads === 1 ? pending : null));
      }
      const host = siteManifest();
      host.shared.netplayRelay = 'wss://relay.invalid/socket';
      const f = await mountBrowser({
        initial: '/en.html?game=th06mp&mpRoom=1234',
        host,
        socketBoundary: true,
        seedPackages: status === 'ready' ? ['th06'] : [],
      });
      const session = api.getTestSession();
      await until(
        () =>
          session.room.service.getSnapshot().preparation?.status ===
          (status === 'cancelled' ? 'preparing' : status),
        'canonical room preparation reaches the test state',
      );
      if (status === 'cancelled') await click('#mpRoomResourceCancel');
      await React.act(async () => {
        session.gameData.openManual();
      });
      const events = [],
        unsubscribe = session.room.service.subscribe(() =>
          events.push(session.room.service.getSnapshot().preparation?.status),
        );
      const eligible = ['cancelled', 'failed'].includes(status);
      let importing;
      await React.act(async () => {
        importing = session.gameData.importFile(
          outcome === 'success'
            ? importPackageFile()
            : new NativeFile(['invalid fixture ZIP'], 'bad.zip'),
        );
        assert.equal(session.gameData.getSnapshot().busy, true);
        assert.equal(
          session.room.service.getSnapshot().preparation.status,
          eligible ? 'importing' : status,
        );
        if (eligible) {
          const attempt = session.gameData.getSnapshot().attempt;
          session.room.service.retryPreparation();
          assert.deepEqual(
            session.gameData.getSnapshot().attempt,
            attempt,
            'ineligible Retry during import cannot clear the busy operation',
          );
        }
      });
      await React.act(async () => {
        await importing;
      });
      if (outcome === 'success') {
        if (eligible)
          await until(
            () => session.room.service.getSnapshot().preparation?.status === 'ready',
            'interrupted room resumes real preparation against the committed package',
          );
        assert.equal(
          node('#status').textContent,
          eligible
            ? 'Game package imported; continuing the previous action…'
            : 'Game package imported; the game is ready to start',
        );
        assert.equal(session.gameData.getSnapshot().attempt, null);
        assert.equal(node('#gameDataImportWindow').hidden, true);
      } else {
        assert.equal(session.gameData.getSnapshot().busy, false);
        assert.equal(node('#gameDataImportWindow').hidden, false);
        assert.match(node('#gameDataImportReason').textContent, /ZIP/);
        assert.equal(
          session.room.service.getSnapshot().preparation.status,
          eligible ? 'cancelled' : status,
        );
      }
      if (!eligible) {
        assert.ok(
          events.every((value) => value === status),
          'ordinary manual import does not publish room importing/cancelled/preparing transitions',
        );
        assert.equal(session.room.service.getSnapshot().preparation.status, status);
      }
      unsubscribe();
      assert.deepEqual(f.navigations, [], 'manual install-only recovery does not start Runtime');
      await React.act(async () => {
        session.room.service.leave();
        releaseRead?.();
      });
    });
  }

test('actual BrowserLauncher terminal blocked return-message storage preserves pinned departure and no toast', async () => {
  const { f, session, socket } = await actualTerminalRoom({
    fromDirectory: true,
  });
  const feedback = captureTerminalFeedback(session),
    writesBefore = f.sessionStorage.writes.length;
  f.sessionStorage.blocked = true;
  await React.act(async () => {
    socket.close(4004);
  });
  await until(
    () =>
      currentUrl().startsWith('/lobby.html') && session.room.service.getSnapshot().room === null,
    'storage denial cannot prevent terminal departure',
  );
  feedback.unsubscribe();
  assert.equal(session.directory.getSnapshot().notice, '');
  assert.equal(node('#notice').hidden, true);
  f.sessionStorage.blocked = false;
});

test('actual BrowserLauncher: authenticated native Return to Menu closes the Player and leaves its multiplayer room', async () => {
  const { f, session } = await actualTerminalRoom({
    fromDirectory: false,
  });
  await click('[data-mp-seat="0"] .mp-seat-edit');
  await click('#mpCheckGame');
  await until(
    () => session.getRuntime().getSnapshot().launched,
    'native multiplayer preflight launched',
  );
  await React.act(async () =>
    f.native.emit('exit', {
      status: 'success',
      returnToMenu: true,
    }),
  );
  await until(
    () => !playerOpen() && session.room.service.getSnapshot().room === null,
    'explicit native menu return retires game and membership',
  );
  assert.equal(currentUrl(), '/lobby.html?game=th06mp');
});

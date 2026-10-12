import {
  NativeFile,
  React,
  api,
  assert,
  back,
  click,
  commands,
  currentUrl,
  developmentAcquisitionHost,
  heldPackageFetch,
  mountBrowser,
  node,
  playerOpen,
  start,
  subsetHost,
  test,
  tick,
  until,
} from './browser-session-fixture.mjs';

test('actual BrowserLauncher: failed native sync offers Stay and retry without retiring the save writer', async () => {
  const f = await mountBrowser();
  await start();
  await until(() => commands().includes('launch'), 'launch ACK');
  const source = f.native.location.href,
    frame = node('#gameFrame'),
    previous = currentUrl();
  f.syncMode = 'fail';
  await back();
  await until(() => node('#decisionDialog').open, 'original save-failure decision appears');
  assert.match(node('#decisionMessage').textContent, /Synthetic native sync failure/);
  assert.equal(node('#decisionCancel').textContent, 'Stay in game');
  await click('#decisionCancel');
  assert.equal(playerOpen(), true);
  assert.equal(currentUrl(), previous);
  assert.equal(f.native.location.href, source);
  assert.equal(node('#gameFrame'), frame);
  f.syncMode = 'auto';
  await back();
  await until(() => !playerOpen(), 'retry sync allows navigation');
  assert.equal(commands().filter((command) => command === 'sync').length, 2);
  assert.equal(node('#gameFrame'), frame);
});

test('actual BrowserLauncher: cold save export owns file-only preparation and retires its temporary native writer', async () => {
  const f = await mountBrowser(),
    frame = node('#gameFrame'),
    previous = currentUrl();
  await click('[data-action="export-save"]');
  await until(
    () => f.downloads.length === 1 && f.navigations.at(-1) === 'about:blank',
    'cold export downloaded and released prepared owner',
  );
  assert.equal(f.downloads[0].name, 'score.dat');
  assert.equal(commands().includes('read'), true);
  assert.equal(commands().includes('sync'), true);
  assert.equal(commands().includes('configure'), false);
  assert.equal(commands().includes('launch'), false);
  assert.equal(playerOpen(), false);
  assert.equal(currentUrl(), previous);
  assert.equal(node('#gameFrame'), frame);
  assert.equal(
    node('#launch').disabled,
    false,
    'launcher activity completes after cold-owner cleanup',
  );
});

// Main app6320–6399: only successful import completion hides Player. Earlier
// write/reopen/verification failures and picker cancellation keep its visibility.
// These use the real mounted BrowserSession; only native file replies are faked.

for (const oldResult of ['success', 'failure', 'native-error'])
  test(`actual BrowserLauncher: old file preparation ${oldResult} preserves a newer epoch-null TH10 acquisition and its Cancel receiver`, async () => {
    const edge = heldPackageFetch(),
      host = developmentAcquisitionHost(['th06', 'th10']);
    const f = await mountBrowser({
        initial: '/?game=th10&keep=a%20b',
        host,
        fetchBoundary: edge.fetchBoundary,
        seedStorage(storage) {
          storage.values.set(
            api.gamePreferenceStorageKey('th10'),
            JSON.stringify({
              music: 'none',
              options: {
                touchEnabled: false,
                thpracEnabled: false,
              },
            }),
          );
        },
      }),
      session = api.getTestSession();
    let releaseRead,
      rejectRead,
      held = false,
      imported,
      outcome,
      unsubscribe;
    const readGate = new Promise((resolve, reject) => {
      releaseRead = resolve;
      rejectRead = reject;
    });
    api.setPackageReadGate((game) => {
      if (game === 'th06' && !held) {
        held = true;
        return readGate;
      }
    });
    const transfers = [];
    try {
      await React.act(async () => {
        imported = session
          .importFile('replay', new NativeFile([new Uint8Array([1])], 'th6_01.rpy'), 'th06')
          .then(
            () => {
              outcome = 'success';
            },
            (error) => {
              outcome = error.message;
            },
          );
      });
      await until(() => held, 'old Replay import waits outside native ownership');
      await start();
      await React.act(async () => {
        await edge.waitForRequest('th10');
      });
      await until(
        () => edge.requests.has('th10') && !session.transfer.getSnapshot().hidden,
        'new Start owns real package acquisition',
      );
      assert.equal(session.getRuntime().getSnapshot().epoch, null);
      const before = session.transfer.getSnapshot();
      assert.equal(before.label, 'TH10 fixture-th10.data');
      assert.equal(before.cancelVisible, true);
      assert.equal(node('#transferCancel').hidden, false);
      unsubscribe = session.transfer.subscribe(() =>
        transfers.push(session.transfer.getSnapshot()),
      );
      f.autoReady = false;
      await React.act(async () => {
        if (oldResult === 'failure') rejectRead(new Error('Held old acquisition failure'));
        else releaseRead();
      });
      if (oldResult !== 'failure') {
        await until(
          () => session.getRuntime().getSnapshot().phase === 'loading',
          'old task acquires only its file-only Runtime',
        );
        assert.equal(session.getRuntime().getSnapshot().fileOnly, true);
        await React.act(async () => {
          f.native.emit('transfer', {
            kind: 'game',
            mode: 'base',
            loaded: 1,
            total: 3,
          });
        });
        assert.equal(
          session.getRuntime().getSnapshot().progress.loaded,
          1,
          'genuine native file progress still reaches the canonical Runtime owner',
        );
        await React.act(async () => {
          if (oldResult === 'native-error')
            f.native.emit('error', {
              error: 'Held file native failure',
            });
          else f.native.emit('ready');
        });
      }
      await React.act(async () => {
        await imported;
      });
      await tick();
      assert.equal(
        outcome,
        oldResult === 'success'
          ? 'success'
          : oldResult === 'native-error'
            ? 'Held file native failure'
            : 'Held old acquisition failure',
      );
      assert.deepEqual(
        session.transfer.getSnapshot(),
        before,
        'old progress/plan/native ready/failure cleanup must preserve newer transfer presentation',
      );
      assert.deepEqual(
        transfers,
        [],
        'no transient stale TH06 transfer label or cancellation publication',
      );
      assert.equal(session.getRuntime().getSnapshot().epoch, null);
      assert.equal(playerOpen(), true);
      assert.equal(
        commands().filter((value) => value === 'write').length,
        oldResult === 'success' ? 1 : 0,
        'background file work keeps its authentic outcome',
      );
      assert.equal(
        edge.requests.get('th10').released,
        false,
        'newer acquisition stays held until the actual Cancel click',
      );
      assert.equal(edge.requests.get('th10').signal.aborted, false);
      await click('#transferCancel');
      await until(
        () => edge.requests.get('th10').signal.aborted,
        'the visible Cancel reaches the newer TH10 acquisition',
      );
      assert.equal(node('#transferCancel').hidden, true);
    } finally {
      unsubscribe?.();
      releaseRead();
      api.setPackageReadGate(null);
      edge.releaseAll();
      if (imported)
        await React.act(async () => {
          await imported;
        });
      await tick();
    }
  });

for (const startState of ['none', 'warning', 'accepted'])
  test(
    `actual BrowserLauncher: file acquisition Cancel with ${startState} newer Start targets its visible owner`,
    {
      timeout: 30_000,
    },
    async () => {
      const edge = heldPackageFetch(),
        host = developmentAcquisitionHost(['th10', 'th11']);
      await mountBrowser({
        initial: '/?game=th11&keep=a%20b',
        host,
        fetchBoundary: edge.fetchBoundary,
        seedStorage(storage) {
          for (const game of ['th10', 'th11'])
            storage.values.set(
              api.gamePreferenceStorageKey(game),
              JSON.stringify({
                music: 'none',
                options: {
                  touchEnabled: false,
                  thpracEnabled: false,
                },
              }),
            );
        },
      });
      const session = api.getTestSession(),
        newerStart = startState === 'accepted';
      let preparing, imported, outcome;
      try {
        await React.act(async () => {
          preparing = session.importFile(
            'replay',
            new NativeFile([new Uint8Array([1])], 'th10_01.rpy'),
            'th10',
          );
          imported = preparing.then(
            () => {
              outcome = 'success';
            },
            (error) => {
              outcome = error.name;
            },
          );
        });
        await React.act(async () => {
          await edge.waitForRequest('th10', preparing);
        });
        await until(
          () => !node('#transferCancel').hidden,
          'file task owns an actual cancellable package acquisition',
        );
        if (newerStart) {
          await start();
          await React.act(async () => {
            await edge.waitForRequest('th11');
          });
          await until(
            () => session.transfer.getSnapshot().label.startsWith('TH11 '),
            'new Start owns the visible network request',
          );
        }
        if (startState === 'warning') {
          await click('#launch');
          assert.equal(node('#decisionDialog').open, true);
          await click('#decisionCancel');
        }
        assert.equal(session.getRuntime().getSnapshot().epoch, null);
        assert.equal(node('#transferCancel').hidden, false);
        const cancelledGame = newerStart ? 'th11' : 'th10';
        assert.equal(
          edge.requests.get(cancelledGame).released,
          false,
          'the visible acquisition remains held until the actual Cancel click',
        );
        assert.equal(edge.requests.get(cancelledGame).signal.aborted, false);
        await click('#transferCancel');
        await until(
          () => edge.requests.get(cancelledGame).signal.aborted,
          'visible cancellation reaches its real fetch signal',
        );
        assert.equal(
          edge.requests.get('th10').signal.aborted,
          !newerStart,
          'hidden older file acquisition must not receive a newer launch Cancel',
        );
        if (newerStart) edge.requests.get('th10').release();
        await React.act(async () => {
          await imported;
        });
        assert.equal(outcome, newerStart ? 'success' : 'AbortError');
        assert.equal(commands().filter((value) => value === 'write').length, newerStart ? 1 : 0);
      } finally {
        edge.releaseAll();
        if (imported)
          await React.act(async () => {
            await imported;
          });
        await tick();
      }
    },
  );

for (const newerStart of [false, true])
  test(`actual BrowserLauncher: authenticated file-only exit ${newerStart ? 'preserves newer TH10 acquisition' : 'closes its current visible Player'}`, async () => {
    const edge = heldPackageFetch();
    const f = await mountBrowser({
        initial: newerStart ? '/?game=th10' : '/?game=th06&preview=touch',
        autoReady: false,
        host: developmentAcquisitionHost(['th06', 'th10']),
        fetchBoundary: edge.fetchBoundary,
        seedStorage(storage) {
          storage.values.set(
            api.gamePreferenceStorageKey('th10'),
            JSON.stringify({
              music: 'none',
              options: {
                touchEnabled: false,
                thpracEnabled: false,
              },
            }),
          );
        },
      }),
      session = api.getTestSession();
    let releaseRead,
      held = false,
      imported,
      outcome;
    const readGate = new Promise((resolve) => {
      releaseRead = resolve;
    });
    api.setPackageReadGate((game) => {
      if (game === 'th06' && !held) {
        held = true;
        return readGate;
      }
    });
    try {
      await React.act(async () => {
        imported = session
          .importFile('replay', new NativeFile([new Uint8Array([1])], 'th6_01.rpy'), 'th06')
          .then(
            () => {
              outcome = 'success';
            },
            (error) => {
              outcome = error;
            },
          );
      });
      await until(() => held, 'file preparation paused before native ownership');
      if (newerStart) {
        await start();
        await until(
          () => edge.requests.has('th10') && !session.transfer.getSnapshot().hidden,
          'newer acquisition owns transfer',
        );
      }
      assert.equal(playerOpen(), true);
      const transfer = session.transfer.getSnapshot();
      await React.act(async () => {
        releaseRead();
      });
      await until(
        () => session.getRuntime().getSnapshot().phase === 'loading',
        'file-only carrier begins',
      );
      assert.equal(session.getRuntime().getSnapshot().fileOnly, true);
      const epoch = session.getRuntime().getSnapshot().epoch,
        delivered = [];
      const unsubscribe = session
        .getRuntime()
        .subscribeEvents((message) => delivered.push(message));
      await React.act(async () => {
        f.native.emit('exit', {
          status: 'success',
          code: 0,
        });
        await imported;
      });
      await tick();
      unsubscribe();
      assert.equal(outcome.name, 'AbortError');
      assert.equal(outcome.message, 'EAGLER_RUNTIME_SESSION_SUPERSEDED');
      assert.equal(delivered.at(-1).event, 'exit');
      assert.equal(delivered.at(-1).epoch, epoch, 'genuine native exit remains delivered');
      assert.equal(session.getRuntime().getSnapshot().epoch, null);
      assert.equal(session.getRuntime().getSnapshot().fileOnly, false);
      assert.equal(
        commands().includes('write'),
        false,
        'terminal file task never writes or falsely reports success',
      );
      assert.equal(playerOpen(), newerStart);
      if (newerStart) {
        assert.equal(
          edge.requests.get('th10').signal.aborted,
          false,
          'old file exit cannot cancel newer acquisition',
        );
        assert.deepEqual(
          session.transfer.getSnapshot(),
          transfer,
          'old file exit cannot replace, hide or cancel newer transfer',
        );
        f.autoReady = true;
        await React.act(async () => {
          edge.requests.get('th10').release();
        });
        await until(
          () => session.getRuntime().getSnapshot().launched,
          'preserved TH10 acquisition finishes and launches',
        );
        assert.equal(session.getRuntime().getSnapshot().game, 'th10');
        assert.equal(playerOpen(), true);
      }
    } finally {
      releaseRead();
      api.setPackageReadGate(null);
      edge.releaseAll();
      if (imported)
        await React.act(async () => {
          await imported;
        });
      await tick();
    }
  });

for (const game of ['th06', 'th07', 'th08', 'th09', 'th10'])
  test(`actual BrowserLauncher: original ${game} multiplayer Replay Runtime path and configure contract`, async () => {
    const product = `${game}mp`,
      host = subsetHost([game]),
      runtimePath = `runtime/${game}/multiplayer/${game}.html`;
    host.games[game].multiplayerRuntime = `${runtimePath}?hosted=1&v=test-multiplayer`;
    // Same explicit setup boundary as above: the old home-card activation is
    // baseline-conflicted, so it is not counted as original browser acceptance.
    const f = await mountBrowser({
      initial: `/?game=${product}&keep=a%20b`,
      host,
      seedPackages: [game],
    });
    const frame = node('#gameFrame');
    await click('#mpReplayViewer');
    await until(
      () => commands().includes('launch'),
      'actual Replay action configures and launches the multiplayer Runtime',
    );
    const source = f.native.location.href,
      configure = f.outbound.find((message) => message.command === 'configure');
    // Original test-multiplayer-replay-launcher-browser.py170–174, observing
    // the actual navigated document rather than Runtime model bookkeeping.
    assert.ok(source.includes(`/${runtimePath}`), source);
    assert.ok(source.includes('runtimeVariant=multiplayer'), source);
    assert.ok(configure);
    assert.equal(configure.options.replayViewer, true);
    assert.equal(
      Object.keys(configure.options).some((key) => key.startsWith('netplay')),
      false,
    );
    assert.equal(playerOpen(), true);
    assert.equal(node('#gameFrame'), frame);
    assert.deepEqual(f.dataReads, [[1, 2, 3]]);
    assert.equal(f.sockets.length, 0, 'Replay viewer never creates room/gameplay transport');
    await back();
    await until(() => !playerOpen(), 'Replay viewer closes through its actual Runtime owner');
    assert.equal(node('#gameFrame'), frame);
  });

// Main1499–1521 may replace business selection while Start's existing input
// warning is open. Main9560–9569 then opens/starts the current selected game.

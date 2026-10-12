import {
  NativeFile,
  React,
  acceptHeldHost,
  api,
  assert,
  back,
  capturePreparationTimers,
  chooseStorageFile,
  click,
  commands,
  currentUrl,
  developmentAcquisitionHost,
  env,
  heldPackageFetch,
  importPackageFile,
  mountBrowser,
  node,
  openStoragePicker,
  playerOpen,
  runningStorageImport,
  selectImport,
  start,
  storageImportCases,
  subsetHost,
  test,
  tick,
  until,
} from './browser-session-fixture.mjs';

for (const mode of [
  'background-live',
  'background-retired',
  'foreground-failure',
  'foreground-cancel',
])
  test(`actual BrowserLauncher: optional package update ${mode} retains pinned feedback and installed generation`, async () => {
    let rejectDescriptor, requestSignal;
    const descriptor = new Promise((_resolve, reject) => {
      rejectDescriptor = reject;
    });
    void descriptor.catch(() => {});
    const catalog = {
      schema: 'eagler-touhou/release-catalog/1',
      games: {
        th06: {
          revision: 'published-next',
          descriptor: 'th06.package.json',
        },
      },
    };
    const f = await mountBrowser({
        seedPackages: ['th06'],
        fetchBoundary(url, init) {
          if (url.pathname.endsWith('/release-catalog.json')) return Response.json(catalog);
          if (url.pathname.endsWith('/th06.package.json')) {
            requestSignal = init.signal;
            return descriptor;
          }
        },
      }),
      session = api.getTestSession(),
      frame = node('#gameFrame');
    let installed;
    await React.act(async () => {
      installed = await api.readCurrentPackageGeneration('th06');
    });
    try {
      await start();
      await until(
        () => node('#decisionDialog').open && !node('#decisionSecondary').hidden,
        'original optional-update decision appears',
      );
      assert.equal(
        node('#decisionMessage').textContent,
        'A newer game package is available from the server. Your imported local version can still be started directly.',
      );
      assert.equal(node('#decisionConfirm').textContent, 'Update now');
      assert.equal(node('#decisionSecondary').textContent, 'Download in background');
      assert.equal(node('#decisionCancel').textContent, 'Keep current version');
      const background = mode.startsWith('background');
      await click(background ? '#decisionSecondary' : '#decisionConfirm');
      await until(() => requestSignal, 'real update installer requests the published descriptor');
      if (background) {
        await until(
          () => session.getRuntime().getSnapshot().launched,
          'old installed generation launches before deferred update settles',
        );
        const epoch = session.getRuntime().getSnapshot().epoch;
        assert.equal(session.getRuntime().getSnapshot().generationId, installed.generation.id);
        assert.equal(
          node('#transferCancel').hidden,
          true,
          'background update never owns blocking Cancel',
        );
        if (mode === 'background-retired') {
          await back();
          await until(() => !playerOpen(), 'Runtime closes before background rejection');
        }
        const feedback = session.feedback.getSnapshot(),
          transfer = session.transfer.getSnapshot();
        await React.act(async () => {
          rejectDescriptor(new Error('fixture background descriptor unavailable'));
        });
        await tick();
        assert.deepEqual(
          session.feedback.getSnapshot(),
          feedback,
          'pinned main logs background failure without status or toast',
        );
        assert.deepEqual(
          session.transfer.getSnapshot(),
          transfer,
          'silent update never changes transfer presentation',
        );
        assert.ok(
          f.warnings.some(
            (value) =>
              value.message ===
              'th06: background Package update failed for local install Error: fixture background descriptor unavailable',
          ),
        );
        if (mode === 'background-live') {
          assert.equal(session.getRuntime().getSnapshot().epoch, epoch);
          assert.equal(session.getRuntime().getSnapshot().launched, true);
          assert.equal(playerOpen(), true);
        } else assert.equal(session.getRuntime().getSnapshot().epoch, null);
      } else {
        assert.equal(node('#transferCancel').hidden, false);
        assert.equal(node('#transferCancel').textContent, 'Cancel update');
        if (mode === 'foreground-cancel') {
          await click('#transferCancel');
          assert.equal(requestSignal.aborted, true);
        } else
          await React.act(async () => {
            rejectDescriptor(new Error('fixture foreground descriptor unavailable'));
          });
        await until(
          () => session.getRuntime().getSnapshot().launched,
          'foreground update failure/cancel continues the installed version',
        );
        assert.equal(
          session.feedback.getSnapshot().toast,
          mode === 'foreground-cancel'
            ? 'Update cancelled; continuing with the current version.'
            : 'Game-resource update failed; continuing with the current version: fixture foreground descriptor unavailable',
        );
        assert.equal(session.getRuntime().getSnapshot().generationId, installed.generation.id);
        assert.equal(node('#transferCancel').hidden, true);
        assert.equal(playerOpen(), true);
      }
      await React.act(async () => {
        assert.equal(
          (await api.readCurrentPackageGeneration('th06')).generation.id,
          installed.generation.id,
          'failed/cancelled update never replaces the canonical installed generation',
        );
      });
      assert.equal(node('#gameFrame'), frame);
    } finally {
      await React.act(async () => {
        rejectDescriptor(new Error('fixture cleanup'));
      });
      await tick();
    }
  });

for (const entry of storageImportCases) {
  test(`actual BrowserLauncher: running ${entry.kind} import hides Player only after successful original transaction`, async () => {
    const f = await runningStorageImport(entry),
      frame = node('#gameFrame'),
      locationKey = f.router.state.location.key,
      previous = currentUrl();
    const startIndex = f.order.length;
    f.writeMode = 'hold';
    await openStoragePicker(entry);
    await chooseStorageFile(entry);
    await until(
      () => f.pendingWrite.length === 1,
      'actual importer awaits native write acknowledgement',
    );
    assert.equal(playerOpen(), true, 'an unfinished import must not hide Player');
    assert.equal(node('#player').classList.contains('open'), true);
    const write = f.pendingWrite.shift();
    assert.equal(write.path, entry.path);
    assert.deepEqual(write.bytes, [3, 7, 9]);
    await React.act(async () => {
      f.storedFiles.set(write.path, [...write.bytes]);
      f.native.respond(write);
    });
    await until(
      () =>
        node('#status').textContent === 'Imported 1 file(s); restart with Start Game to apply them',
      'original import success feedback',
    );
    assert.equal(
      playerOpen(),
      false,
      'main6396–6397 hides Player after successful Runtime retirement',
    );
    assert.equal(node('#player').classList.contains('open'), false);
    assert.equal(node('#toastText').textContent, node('#status').textContent);
    assert.equal(
      node('#gameFrame'),
      frame,
      'the same React-owned iframe survives file Runtime retirement',
    );
    assert.equal(currentUrl(), previous);
    assert.equal(f.router.state.location.key, locationKey, 'import success adds no navigation');
    const lifecycle = f.order
      .slice(startIndex)
      .filter((value) =>
        [
          'sync',
          'list',
          'write',
          'read',
          'native-retired',
          'native-navigation',
          'launch',
          'configure',
          'fullscreen-exit',
        ].includes(value),
      );
    if (entry.kind === 'replay') {
      assert.deepEqual(lifecycle, [
        'list',
        'write',
        'native-retired',
        'native-navigation',
        'sync',
        'list',
      ]);
      assert.equal(
        node('#replayDialog').open,
        true,
        'main keeps the open Replay manager and refreshes it after retirement',
      );
      assert.equal(node('.replay-name').textContent, 'th6_01.rpy');
    } else {
      assert.deepEqual(lifecycle, [
        'sync',
        'native-retired',
        'native-navigation',
        'write',
        'native-retired',
        'native-navigation',
        'read',
        'native-retired',
      ]);
      assert.equal(f.native.location.href, 'about:blank');
      assert.equal(node('#replayDialog').open, false);
    }
  });
  test(`actual BrowserLauncher: running ${entry.kind} import picker cancellation preserves Player and its native owner`, async () => {
    const f = await runningStorageImport(entry),
      frame = node('#gameFrame'),
      source = f.native.location.href,
      previous = currentUrl(),
      before = [...f.order];
    await openStoragePicker(entry);
    await React.act(async () => {
      node('#fileInput').dispatchEvent(new env.window.Event('cancel'));
    });
    await tick();
    assert.equal(playerOpen(), true);
    assert.equal(node('#player').classList.contains('open'), true);
    assert.equal(node('#gameFrame'), frame);
    assert.equal(f.native.location.href, source);
    assert.equal(currentUrl(), previous);
    assert.deepEqual(
      f.order,
      before,
      'cancellation cannot sync, write or retire the running owner',
    );
  });
  test(`actual BrowserLauncher: running ${entry.kind} import write failure does not use the success visibility boundary`, async () => {
    const f = await runningStorageImport(entry),
      frame = node('#gameFrame'),
      previous = currentUrl();
    f.writeMode = 'fail';
    await openStoragePicker(entry);
    await chooseStorageFile(entry);
    await until(
      () => node('#status').textContent.includes('Synthetic native write failure'),
      'native write error reaches original feedback',
    );
    assert.equal(playerOpen(), true);
    assert.equal(node('#player').classList.contains('open'), true);
    assert.equal(node('#gameFrame'), frame);
    assert.equal(currentUrl(), previous);
    assert.equal(
      f.native.location.href === 'about:blank',
      false,
      'failed import retains its current native owner',
    );
    assert.equal(node('#status').textContent.includes('Imported'), false);
  });
  if (entry.kind !== 'replay')
    test(`actual BrowserLauncher: running ${entry.kind} import verification failure keeps Player open after the real reopen`, async () => {
      const f = await runningStorageImport(entry),
        before = f.navigations.length;
      f.corruptRead = true;
      await openStoragePicker(entry);
      await chooseStorageFile(entry);
      await until(
        () =>
          node('#status').textContent ===
          'Error: The saved file could not be read back intact from browser storage',
        'exact readback failure',
      );
      assert.equal(playerOpen(), true);
      assert.equal(node('#player').classList.contains('open'), true);
      assert.deepEqual(f.storedFiles.get(entry.path), [3, 7, 9]);
      assert.equal(
        f.navigations.length - before,
        4,
        'running retirement and verification reopen happened, without final success retirement',
      );
      assert.equal(f.native.location.href === 'about:blank', false);
    });
}

for (const outcome of ['launch', 'failure', 'fullscreen'])
  test(`actual BrowserLauncher: Replay import completion cannot hide a newer Start (${outcome}) waiting on its manager refresh`, async () => {
    const entry = storageImportCases[2],
      f = await runningStorageImport(entry),
      frame = node('#gameFrame'),
      previous = currentUrl(),
      music = node('#musicSelect').value;
    f.holdImportedList = true;
    await openStoragePicker(entry);
    await chooseStorageFile(entry);
    await until(
      () => f.pendingList.length === 1,
      'import retired gameplay and awaits the refreshed Replay listing',
    );
    assert.equal(playerOpen(), true);
    await click('[data-replay-close]');
    await until(() => !node('#replayDialog').open, 'manager closes during its pending refresh');
    let releaseFullscreen;
    if (outcome === 'fullscreen')
      f.fullscreenReady = new Promise((resolve) => {
        releaseFullscreen = resolve;
      });
    if (outcome === 'failure') f.failConfigure = true;
    await click('#launch');
    assert.equal(node('#decisionDialog').open, true);
    await click('#decisionConfirm');
    assert.equal(
      commands().filter((command) => command === 'launch').length,
      1,
      'new Start waits for fullscreen or the actual existing file owner',
    );
    await React.act(async () => {
      f.holdImportedList = false;
      f.native.respond(f.pendingList.shift(), {
        files: [...f.storedFiles].map(([path, bytes]) => ({
          path,
          size: bytes.length,
        })),
      });
    });
    if (outcome === 'fullscreen') {
      await until(
        () => node('#status').textContent.startsWith('Imported 1 file(s)'),
        'old import completes before the newer Start has a Runtime epoch',
      );
      assert.equal(
        playerOpen(),
        true,
        'same-value Player open intent survives old completion before native preparation',
      );
      await React.act(async () => {
        releaseFullscreen();
      });
    }
    if (outcome === 'failure')
      await until(
        () => node('#startupError').textContent.includes('Synthetic native configure failure'),
        'newer accepted Start reports its own failure',
      );
    else
      await until(
        () => commands().filter((command) => command === 'launch').length === 2,
        'new Start acquires and launches its next Runtime',
      );
    await tick();
    assert.equal(
      playerOpen(),
      true,
      'the completed old import must not hide the newer Player intent, including its error view',
    );
    assert.equal(node('#player').classList.contains('open'), true);
    assert.equal(node('#gameFrame'), frame);
    assert.equal(currentUrl(), previous);
    assert.equal(
      node('#musicSelect').value,
      music,
      'old completion cannot replace current settings',
    );
  });

test('actual BrowserLauncher: two queued Replay imports followed by accepted Start respect the actual Runtime queue', async () => {
  const entry = storageImportCases[2],
    f = await runningStorageImport(entry),
    frame = node('#gameFrame');
  f.holdImportedList = true;
  await openStoragePicker(entry);
  await chooseStorageFile(entry);
  await until(
    () => f.pendingList.length === 1,
    'first import waits for its post-retirement manager refresh',
  );
  await openStoragePicker(entry);
  await chooseStorageFile({
    ...entry,
    name: 'th6_02.rpy',
  });
  assert.equal(
    commands().filter((command) => command === 'write').length,
    1,
    'second actual import is queued',
  );
  await click('[data-replay-close]');
  await until(() => !node('#replayDialog').open, 'manager closed');
  await click('#launch');
  await click('#decisionConfirm');
  await React.act(async () => {
    f.holdImportedList = false;
    f.native.respond(f.pendingList.shift(), {
      files: [...f.storedFiles].map(([path, bytes]) => ({
        path,
        size: bytes.length,
      })),
    });
  });
  await until(
    () => commands().filter((command) => command === 'launch').length === 2,
    'new Start owns the next Runtime',
  );
  await tick();
  assert.equal(playerOpen(), true);
  assert.equal(node('#gameFrame'), frame);
  assert.equal(
    commands().filter((command) => command === 'write').length,
    2,
    'both serialized imports finish their writes',
  );
  const secondWrite = f.order.lastIndexOf('write'),
    lastRetirement = f.order.lastIndexOf('native-retired');
  assert.ok(
    secondWrite < lastRetirement && lastRetirement < f.order.lastIndexOf('native-navigation'),
  );
  assert.ok(
    f.order.lastIndexOf('native-navigation') < f.order.lastIndexOf('configure') &&
      f.order.lastIndexOf('configure') < f.order.lastIndexOf('launch'),
    'the second import retires its file-only owner before the newer Start acquires its running epoch',
  );
  assert.notEqual(f.native.location.href, 'about:blank');
});

for (const { launchedGame, phase } of [
  {
    launchedGame: 'th06',
    phase: 'running',
  },
  {
    launchedGame: 'th07',
    phase: 'running',
  },
  {
    launchedGame: 'th06',
    phase: 'loading',
  },
])
  test(`actual BrowserLauncher: acquisition-delayed queued Replay import cannot retire a newer accepted ${launchedGame} ${phase} Runtime`, async () => {
    const f = await mountBrowser({
        initial: `/?game=${launchedGame}&keep=a%20b`,
        host: subsetHost(['th06', 'th07']),
        seedStorage(storage) {
          storage.values.set(
            api.gamePreferenceStorageKey(launchedGame),
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
      session = api.getTestSession(),
      frame = node('#gameFrame');
    let releaseRead,
      held = false;
    const readGate = new Promise((resolve) => {
      releaseRead = resolve;
    });
    api.setPackageReadGate(async (game) => {
      if (game === 'th06' && !held) {
        held = true;
        await readGate;
      }
    });
    const outcomes = [];
    const observe = (promise) =>
      promise.then(
        () => {
          outcomes.push({
            status: 'fulfilled',
          });
        },
        (error) => {
          outcomes.push({
            status: 'rejected',
            name: error.name,
            message: error.message,
          });
        },
      );
    let first, second;
    try {
      await React.act(async () => {
        // Same real entry as Replay drop import, before either operation owns an epoch.
        first = observe(
          session.importFile('replay', new NativeFile([new Uint8Array([1])], 'th6_01.rpy'), 'th06'),
        );
        second = observe(
          session.importFile('replay', new NativeFile([new Uint8Array([2])], 'th6_02.rpy'), 'th06'),
        );
      });
      await until(() => held, 'first queued import waits at the real acquisition storage read');
      assert.equal(session.getRuntime().getSnapshot().epoch, null);
      assert.equal(commands().includes('write'), false);
      if (phase === 'loading') f.autoReady = false;
      await start();
      await until(
        () => session.getRuntime().getSnapshot().phase === phase,
        'newer accepted Start acquires its Runtime while old file preparation is outside native ownership',
      );
      const running = session.getRuntime().getSnapshot(),
        beforeRelease = [...f.order],
        previous = currentUrl(),
        transfer = session.transfer.getSnapshot();
      assert.equal(running.launched, phase === 'running');
      assert.equal(playerOpen(), true);
      f.autoReady = true; // Only subsequent navigations auto-ready; the held document still waits.
      await React.act(async () => {
        releaseRead();
        await Promise.all([first, second]);
      });
      await tick();
      assert.equal(
        outcomes[0].status,
        'rejected',
        'first stale preparation is rejected before it can acquire native ownership',
      );
      assert.equal(outcomes[0].name, 'AbortError');
      assert.equal(outcomes[0].message, 'EAGLER_RUNTIME_SESSION_SUPERSEDED');
      assert.equal(
        outcomes[1].status,
        'rejected',
        'queued import explicitly rejects rather than reporting a false import success',
      );
      assert.equal(outcomes[1].name, 'AbortError');
      assert.equal(outcomes[1].message, 'EAGLER_RUNTIME_SESSION_SUPERSEDED');
      assert.equal(node('#gameFrame'), frame);
      assert.equal(currentUrl(), previous);
      assert.deepEqual(
        f.order,
        beforeRelease,
        'neither stale preparation nor the queued import may list/write/sync/retire the newer Runtime',
      );
      assert.deepEqual(
        session.transfer.getSnapshot(),
        transfer,
        'late onRuntimePlan/error cleanup cannot replace or hide the newer transfer view',
      );
      const current = session.getRuntime().getSnapshot();
      assert.equal(
        current.epoch,
        running.epoch,
        `stale second import cannot retire newer epoch; ${JSON.stringify({
          outcomes,
          order: f.order,
          current,
          playerOpen: playerOpen(),
        })}`,
      );
      assert.equal(current.game, launchedGame);
      assert.equal(current.launched, phase === 'running');
      assert.equal(playerOpen(), true);
      assert.equal(commands().includes('write'), false);
      if (phase === 'loading') {
        await React.act(async () => {
          f.native.emit('ready');
        });
        await until(
          () => session.getRuntime().getSnapshot().launched,
          'the preserved newer startup can finish its own launch',
        );
        assert.equal(session.getRuntime().getSnapshot().epoch, running.epoch);
        assert.equal(playerOpen(), true);
      }
    } finally {
      releaseRead();
      api.setPackageReadGate(null);
      await Promise.all([first, second].filter(Boolean));
    }
  });
// Real development acquisition, paused only at the authored fetch/storage edges.

test(
  'held package fetch fixture drains current and future requests without releasing an active acquisition early',
  {
    timeout: 3000,
  },
  async () => {
    const edge = heldPackageFetch(),
      controller = new AbortController();
    const registered = edge.waitForRequest('th10');
    let completed = false;
    const response = edge
      .fetchBoundary(new URL('https://launcher.invalid/fixture-th10.data'), {
        signal: controller.signal,
      })
      .then((value) => {
        completed = true;
        return value;
      });
    const request = await registered;
    assert.equal(request, edge.requests.get('th10'));
    assert.equal(request.signal, controller.signal);
    assert.equal(request.released, false);
    assert.equal(completed, false, 'registration does not release the held fetch');
    edge.releaseAll();
    assert.deepEqual([...new Uint8Array(await (await response).arrayBuffer())], [1, 2, 3]);
    const late = edge.fetchBoundary(new URL('https://launcher.invalid/fixture-th11.data'), {
      signal: controller.signal,
    });
    assert.equal((await edge.waitForRequest('th11')).released, true);
    assert.deepEqual([...new Uint8Array(await (await late).arrayBuffer())], [1, 2, 3]);
    edge.releaseAll();
  },
);

test(
  'held package fetch registration rejects an operation failure, early completion and a bounded missing request',
  {
    timeout: 3000,
  },
  async () => {
    const edge = heldPackageFetch(),
      failure = new Error('Synthetic acquisition failure before fetch');
    await assert.rejects(
      edge.waitForRequest('th10', Promise.reject(failure)),
      (error) => error === failure,
    );
    await assert.rejects(
      edge.waitForRequest('th10', Promise.resolve()),
      /Operation completed before package fetch th10/,
    );
    await assert.rejects(
      edge.waitForRequest('th10', undefined, {
        timeoutMs: 10,
      }),
      /Package fetch th10 was not registered/,
    );
    edge.releaseAll();
  },
);

test('actual BrowserLauncher: queued import captured before held fullscreen cannot reclaim a cancelled newer Start recovery', async () => {
  const edge = heldPackageFetch();
  const f = await mountBrowser({
      initial: '/?game=th10',
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
  let firstRead,
    secondRead,
    fullscreen,
    calls = 0,
    first,
    second;
  const gate1 = new Promise((resolve) => {
      firstRead = resolve;
    }),
    gate2 = new Promise((resolve) => {
      secondRead = resolve;
    });
  f.fullscreenReady = new Promise((resolve) => {
    fullscreen = resolve;
  });
  api.setPackageReadGate((game) => (game === 'th06' ? (++calls === 1 ? gate1 : gate2) : null));
  try {
    await React.act(async () => {
      first = session
        .importFile('replay', new NativeFile([new Uint8Array([1])], 'th6_01.rpy'), 'th06')
        .catch((error) => error);
      second = session
        .importFile('replay', new NativeFile([new Uint8Array([2])], 'th6_02.rpy'), 'th06')
        .catch((error) => error);
    });
    await until(() => calls === 1, 'first queued import waits before native ownership');
    await start();
    await React.act(async () => {
      firstRead();
      assert.equal(await first, undefined);
    });
    await until(
      () => calls === 2,
      'second older import begins preparation during newer fullscreen wait',
    );
    assert.equal(session.getRuntime().getSnapshot().epoch, null);
    await React.act(async () => {
      fullscreen();
    });
    await until(
      () => edge.requests.has('th10') && !node('#transferCancel').hidden,
      'newer Start owns real TH10 acquisition',
    );
    await click('#transferCancel');
    await until(
      () => session.gameData.getSnapshot().attempt?.manual === true && !playerOpen(),
      'cancelled newer Start owns manual-import recovery',
    );
    const recovery = session.gameData.getSnapshot(),
      transfer = session.transfer.getSnapshot(),
      previous = currentUrl();
    assert.equal(recovery.importOpen, true);
    assert.equal(node('#gameDataImportWindow').hidden, false);
    f.autoReady = false;
    await React.act(async () => {
      secondRead();
    });
    await until(
      () => session.getRuntime().getSnapshot().phase === 'loading',
      'second import still acquires its own file Runtime',
    );
    assert.deepEqual(
      session.gameData.getSnapshot(),
      recovery,
      'old onRuntimePlan cannot replace newer recovery with a direct-download attempt',
    );
    await React.act(async () => {
      f.native.emit('ready');
      assert.equal(await second, undefined);
    });
    await tick();
    assert.equal(
      commands().filter((command) => command === 'write').length,
      2,
      'both original queued imports retain their successful native writes',
    );
    assert.deepEqual(
      session.gameData.getSnapshot(),
      recovery,
      'old native ready cannot clear the newer recovery window',
    );
    assert.deepEqual(
      session.transfer.getSnapshot(),
      transfer,
      'old work cannot reclaim cancellation or transfer presentation after launch flags clear',
    );
    assert.equal(node('#gameDataImportWindow').hidden, false);
    assert.equal(node('#transferCancel').hidden, !transfer.cancelVisible);
    assert.equal(currentUrl(), previous);
    assert.equal(playerOpen(), false);
    assert.equal(session.getRuntime().getSnapshot().epoch, null);
  } finally {
    firstRead();
    secondRead();
    fullscreen();
    api.setPackageReadGate(null);
    edge.releaseAll();
    if (f.native && session.getRuntime().getSnapshot().phase === 'loading') f.native.emit('ready');
    await React.act(async () => {
      await Promise.all([first, second].filter(Boolean));
    });
    await tick();
  }
});

test('actual BrowserLauncher: an unaccepted newer Start warning does not suppress the old import success close', async () => {
  const entry = storageImportCases[2],
    f = await runningStorageImport(entry);
  f.holdImportedList = true;
  await openStoragePicker(entry);
  await chooseStorageFile(entry);
  await until(() => f.pendingList.length === 1, 'post-import Replay refresh is pending');
  await click('[data-replay-close]');
  await until(() => !node('#replayDialog').open, 'manager closed');
  await click('#launch');
  assert.equal(node('#decisionDialog').open, true);
  await React.act(async () => {
    f.holdImportedList = false;
    f.native.respond(f.pendingList.shift(), {
      files: [],
    });
  });
  await until(
    () => node('#status').textContent.startsWith('Imported 1 file(s)'),
    'original import succeeded while warning is undecided',
  );
  assert.equal(
    playerOpen(),
    false,
    'only an accepted Player presentation supersedes old import completion',
  );
  await click('#decisionCancel');
  assert.equal(playerOpen(), false);
  assert.equal(commands().filter((command) => command === 'launch').length, 1);
});

test('actual BrowserLauncher: running save import overwrite refusal leaves the picker and native owner untouched', async () => {
  const f = await runningStorageImport(storageImportCases[0]),
    before = [...f.order],
    accept = node('#fileInput').accept;
  await click('[data-action="import-save"]');
  await click('#decisionCancel');
  assert.equal(playerOpen(), true);
  assert.equal(node('#player').classList.contains('open'), true);
  assert.equal(node('#fileInput').accept, accept);
  assert.deepEqual(f.order, before);
});

test('actual BrowserLauncher: import-only Start → invalid file → valid canonical ZIP retries the captured launch without a second warning/fullscreen', async () => {
  const f = await mountBrowser({
    resourceMode: 'import',
  });
  await click('#launch');
  await until(() => !node('#gameDataImportWindow').hidden, 'main import-required window opens');
  assert.equal(node('#decisionDialog').open, false);
  assert.equal(playerOpen(), false);
  assert.equal(f.navigations.length, 0);
  await selectImport(new NativeFile([], 'empty.zip'));
  assert.equal(node('#gameDataImportWindow').hidden, false);
  assert.equal(node('#gameDataImportWindow').getAttribute('aria-busy'), 'false');
  await React.act(async () => {
    assert.equal(
      (await api.readCurrentPackageGeneration('th06')).generation,
      null,
      'invalid input cannot install a partial current generation',
    );
  });
  await selectImport(importPackageFile());
  await until(
    () => commands().includes('launch'),
    'validated canonical import resumes the original launch',
  );
  assert.equal(playerOpen(), true);
  assert.equal(node('#decisionDialog').open, false);
  assert.equal(f.order.includes('fullscreen-enter'), false);
  let current;
  await React.act(async () => {
    current = await api.readCurrentPackageGeneration('th06');
  });
  assert.equal(current.generation.descriptor.revision, 'synthetic-import-r1');
  assert.deepEqual(
    f.dataReads,
    [[1, 2, 3]],
    'actual managed DATA callback reads the installed canonical object',
  );
  assert.deepEqual(f.nativeWrites.map((item) => item.path).sort(), [
    '/msgothic.ttc',
    '/unifont.otf',
  ]);
  assert.equal(commands().filter((command) => command === 'launch').length, 1);
  await back();
  await until(() => !playerOpen(), 'imported session closes through actual save boundary');
});

// Main4928–4949 reads music after the touch-input confirmation, not before it.
test('actual BrowserLauncher: late Host fallback between input warnings uses the current music warning', async () => {
  env.window.AudioContext = class {};
  Object.defineProperty(env.window.navigator, 'maxTouchPoints', {
    configurable: true,
    value: 1,
  });
  try {
    const f = await mountBrowser({
      initial: '/?game=th07',
      holdHost: true,
      host: subsetHost(['th06']),
      seedStorage(storage) {
        storage.values.set(
          api.gamePreferenceStorageKey('th07'),
          JSON.stringify({
            music: 'midi',
            musicPreferenceExplicit: true,
            options: {
              touchEnabled: false,
            },
          }),
        );
        storage.values.set(
          api.gamePreferenceStorageKey('th06'),
          JSON.stringify({
            music: 'none',
            musicPreferenceExplicit: true,
            options: {
              touchEnabled: false,
            },
          }),
        );
      },
    });
    assert.equal(node('#musicSelect').value, 'midi');
    await click('#launch');
    assert.match(node('#decisionMessage').textContent, /touch controls.*disabled/i);
    await acceptHeldHost(f);
    assert.equal(node('#musicSelect').value, 'none');
    await click('#decisionConfirm');
    assert.equal(node('#decisionDialog').open, true);
    assert.match(node('#decisionMessage').textContent, /without music/i);
    assert.equal(playerOpen(), false);
    assert.deepEqual(f.navigations, []);
    await click('#decisionCancel');
  } finally {
    delete env.window.AudioContext;
    Object.defineProperty(env.window.navigator, 'maxTouchPoints', {
      configurable: true,
      value: 0,
    });
  }
});

// Main lobby.mts113 gives the directory guide its filter product. The Launcher
// Host's independently retained business selection is not the directory filter.

for (const locale of ['zh-CN', 'en'])
  test(`actual BrowserLauncher: completed direct preload retains pinned ${locale} timeout recovery and startup error order`, async () => {
    const timers = capturePreparationTimers();
    try {
      const f = await mountBrowser({
          autoReady: false,
        }),
        session = api.getTestSession();
      await React.act(async () => {
        session.setLocale(locale);
      });
      await start();
      await until(
        () =>
          timers.live(120_000).length === 1 &&
          session.getRuntime().getSnapshot().phase === 'loading',
        'real direct Runtime waits the unchanged 120s ready deadline',
      );
      assert.equal(session.getRuntime().getSnapshot().generationId, null);
      await React.act(async () => {
        f.native.emit('transfer', {
          kind: 'game',
          mode: 'runtime',
          loaded: 100,
          total: 100,
        });
      });
      const attempt = session.gameData.getSnapshot().attempt;
      assert.equal(attempt.firstByte, true);
      assert.equal(attempt.downloadComplete, true);
      assert.equal(attempt.unlocked, false);
      assert.equal(timers.live(10_000).length, 0);
      assert.equal(timers.live(20_000).length, 0);
      const events = [],
        stopImport = session.gameData.subscribe(() => {
          if (session.gameData.getSnapshot().importOpen) events.push('import');
        });
      const stopError = session.startupError.subscribe(() => {
        if (session.startupError.getSnapshot().open) events.push('startup');
      });
      await React.act(async () => {
        timers.fire(timers.live(120_000)[0]);
      });
      await tick();
      stopImport();
      stopError();
      const message = locale === 'zh-CN' ? '游戏加载超时' : 'Game load timed out';
      assert.equal(session.getRuntime().getSnapshot().phase, 'error');
      assert.equal(session.getRuntime().getSnapshot().epoch, null);
      assert.equal(session.gameData.getSnapshot().attempt.id, attempt.id);
      assert.equal(session.startupError.getSnapshot().open, true);
      assert.match(node('#startupErrorText').textContent, new RegExp(message));
      assert.equal(node('#playerStatus').textContent, message);
      assert.equal(session.gameData.getSnapshot().attempt.unlocked, true);
      assert.equal(node('#gameDataImportWindow').hidden, false);
      {
        assert.ok(
          events.indexOf('import') >= 0 && events.indexOf('import') < events.indexOf('startup'),
        );
        assert.ok(node('#gameDataImportReason').textContent.trim());
        await click('#startupErrorClose');
        f.autoReady = true;
        await React.act(async () => {
          await session.gameData.importFile(importPackageFile());
        });
        await until(
          () => session.getRuntime().getSnapshot().launched,
          'the recovered import commits locally and starts the same Player without another Start click',
        );
        assert.equal(session.gameData.getSnapshot().attempt, null);
        assert.equal(playerOpen(), true);
      }
    } finally {
      timers.restore();
    }
  });

for (const scenario of ['managed timeout', 'native error', 'closed timeout'])
  test(`actual BrowserLauncher: ${scenario} cannot unlock unrelated direct import recovery`, async () => {
    const timers = capturePreparationTimers();
    try {
      const f = await mountBrowser({
          autoReady: false,
          seedPackages: scenario === 'managed timeout' ? ['th06'] : [],
        }),
        session = api.getTestSession();
      await React.act(async () => {
        session.setLocale('zh-CN');
      });
      await start();
      await until(() => timers.live(120_000).length === 1, 'actual native-ready deadline');
      if (scenario === 'native error')
        await React.act(async () => {
          f.native.emit('error', {
            error: 'fixture engine failed',
          });
        });
      else if (scenario === 'closed timeout') {
        let closing;
        await React.act(async () => {
          timers.fire(timers.live(120_000)[0]);
          closing = session.requestRuntimeClose();
          await closing;
        });
      } else
        await React.act(async () => {
          timers.fire(timers.live(120_000)[0]);
        });
      await tick();
      assert.equal(node('#gameDataImportWindow').hidden, true);
      assert.notEqual(session.gameData.getSnapshot().attempt?.unlocked, true);
      assert.equal(session.startupError.getSnapshot().open, scenario !== 'closed timeout');
      if (scenario === 'closed timeout') {
        assert.equal(session.gameData.getSnapshot().attempt, null);
        assert.equal(playerOpen(), false);
      }
    } finally {
      timers.restore();
    }
  });

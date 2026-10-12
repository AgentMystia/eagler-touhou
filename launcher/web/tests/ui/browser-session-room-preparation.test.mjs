import {
  NativeFile,
  React,
  api,
  assert,
  mountBrowser,
  node,
  siteManifest,
  test,
  until,
} from './browser-session-fixture.mjs';

for (const kind of ['launch', undefined])
  test(`actual BrowserLauncher: failed-room import rejects ${kind ?? 'absent'} continuation as a room resume handle`, async () => {
    const host = siteManifest();
    host.shared.netplayRelay = 'wss://relay.invalid/socket';
    await mountBrowser({
      initial: '/en.html?game=th06mp&mpRoom=1234',
      host,
      socketBoundary: true,
    });
    const session = api.getTestSession();
    await until(
      () => session.room.service.getSnapshot().preparation?.status === 'failed',
      'canonical room preparation fails without a local or published package',
    );
    await React.act(async () => {
      if (kind)
        session.gameData.beginManual({
          kind,
        });
      else {
        session.gameData.beginDirectDownload();
        session.gameData.unlock('slow');
      }
    });
    let importing;
    await React.act(async () => {
      importing = session.gameData.importFile(new NativeFile(['invalid fixture ZIP'], 'bad.zip'));
      assert.equal(
        session.gameData.getSnapshot().busyText,
        'Validating and installing game package…',
      );
      assert.equal(session.room.service.getSnapshot().preparation.status, 'failed');
    });
    await React.act(async () => {
      await importing;
    });
    assert.equal(session.room.service.getSnapshot().preparation.status, 'failed');
    assert.equal(node('#gameDataImportWindow').hidden, false);
    await React.act(async () => {
      session.room.service.leave();
    });
  });

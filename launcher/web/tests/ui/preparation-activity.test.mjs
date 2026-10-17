import test from 'node:test';
import assert from 'node:assert/strict';
import { build } from 'esbuild';
import { fileURLToPath } from 'node:url';

const project = fileURLToPath(new URL('../../', import.meta.url));
const output = await build({
  absWorkingDir: project,
  entryPoints: ['app/session/preparation-activity.ts'],
  bundle: true,
  platform: 'node',
  format: 'esm',
  write: false,
  logLevel: 'silent',
});
const { createPreparationActivity } = await import(
  `data:text/javascript;base64,${Buffer.from(output.outputFiles[0].contents).toString('base64')}`
);

function fixture() {
  const notifications = [];
  const activity = createPreparationActivity({
    translate: (key) => key,
    onChange: (finished) => notifications.push(finished),
  });
  return { activity, notifications };
}

test('preparation: a retired runtime plan cannot change the current cancellation target', async () => {
  const { activity } = fixture();
  const previous = activity.begin();
  const current = activity.begin();
  current.setActivity('update', true);

  previous.runtimePlanned();
  await activity.cancelDownload(null);

  assert.equal(current.updateSignal.aborted, true);
  assert.equal(current.signal.aborted, false);
  assert.equal(previous.signal.aborted, false);
  current.finish();
  previous.finish();
});

test('preparation: retired activity and cleanup cannot release the current owner', () => {
  const { activity, notifications } = fixture();
  const previous = activity.begin();
  const current = activity.begin();
  current.setActivity('package', true);
  const before = [...notifications];

  previous.setActivity('package', false);
  previous.finish();

  assert.equal(activity.hasLaunch(), true);
  assert.equal(activity.isBlocking(), true);
  assert.deepEqual(notifications, before);
  current.finish();
  assert.equal(activity.hasLaunch(), false);
  assert.equal(activity.isBlocking(), false);
  assert.equal(notifications.at(-1), true);
});

test('preparation: optional downloads cancel independently from mandatory acquisition', async () => {
  for (const kind of ['update', 'music', 'package', 'language']) {
    const { activity } = fixture();
    const current = activity.begin();
    current.setActivity(kind, true);
    let fallbackUsed = false;
    await activity.cancelDownload({
      label: 'other download',
      async cancel() {
        fallbackUsed = true;
      },
    });
    assert.equal(fallbackUsed, false);
    assert.equal(current.updateSignal.aborted, kind === 'update');
    assert.equal(current.musicSignal.aborted, kind === 'music');
    assert.equal(current.signal.aborted, kind === 'package' || kind === 'language');
    current.finish();
  }
});

test('preparation: an external abort stays attached to its own operation', () => {
  const { activity } = fixture();
  const external = new AbortController();
  const previous = activity.begin(external.signal);
  previous.finish();
  const current = activity.begin();
  external.abort(new Error('previous operation cancelled'));
  assert.equal(current.signal.aborted, false);
  assert.equal(current.isCurrent(), true);
  activity.abort(external.signal.reason);
  assert.equal(current.signal.aborted, true);
  assert.equal(current.signal.reason, external.signal.reason);
  current.finish();
});

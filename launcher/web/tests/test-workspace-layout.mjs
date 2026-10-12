import assert from 'node:assert/strict';
import { resolve } from 'node:path';
import {
  launcherRoot,
  workspacePath,
  workspaceRepositoryRoot,
  WORKSPACE_REPOSITORIES,
  projectRelativeWorkspacePath,
} from '../lib/workspace-layout.mjs';
const original = process.env.EAGLER_WORKSPACE_ROOT;
try {
  const relocated = resolve(launcherRoot(), '.cache/relocated-project');
  process.env.EAGLER_WORKSPACE_ROOT = relocated;
  for (const [name, directory] of Object.entries(WORKSPACE_REPOSITORIES)) {
    if (name === 'launcher') continue;
    assert.equal(workspacePath(name, 'input.dat'), resolve(relocated, directory, 'input.dat'));
    assert.equal(
      resolve(launcherRoot(), projectRelativeWorkspacePath(name, 'input.dat')),
      workspacePath(name, 'input.dat'),
    );
  }
  assert.equal(workspacePath('launcher'), launcherRoot());
  assert.throws(() => workspacePath('not-a-repository'), /unknown workspace repository/);
} finally {
  if (original === undefined) delete process.env.EAGLER_WORKSPACE_ROOT;
  else process.env.EAGLER_WORKSPACE_ROOT = original;
}
assert.equal(workspaceRepositoryRoot('launcher'), resolve(launcherRoot(), '../..'));
console.log('Workspace relocation and component/repository boundaries: PASS');

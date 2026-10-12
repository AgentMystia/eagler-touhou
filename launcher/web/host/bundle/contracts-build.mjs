import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
const root = fileURLToPath(new URL('../', import.meta.url));
/** 自托管发行包使用随包发布的协议产物。 */
export async function ensureContractsBuild({ project = root } = {}) {
  return { mode: 'prebuilt', root: project, contractsDirectory: resolve(project, 'assets/contracts'), inputHash: null };
}
export async function resolveRuntimeGenerationWorkerSource({ project = root } = {}) {
  return resolve(project, 'assets/contracts/runtime-generations-worker.js');
}

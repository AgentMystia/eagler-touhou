import { componentFileIds } from '../../../package/package-generation.mjs';
import { sha256Hex } from '../../../src/launcher/sha256.mts';
import type { InstalledPackageGeneration } from '../../../src/launcher/package-read-models.mts';
import type { RuntimeSessionToken } from '../../../src/launcher/runtime-session.mts';
import type {
  RuntimeDependencies,
  RuntimePlan,
  RuntimeServiceOptions,
  RuntimeWindow,
} from './types';
import { LocalOggBytesError, RuntimeSessionSupersededError } from './errors';
import { errorText } from '../error-text';

/** Owns resource validation, installation and progress for one runtime service.
 * The lifecycle owner supplies an identity check before and after every await. */
export function createRuntimeResources({
  runtimeIdentity,
  readResource,
  translate,
  onLocalResourceProgress,
}: {
  runtimeIdentity(token: RuntimeSessionToken): { runtime: RuntimeWindow; document: object };
  readResource: RuntimeDependencies['readResource'];
  translate: RuntimeServiceOptions['translate'];
  onLocalResourceProgress?: RuntimeServiceOptions['onLocalResourceProgress'];
}) {
  let localResourceProgress: {
    epoch: number;
    revision: string;
    started: number;
    loaded: number;
    completed: Set<string>;
  } | null = null;
  function emitLocalResourceProgress(
    token: RuntimeSessionToken,
    generation: InstalledPackageGeneration,
    phase: 'initial' | 'remaining',
  ) {
    if (!onLocalResourceProgress) return;
    runtimeIdentity(token);
    if (
      !localResourceProgress ||
      localResourceProgress.epoch !== token.id ||
      localResourceProgress.revision !== generation.descriptor.revision
    ) {
      localResourceProgress = {
        epoch: token.id,
        revision: generation.descriptor.revision,
        started: performance.now(),
        loaded: 0,
        completed: new Set(),
      };
    }
    const ids = componentFileIds(generation.descriptor, 'ogg').filter(
      (id) => !!generation.files[id]?.objectId,
    );
    const total = ids.reduce(
      (sum, id) => sum + (Number(generation.descriptor.files[id]?.bytes) || 0),
      0,
    );
    onLocalResourceProgress({
      epoch: token.id,
      game: generation.game,
      generationId: generation.id,
      phase,
      loaded: localResourceProgress.loaded,
      total,
      completed: localResourceProgress.completed.size,
      files: ids.length,
      speed:
        localResourceProgress.loaded /
        Math.max((performance.now() - localResourceProgress.started) / 1000, 0.1),
    });
  }
  function noteLocalResource(
    token: RuntimeSessionToken,
    generation: InstalledPackageGeneration,
    id: string,
    bytes: number,
    phase: 'initial' | 'remaining',
  ) {
    if (!onLocalResourceProgress) return;
    if (!localResourceProgress || localResourceProgress.epoch !== token.id)
      emitLocalResourceProgress(token, generation, phase);
    if (localResourceProgress && !localResourceProgress.completed.has(id)) {
      localResourceProgress.loaded += bytes;
      localResourceProgress.completed.add(id);
    }
    emitLocalResourceProgress(token, generation, phase);
  }
  function executableResource(
    plan: RuntimePlan,
    id: string,
    source: string,
    target: string,
  ): boolean {
    return (
      id === 'game-data' ||
      id === plan.generation?.descriptor.runtimeRequirement?.dataFile ||
      /\.(?:html|m?js|wasm)$/i.test(source) ||
      /\.(?:html|m?js|wasm)$/i.test(target)
    );
  }
  async function installResources(
    token: RuntimeSessionToken,
    plan: RuntimePlan,
    extensionGuard?: () => void,
    localProgress = true,
  ) {
    const generation = plan.resourceGeneration ?? plan.generation;
    extensionGuard?.();
    if (!plan.resourceFileIds?.length) return;
    if (!generation || !plan.generation)
      throw new Error('Managed resources require a Package generation');
    const before = runtimeIdentity(token),
      fs = before.runtime.FS ?? before.runtime.Module?.FS;
    if (!fs) throw new Error(translate('runtime.offlineFsUnavailable'));
    let failed = false,
      next = 0;
    const check = () => {
      if (failed) throw new RuntimeSessionSupersededError();
      extensionGuard?.();
      const after = runtimeIdentity(token);
      if (after.runtime !== before.runtime || after.document !== before.document)
        throw new RuntimeSessionSupersededError();
    };
    const installOne = async (id: string) => {
      try {
        check();
        const declaration = generation.descriptor.files[id];
        if (!declaration || executableResource(plan, id, declaration.source, declaration.target))
          throw new Error(`Not a managed Runtime resource: ${id}`);
        const resource = await readResource(generation, id);
        check();
        if (
          !resource ||
          resource.path !== declaration.target ||
          (extensionGuard && resource.buffer.byteLength <= 0) ||
          (declaration.bytes != null && resource.buffer.byteLength !== Number(declaration.bytes))
        )
          throw new Error(translate('runtime.resourceDamaged', { path: declaration.target }));
        if (
          extensionGuard &&
          declaration.sha256 &&
          (await sha256Hex(resource.buffer)) !== declaration.sha256.toLowerCase()
        )
          throw new Error(translate('runtime.resourceDamaged', { path: declaration.target }));
        check();
        const slash = resource.path.lastIndexOf('/');
        if (slash > 0) fs.mkdirTree(resource.path.slice(0, slash));
        fs.writeFile(resource.path, new Uint8Array(resource.buffer), { canOwn: true });
        if (extensionGuard && localProgress)
          noteLocalResource(token, generation, id, resource.buffer.byteLength, 'remaining');
      } catch (error) {
        failed = true;
        throw error;
      }
    };
    if (extensionGuard && localProgress) emitLocalResourceProgress(token, generation, 'remaining');
    const worker = async () => {
      while (!failed && next < plan.resourceFileIds!.length)
        await installOne(plan.resourceFileIds![next++]);
    };
    await Promise.all(
      Array.from({ length: Math.min(extensionGuard ? 2 : 1, plan.resourceFileIds.length) }, worker),
    );
  }
  function localOggIds(plan: RuntimePlan): string[] {
    if (!plan.localOgg) return [];
    if (!plan.generation) throw new Error('Local OGG requires a Package generation');
    const generation = plan.resourceGeneration ?? plan.generation;
    // A historical TH09 viewer can cross music cues before the ordinary
    // background transfer finishes. This opt-in experiment requires the
    // complete compressed OGG set before either player/viewer starts.
    const ids =
        plan.game === 'th09' && plan.configure.options?.netplaySpectatorTimeline === true
          ? componentFileIds(generation.descriptor, 'ogg')
          : [...plan.localOgg.fileIds],
      allowed = new Set(componentFileIds(generation.descriptor, 'ogg'));
    if (
      plan.configure.music !== 'ogg' ||
      !ids.length ||
      new Set(ids).size !== ids.length ||
      ids.some((id) => {
        const declaration = generation.descriptor.files[id],
          ref = generation.files[id];
        return (
          !allowed.has(id) ||
          generation.descriptor.base.files.includes(id) ||
          !declaration?.target ||
          executableResource(plan, id, declaration.source, declaration.target) ||
          !ref?.objectId ||
          ref.revision !== declaration.revision
        );
      })
    )
      throw new Error(translate('runtime.localOggDescriptorInvalid'));
    // The validated descriptor owns target paths. Historical descriptors may
    // use custom mounts and omit byte length/hash; do not invent a new format.
    return ids;
  }
  async function installLocalOgg(
    token: RuntimeSessionToken,
    plan: RuntimePlan,
    ids: readonly string[],
  ) {
    const generation = plan.resourceGeneration ?? plan.generation;
    if (!generation) throw new Error('Local OGG requires a Package generation');
    const before = runtimeIdentity(token),
      fs = before.runtime.FS ?? before.runtime.Module?.FS;
    if (!before.runtime.Module) throw new Error('Runtime music selection is unavailable');
    if (!fs) throw new Error(translate('runtime.offlineFsUnavailable'));
    let failed = false,
      next = 0;
    const check = () => {
      if (failed) throw new RuntimeSessionSupersededError();
      const after = runtimeIdentity(token);
      if (after.runtime !== before.runtime || after.document !== before.document)
        throw new RuntimeSessionSupersededError();
    };
    const installOne = async (id: string) => {
      try {
        check();
        const declaration = generation.descriptor.files[id];
        let resource;
        try {
          resource = await readResource(generation, id);
        } catch (error) {
          check();
          if (error instanceof Error && error.name === 'AbortError') throw error;
          throw new LocalOggBytesError(errorText(error), { cause: error });
        }
        check();
        if (resource && resource.path !== declaration.target)
          throw new Error(translate('runtime.localOggDescriptorInvalid'));
        if (
          !resource ||
          resource.buffer.byteLength <= 0 ||
          (declaration.bytes != null && resource.buffer.byteLength !== Number(declaration.bytes)) ||
          (declaration.sha256 &&
            (await sha256Hex(resource.buffer)) !== declaration.sha256.toLowerCase())
        ) {
          check();
          throw new LocalOggBytesError(
            translate('runtime.resourceDamaged', { path: declaration.target }),
          );
        }
        // Hashing is asynchronous too: cancelled/sibling-failed reads cannot
        // write into a replaced document or revive a failed transfer.
        check();
        const slash = declaration.target.lastIndexOf('/');
        if (slash > 0) fs.mkdirTree(declaration.target.slice(0, slash));
        fs.writeFile(declaration.target, new Uint8Array(resource.buffer), { canOwn: true });
        noteLocalResource(token, generation, id, resource.buffer.byteLength, 'initial');
      } catch (error) {
        failed = true;
        throw error;
      }
    };
    emitLocalResourceProgress(token, generation, 'initial');
    const worker = async () => {
      while (!failed && next < ids.length) await installOne(ids[next++]);
    };
    await Promise.all(Array.from({ length: Math.min(2, ids.length) }, worker));
    check();
    runtimeIdentity(token).runtime.Module!.touhouMusicMode = 'ogg';
  }
  return { installResources, localOggIds, installLocalOgg, executableResource };
}

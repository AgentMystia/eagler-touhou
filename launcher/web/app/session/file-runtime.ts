import {
  gameIdForProduct,
  isMultiplayerProductId,
  type ProductId,
} from '@eagler/contracts/product-catalog';
import type { SettingsSnapshot } from '../models/game-settings';
import { prepareFilePlan, type FilePreparationInput } from '../services/game-preparation';
import type { PreparationProgress } from '../services/package-acquisition';
import {
  RuntimeSessionSupersededError,
  type RuntimeService,
  type RuntimePlan,
} from '../services/runtime';

interface FilePreparationPresentation {
  capture(): () => boolean;
  download(active: boolean): void;
  begin(plan: Readonly<RuntimePlan>): void;
  progress(progress: PreparationProgress): void;
  clear(): void;
  finish(): void;
}
interface FileRuntimeOptions extends Pick<
  FilePreparationInput,
  'acquisition' | 'baseUrl' | 'translate'
> {
  getRuntime(): RuntimeService;
  getSettings(product: ProductId): SettingsSnapshot;
  decideClose(error: unknown): Promise<'retry' | 'leave' | 'stay'>;
  presentation: FilePreparationPresentation;
}

/** Owns temporary file-only runtimes and their download cancellation.
 * A later gameplay launch never inherits or loses this operation's epoch. */
export function createFileRuntimeCoordinator({
  getRuntime,
  getSettings,
  acquisition,
  baseUrl,
  translate,
  decideClose,
  presentation,
}: FileRuntimeOptions) {
  let fileDownload: { controller: AbortController; ownsPresentation(): boolean } | null = null;
  let fileRuntimePresentation: (() => boolean) | null = null;
  const temporaryFiles = new Map<ProductId, number>();
  async function prepareFiles(
    product: ProductId,
    options: { retireRunning?: boolean; ownsPresentation?: () => boolean } = {},
  ) {
    const owner = getRuntime(),
      before = owner.getSnapshot(),
      game = gameIdForProduct(product);
    const variant = isMultiplayerProductId(product) ? 'multiplayer' : 'normal';
    // Imports retain their enqueue-time context across the Replay queue. Other
    // file tasks capture here. Progress, native UI and Cancel share this owner.
    const ownsPresentation = options.ownsPresentation ?? presentation.capture();
    if (
      before.ready &&
      before.game === game &&
      before.runtimeVariant === variant &&
      !(options.retireRunning && before.launched)
    )
      return;
    if (before.epoch !== null && !(await owner.close({ decide: decideClose })))
      throw new RuntimeSessionSupersededError();
    const controller = new AbortController();
    let preparationEpoch: number | null = null;
    const ownsCurrentEpoch = () => {
      const current = owner.getSnapshot();
      return (
        current.epoch === null ||
        (current.epoch === preparationEpoch &&
          current.game === game &&
          current.runtimeVariant === variant)
      );
    };
    const requireIdle = () => {
      if (owner.getSnapshot().epoch !== null) throw new RuntimeSessionSupersededError();
    };
    try {
      const plan = await prepareFilePlan({
        productId: product,
        settings: getSettings(product),
        acquisition,
        baseUrl,
        translate,
        signal: controller.signal,
        onAcquisitionActivity: (active) => {
          if (active) {
            requireIdle();
            fileDownload = { controller, ownsPresentation };
            if (ownsPresentation()) presentation.download(true);
          } else if (fileDownload?.controller === controller) {
            fileDownload = null;
            if (ownsCurrentEpoch() && ownsPresentation()) presentation.download(false);
          }
        },
        onRuntimePlan: (plan) => {
          requireIdle();
          if (ownsPresentation()) presentation.begin(plan);
        },
        onProgress: (progress) => {
          if (ownsCurrentEpoch() && ownsPresentation()) presentation.progress(progress);
        },
      });
      // Acquisition owns no native epoch. A newer Start may have acquired one
      // while it waited; never cancel that owner in this operation's catch.
      requireIdle();
      fileRuntimePresentation = ownsPresentation;
      const preparing = owner.prepare(plan, controller.signal);
      preparationEpoch = owner.getSnapshot().epoch;
      const ready = await preparing;
      if (ready.epoch !== owner.getSnapshot().epoch) throw new RuntimeSessionSupersededError();
      if (ready.epoch !== null) temporaryFiles.set(product, ready.epoch);
    } catch (error) {
      // A published-code retry may have advanced this prepare to a later epoch.
      // Runtime owns its failure reset; do not cancel an unproven current owner
      // or replace the original error if a later retry could not retire itself.
      if (error instanceof RuntimeSessionSupersededError || !ownsCurrentEpoch()) throw error;
      if (!owner.getSnapshot().launched) {
        try {
          owner.cancel();
        } catch {}
        if (ownsPresentation()) presentation.clear();
      }
      throw error;
    } finally {
      if (fileDownload?.controller === controller) {
        fileDownload = null;
        if (ownsCurrentEpoch() && ownsPresentation()) presentation.finish();
      }
    }
  }
  async function releasePrepared(product: ProductId, expectedEpoch = temporaryFiles.get(product)) {
    const owner = getRuntime(),
      current = owner.getSnapshot();
    if (
      expectedEpoch === undefined ||
      current.epoch !== expectedEpoch ||
      current.game !== gameIdForProduct(product) ||
      current.launched
    )
      return;
    if (await owner.close({ decide: decideClose })) temporaryFiles.delete(product);
  }
  return {
    prepareFiles,
    releasePrepared,
    preparedEpoch: (product: ProductId) => temporaryFiles.get(product),
    isDownloading: () => fileDownload !== null,
    allowsRuntimePresentation: () => fileRuntimePresentation?.() !== false,
    abort(reason: unknown) {
      fileDownload?.controller.abort(reason);
    },
    cancelDownload(reason: unknown) {
      if (!fileDownload?.ownsPresentation()) return false;
      fileDownload.controller.abort(reason);
      return true;
    },
  };
}

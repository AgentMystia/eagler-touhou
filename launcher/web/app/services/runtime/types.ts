import type { Translate } from '../../i18n';
import type { GameId } from '@eagler/contracts/product-catalog';
import type { SiteManifest } from '@eagler/contracts/site-manifest';
import type {
  RuntimeCommandPayloads,
  RuntimeConfigureOptions,
  RuntimeEventMessage,
  RuntimeEventPayloads,
  RuntimeResponseMessage,
} from '@eagler/contracts/runtime-protocol';
import type { InstalledPackageGeneration } from '../../../src/launcher/package-read-models.mts';
import type { RuntimeMessageTarget } from '../../../src/launcher/touch-runtime-protocol.mts';
import type { TouchLayout } from '../../../src/launcher/touch-layout-model.mts';
import type { prepareRuntimeLaunch } from '../../../src/launcher/runtime-launch.mts';
import type {
  readManagedRuntimeData,
  readManagedRuntimeResource,
} from '../../../src/launcher/runtime-preparation.mts';
import type {
  createNetworkActivityTracker,
  NetworkActivitySnapshot,
} from '../../../src/launcher/network-activity.mts';
import type {
  retainPackageGeneration,
  releasePackageGeneration,
} from '../../../package/package-store.mjs';

export interface RuntimeFilesystem {
  mkdirTree(path: string): void;
  writeFile(path: string, bytes: Uint8Array, options: { canOwn: boolean }): void;
}
export interface RuntimeWindow extends RuntimeMessageTarget {
  readonly document: object;
  readonly location: { href: string; replace(url: string): void };
  addEventListener?(type: string, listener: EventListener): void;
  removeEventListener?(type: string, listener: EventListener): void;
  FS?: RuntimeFilesystem;
  Module?: { FS?: RuntimeFilesystem; touhouMusicMode?: 'ogg' | 'midi' | 'none' };
}
/** Read-only native event surface; no filesystem or lifecycle mutation port. */
export interface RuntimeMidiEventTarget {
  addEventListener(type: string, listener: EventListener): void;
  removeEventListener(type: string, listener: EventListener): void;
}
export interface RuntimeMidiEventContext {
  readonly epoch: number;
  readonly game: GameId;
  readonly document: object;
  readonly target: RuntimeMidiEventTarget;
  readonly music: 'ogg' | 'midi' | 'none';
}
export interface RuntimeFrame {
  readonly contentWindow: RuntimeWindow | null;
  setAttribute(name: 'src', value: string): void;
  removeAttribute(name: 'src'): void;
  /** Emergency cleanup is allowed only with positive evidence of DOM removal. */
  readonly isConnected?: boolean;
  addEventListener(type: 'load', listener: EventListener): void;
  removeEventListener(type: 'load', listener: EventListener): void;
}
export interface ManagedDataRequest {
  game: string;
  generation: string;
  epoch: number;
}
export interface RuntimeHost {
  location: { href: string; origin: string };
  addEventListener(type: 'message', listener: (event: MessageEvent) => void): void;
  removeEventListener(type: 'message', listener: (event: MessageEvent) => void): void;
  __eaglerPrepareManagedRuntimeDataV1?: (
    request: ManagedDataRequest,
  ) => ReturnType<typeof readManagedRuntimeData>;
}
export interface RuntimeLauncherControls {
  readonly restartButtonEnabled: boolean;
  readonly thpracTouchControlsEnabled: boolean;
  readonly magnifierEnabled: boolean;
  readonly touchLayout: TouchLayout | null;
  /** Presentation-only alpha for the Launcher-owned touch HUD. Never sent to Runtime. */
  readonly touchControlOpacity?: number;
}
export interface RuntimeLauncherControlContext {
  readonly epoch: number;
  readonly game: GameId;
  readonly runtimeVariant: 'normal' | 'multiplayer';
  readonly options: Readonly<RuntimeConfigureOptions>;
  readonly launcherControls: Readonly<RuntimeLauncherControls>;
}
export interface RuntimePlan {
  game: GameId;
  runtimeVariant: 'normal' | 'multiplayer';
  /** Main file-only ensureRuntime(false) stops at native-ready, before configure. */
  fileOnly?: boolean;
  /** An already installed, verified Package Store generation, never mutable current lookup. */
  generation: InstalledPackageGeneration | null;
  /** Exact original hosted emscripten-preload fallback, with no Package DATA. */
  directPreloadHost?: SiteManifest;
  /** Optional resources acquired after native ready; never replaces loaded DATA. */
  resourceGeneration?: InstalledPackageGeneration;
  entry: string;
  /** Must reflect the current Site Manifest's runtimeManifest capability. */
  publishedRuntime: boolean;
  /** Exact validated Host authorizing live development code, never a fallback. */
  developmentRuntimeHost?: SiteManifest;
  runtimeHost?: SiteManifest;
  localCatalogRuntime?: boolean;
  /** Exact current wire payload. Adonis options are passed through, never inferred. */
  configure: RuntimeCommandPayloads['configure'];
  /** Package resource IDs selected by the acquisition owner, excluding DATA/code. */
  resourceFileIds?: readonly string[];
  /** Verified managed OGG; fallback permission is attested by the acquisition
   * owner against the validated Host and successfully prepared MIDI bridge. */
  localOgg?: { fileIds: readonly string[]; fallbackToMidi: boolean };
  /** Click-time Launcher-only controls, never added to native configure wire. */
  launcherControls?: RuntimeLauncherControls;
}
export type RuntimeReadyContinuation = (epoch: number) => Promise<RuntimePlan>;
export type RuntimePhase =
  | 'idle'
  | 'loading'
  | 'configuring'
  | 'prepared'
  | 'launching'
  | 'running'
  | 'saving'
  | 'exited'
  | 'error';
export type RuntimeRequestCommand =
  'list' | 'read' | 'write' | 'remove' | 'resources' | 'retry-music';
export type RuntimeFileCommand = 'list' | 'read' | 'write' | 'remove';
/** Exclusive access to the existing prepared Runtime filesystem, never another writer. */
export interface RuntimeFileSession {
  readonly epoch: number;
  send<C extends RuntimeFileCommand>(
    command: C,
    payload: RuntimeCommandPayloads[C],
  ): Promise<RuntimeResponseMessage>;
  sync(): Promise<void>;
  /** Retire this native owner, restore the captured plan and keep the same lock. */
  restart(options?: { sync?: boolean }): Promise<RuntimeFileSession>;
  /** Imported bytes already have native persistence acknowledgment. */
  retire(): Promise<void>;
}
export type RuntimeInputCommand =
  | 'keyboard'
  | 'keyboard-clear'
  | 'touch-controls'
  | 'touch-cancel'
  | 'direct-touch'
  | 'thprac-mouse'
  | 'network-cancel';
export interface RuntimeSnapshot {
  readonly phase: RuntimePhase;
  readonly fileOperationBusy: boolean;
  /** Current token's authored plan role; presentation only, never an RPC bypass. */
  readonly fileOnly: boolean;
  readonly game: GameId | null;
  readonly runtimeVariant?: RuntimePlan['runtimeVariant'];
  readonly epoch: number | null;
  readonly generationId: string | null;
  readonly codeGeneration: string | null;
  readonly source: string | null;
  readonly ready: boolean;
  readonly launched: boolean;
  readonly firstFrame: boolean;
  /** Main's nonfatal diagnostic watchdog is separate from caller-requested waits. */
  readonly firstFrameTimedOut: boolean;
  /** Epoch-scoped engine dry run; never a gameplay/save session. */
  readonly multiplayerPreflight?: boolean;
  /** Launch-scoped effective choice, distinct from durable user preferences. */
  readonly music?: 'ogg' | 'midi' | 'none' | null;
  readonly musicWarning?: string | null;
  readonly spectator: boolean;
  readonly error: string | null;
  readonly saveError: string | null;
  /** The native document can no longer service sync; a retained epoch is cleanup-only. */
  readonly saveUnavailable: boolean;
  /** Failure to retire the frame, distinct from failure to persist Runtime data. */
  readonly closeError: string | null;
  readonly saveRoot: string | null;
  readonly scoreFile: string | null;
  readonly configFiles: readonly string[];
  readonly runtimeInfo: Readonly<RuntimeEventPayloads['runtime-info']>;
  /** Native Runtime report only; validating/mirroring/fixing room timing is outside this seam. */
  readonly netplayTiming: unknown;
  readonly progress: Readonly<RuntimeEventPayloads['transfer']> | null;
  readonly frameHealth: Readonly<RuntimeEventPayloads['frame-health']> | null;
  readonly audioHealth: Readonly<RuntimeEventPayloads['audio-health']> | null;
  readonly exit: Readonly<RuntimeEventPayloads['exit']> | null;
}
export interface RuntimeDependencies {
  prepareCode: typeof prepareRuntimeLaunch;
  readData: typeof readManagedRuntimeData;
  readResource: typeof readManagedRuntimeResource;
  retainGeneration: typeof retainPackageGeneration;
  releaseGeneration: typeof releasePackageGeneration;
}
export type Timer = ReturnType<typeof setTimeout>;
export interface RuntimeTimers {
  setTimeout(callback: () => void, ms: number): Timer;
  clearTimeout(timer: Timer): void;
  setInterval(callback: () => void, ms: number): Timer;
  clearInterval(timer: Timer): void;
}
export interface RuntimeLocalResourceProgress {
  epoch: number;
  game: GameId;
  generationId: string;
  phase: 'initial' | 'remaining';
  loaded: number;
  total: number;
  completed: number;
  files: number;
  speed: number;
}
export interface RuntimeServiceOptions {
  translate: Translate;
  frame: RuntimeFrame;
  hostWindow?: RuntimeHost;
  /** Explicit application mount, independent of a Router's current URL. */
  baseUrl: string;
  fetchImpl?: typeof fetch;
  worker?: NonNullable<Parameters<typeof prepareRuntimeLaunch>[1]>['worker'];
  dependencies?: Partial<RuntimeDependencies>;
  timers?: RuntimeTimers;
  timeouts?: {
    ready?: number;
    configure?: number;
    command?: number;
    launch?: number;
    firstFrame?: number;
    lease?: number;
  };
  /** Authenticated source role captured before terminal events reset the snapshot.
   * Callback metadata only; never appended to a Runtime protocol message. */
  onEvent?: (
    message: RuntimeEventMessage,
    context: Readonly<{ epoch: number; fileOnly: boolean }>,
  ) => void;
  onNetworkChange?: (snapshot: NetworkActivitySnapshot) => void;
  /** Optional document-wide tracker; supplying it avoids a second fetch owner. */
  network?: ReturnType<typeof createNetworkActivityTracker>;
  onLocalResourceProgress?: (progress: RuntimeLocalResourceProgress) => void;
  onWarning?: (error: unknown) => void;
  /** Discard the retired child browsing context while retaining its host node. */
  onFrameRetired?: () => void;
}

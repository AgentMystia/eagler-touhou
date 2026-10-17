import { createRuntimeDiagnostics } from '../models/runtime-diagnostics';
import { createGameplayNetwork } from '../models/gameplay-network';
import { createNetplayTelemetry } from '../../src/launcher/netplay-telemetry.mts';
import { describeBrowserEnvironment } from '../../src/launcher/runtime-diagnostics-model.mts';
import type { ProductId } from '@eagler/contracts/product-catalog';
import type { RuntimeService } from '../services/runtime';

interface RuntimeObservationOptions {
  runtime: RuntimeService;
  frame: HTMLIFrameElement;
  window: Window;
  storage: Storage | null;
  mobile: boolean;
  translate: Parameters<typeof createRuntimeDiagnostics>[0]['translate'];
  testBuild(): boolean;
  frameLimit60(): boolean;
  english(): boolean;
  product(): ProductId;
  endpoint: Parameters<typeof createNetplayTelemetry>[0]['endpoint'];
  context: Parameters<typeof createGameplayNetwork>[0]['context'];
}

/** 诊断订阅与遥测时钟随 iframe 会话一起释放，不拥有游戏或房间状态。 */
export function createRuntimeObservation(options: RuntimeObservationOptions) {
  const { runtime, frame, window, storage, mobile, translate } = options;
  let disposed = false;
  const browser = describeBrowserEnvironment({
    userAgent: window.navigator.userAgent,
    platform: window.navigator.platform,
    userAgentDataPlatform: (
      window.navigator as Navigator & { userAgentData?: { platform?: string } }
    ).userAgentData?.platform,
    mobile,
    brave: !!(window.navigator as Navigator & { brave?: unknown }).brave,
  });
  const diagnostics = createRuntimeDiagnostics({
    runtime,
    frame,
    window,
    storage,
    translate,
    browser: () => browser.browser,
    testBuild: options.testBuild,
    frameLimit60: options.frameLimit60,
  });
  const gameplayNetwork = createGameplayNetwork({
    runtime,
    timers: window,
    english: options.english,
    translate,
    context: options.context,
  });
  const telemetry = createNetplayTelemetry({
    origin: window.location.origin,
    endpoint: options.endpoint,
    snapshot: () => {
      const state = runtime.getSnapshot(),
        token = runtime.getMidiEventContext();
      const net = gameplayNetwork.readNativeSnapshot(),
        config = runtime.getLauncherControlContext()?.options;
      if (
        disposed ||
        !state.launched ||
        !token ||
        !net?.active ||
        net.failed ||
        net.frame == null ||
        net.frame < 0 ||
        net.frame === 4294967295
      )
        return null;
      return {
        identity: token.document,
        product: options.product(),
        players: config?.netplayPlayerCount ?? 2,
        spectator: net.spectator,
        transport: net.transport,
        peers: net.peerState?.peers ?? [],
      };
    },
  });
  const timer = window.setInterval(() => {
    void telemetry.tick();
  }, 1000);
  return {
    diagnostics,
    gameplayNetwork,
    refresh() {
      diagnostics.refresh();
      gameplayNetwork.refresh();
    },
    dispose() {
      if (disposed) return;
      disposed = true;
      window.clearInterval(timer);
      diagnostics.dispose();
      gameplayNetwork.dispose();
    },
  };
}

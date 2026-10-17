import { LaunchActions } from './components/launcher/LaunchActions';
import { useEffect, useLayoutEffect, useMemo, useRef, useState, useSyncExternalStore } from 'react';
import { useLocation } from 'react-router';
import {
  gameIdForProduct,
  isMultiplayerProductId,
  PRODUCT_IDS,
} from '@eagler/contracts/product-catalog';
import { buildMultiplayerDiagnosticRelayUrl } from '../src/launcher/multiplayer-relay-url.mts';
import { generateMultiplayerRoomCode } from '../src/launcher/multiplayer-identity.mts';
import { errorText } from './services/error-text';
import { readReactAppShellDeployment } from './services/app-shell-deployment';
import { siteOriginMigrationAvailable } from '@eagler/contracts/site-manifest';
import { LauncherApplication } from './LauncherApplication';
import type { LauncherModels, LauncherViews, LauncherDirectorySlots } from './launcher-bindings';
import { createBrowserSession, type BrowserSession } from './session/browser-session';
import { RoomComposition } from './session/RoomComposition';
import {
  RoomSettingsDrawer,
  MultiplayerSettingsControls,
  MultiplayerDiagnostics,
  MultiplayerOnlineFold,
} from './components/room';
import {
  LauncherMasthead,
  MainSelect,
  SiteFooter,
  createLibraryProducts,
} from './components/launcher';
import { LobbyDirectoryView } from './components/directory/LobbyDirectoryView';
import {
  NetworkDiagnosticsDialog,
  type NetworkDiagnosticsHandle,
} from './components/directory/NetworkDiagnosticsDialog';
import { DirectoryRoomDialog } from './components/directory/DirectoryRoomDialog';
import {
  FirstUseNotice,
  SiteNotice,
  DonationDialog,
  AppleRefreshDialog,
  MultiplayerGuideDialog,
  ReplayDialog,
  type FirstUseNoticeHandle,
  type SiteNoticeHandle,
} from './components/notices';
import { GameDataImportWindows } from './components/player/GameDataImportWindows';
import { MultiplayerQuickChat } from './components/player/MultiplayerQuickChat';
import { PlayerControls } from './components/player/PlayerControls';
import { FullscreenTransient } from './components/FullscreenTransient';
import { StartupError } from './components/feedback/StartupError';
import { InitialLauncher } from './components/startup/InitialLauncher';
import { Feedback } from './components/feedback/Feedback';
import { useSurfaceNavigation } from './navigation/surface-navigation';
import { launcherBaseUrl, routerDestination } from './navigation/addresses';
import { useLocale } from './i18n';

/** Created after hydration, so SPA prerender never touches storage/native APIs. */
export function BrowserLauncher() {
  const entryLocation = useLocation();
  const [session, setSession] = useState<BrowserSession | null>(null);
  useEffect(() => {
    const owner = createBrowserSession({
      document,
      window,
      baseUrl: launcherBaseUrl(window.location.href, entryLocation.pathname),
      appShellDeployment: readReactAppShellDeployment(document, window.location),
    });
    setSession(owner);
    return () => {
      void owner.dispose();
    };
  }, []);
  return session ? <BrowserWorkspace session={session} /> : <InitialLauncher />;
}
function BrowserWorkspace({ session }: { session: BrowserSession }) {
  const state = useSyncExternalStore(session.subscribe, session.getSnapshot, session.getSnapshot);
  const metadata = useSyncExternalStore(
    session.metadata.subscribe,
    session.metadata.getSnapshot,
    session.metadata.getSnapshot,
  );
  const preferences = useSyncExternalStore(
    session.preferences.subscribe,
    session.preferences.getSnapshot,
    session.preferences.getSnapshot,
  );
  const firstUse = useRef<FirstUseNoticeHandle>(null),
    notice = useRef<SiteNoticeHandle>(null),
    network = useRef<NetworkDiagnosticsHandle>(null);
  const [networkRunning, setNetworkRunning] = useState(false);
  const [donationAvailable, setDonationAvailable] = useState(true);
  const [settingsClosing, setSettingsClosing] = useState(false);
  const assetUrl = useMemo(() => (path: string) => new URL(path, session.baseUrl).href, [session]);
  const products = useMemo(
    () => createLibraryProducts(metadata.products, assetUrl, (id) => `?game=${id}`),
    [metadata.products, assetUrl],
  );
  const catalogProducts = useMemo(
    () => createLibraryProducts(PRODUCT_IDS, assetUrl, (id) => `?game=${id}`),
    [assetUrl],
  );
  const masthead = (
    <Header
      session={session}
      firstUse={firstUse}
      notice={notice}
      donationAvailable={donationAvailable}
    />
  );
  const models: LauncherModels = {
    locale: state.locale,
    lessMotion: preferences.lessMotion,
    catalog: {
      products,
      allProducts: catalogProducts,
      available: session.productAvailable,
      reportSelection: session.reportProductSelection,
      select: session.selectRouteProduct,
    },
    settings: { model: session.settings, actions: session.settingsActions },
    touch: {
      model: session.touchLayout,
      native: session.nativeTouch,
      fireEnabled: session.touchFireState.getEnabled(),
      bootPreview: state.bootTouchPreview !== null,
      bootPreviewImage: state.bootTouchPreviewImage,
    },
    player: {
      open: state.playerOpen,
      launched: state.runtime?.launched === true,
      titleOverlayOpen: state.th09NetworkOverlayOpen,
      frame: session.attachFrame,
      requestClose: session.requestRuntimeClose,
    },
    room: { requestLeave: session.requestRoomLeave },
    decisions: session.decisions,
  };
  const views: LauncherViews = {
    assetUrl,
    lobbyHref: assetUrl('lobby.html'),
    library: {
      masthead,
      footer: <Footer donationAvailable={donationAvailable} />,
      serverStatusNote: <ServerStatusNote session={session} />,
    },
    options: {
      actions: <SessionLaunchActions session={session} />,
      multiplayerControls: <RoomSettingsControls session={session} />,
      diagnostics: <InlineMultiplayerDiagnostics session={session} />,
      online: <LegacyMultiplayerOnline session={session} />,
    },
    room: {
      surface: <RoomComposition session={session} settingsClosing={settingsClosing} />,
      drawer: <RoomDrawer session={session} onClosingChange={setSettingsClosing} />,
    },
    playerControls: (
      <>
        <PlayerControls session={session} />
        <MultiplayerQuickChat model={session.room.quickChat} />
      </>
    ),
    renderLobby: (props) => (
      <Directory session={session} {...props} network={network} networkRunning={networkRunning} />
    ),
    overlays: (
      <SessionOverlays
        session={session}
        firstUse={firstUse}
        notice={notice}
        assetUrl={assetUrl}
        onDonationUnavailable={() => setDonationAvailable(false)}
        network={network}
        onNetworkRunning={setNetworkRunning}
      />
    ),
  };
  return <LauncherApplication models={models} views={views} />;
}
const emptySubscribe = () => () => {};
const noDiagnostics = () => null;
function InlineMultiplayerDiagnostics({ session }: { session: BrowserSession }) {
  const navigation = useSurfaceNavigation();
  if (navigation.context === 'lobby') return null;
  return (
    <MultiplayerDiagnostics
      model={session.networkDiagnostics}
      onGuide={() => navigation.openInfoDialog('mpGuideDialog')}
      getRelayUrl={() => {
        const relay = session.metadata.getSnapshot().siteManifest?.shared.netplayRelay;
        return relay ? buildMultiplayerDiagnosticRelayUrl(relay) : '';
      }}
    />
  );
}
function LegacyMultiplayerOnline({ session }: { session: BrowserSession }) {
  const settings = useSyncExternalStore(
    session.settings.subscribe,
    session.settings.getSnapshot,
    session.settings.getSnapshot,
  );
  const metadata = useSyncExternalStore(
    session.metadata.subscribe,
    session.metadata.getSnapshot,
    session.metadata.getSnapshot,
  );
  const { t } = useLocale();
  const product = settings?.context.productId;
  const enter = (code: string, created: boolean) => {
    if (product && isMultiplayerProductId(product))
      session.room.service.enterRoom(product, code, created);
  };
  const configuration = metadata.siteManifest?.shared.netplayRelay
    ? 'ready'
    : metadata.siteManifest || metadata.siteManifestError
      ? 'missing'
      : 'loading';
  return (
    <MultiplayerOnlineFold
      configuration={configuration}
      onCreate={() => {
        enter(generateMultiplayerRoomCode(), true);
      }}
      onJoin={(code) => enter(code, false)}
      onInvalidCode={() => session.feedback.toast(t('status.enterRoomCode'))}
    />
  );
}
function RoomSettingsControls({ session }: { session: BrowserSession }) {
  const settings = useSyncExternalStore(
    session.settings.subscribe,
    session.settings.getSnapshot,
    session.settings.getSnapshot,
  );
  if (!settings || !isMultiplayerProductId(settings.context.productId)) return null;
  return (
    <MultiplayerSettingsControls
      onReplayViewer={() => {
        void session.openReplayViewer(settings.context.productId).catch(session.reportError);
      }}
      onImport={session.gameData.openManual}
    />
  );
}
function RoomDrawer({
  session,
  onClosingChange,
}: {
  session: BrowserSession;
  onClosingChange(value: boolean): void;
}) {
  const navigation = useSurfaceNavigation(),
    state = useSyncExternalStore(session.subscribe, session.getSnapshot, session.getSnapshot);
  return (
    <RoomSettingsDrawer
      roomOpen={
        navigation.roomCode !== null &&
        navigation.productId !== null &&
        isMultiplayerProductId(navigation.productId)
      }
      open={navigation.roomSettingsOpen}
      onCloseRequest={navigation.closeRoomSettings}
      foregroundActive={navigation.surface === 'touch' || state.playerOpen}
      onClosingChange={onClosingChange}
    />
  );
}
function Header({
  session,
  firstUse,
  notice,
  donationAvailable,
}: {
  session: BrowserSession;
  firstUse: React.RefObject<FirstUseNoticeHandle | null>;
  notice: React.RefObject<SiteNoticeHandle | null>;
  donationAvailable: boolean;
}) {
  const navigation = useSurfaceNavigation(),
    { locale, t } = useLocale();
  const prefs = useSyncExternalStore(
    session.preferences.subscribe,
    session.preferences.getSnapshot,
    session.preferences.getSnapshot,
  );
  const metadata = useSyncExternalStore(
    session.metadata.subscribe,
    session.metadata.getSnapshot,
    session.metadata.getSnapshot,
  );
  const diagnostics = session.getDiagnostics();
  const diagnosticState = useSyncExternalStore(
    diagnostics?.subscribe ?? emptySubscribe,
    diagnostics?.getSnapshot ?? noDiagnostics,
    noDiagnostics,
  );
  const updatedAge = useSyncExternalStore(
    session.appShell.brandAge.subscribe,
    session.appShell.brandAge.getSnapshot,
    session.appShell.brandAge.getSnapshot,
  );
  const [menuOpen, setMenuOpen] = useState(false),
    url = (path: string) => new URL(path, session.baseUrl).href;
  return (
    <LauncherMasthead
      updatedAge={updatedAge}
      variant={navigation.context}
      assetUrl={url}
      launcherHref={url(locale === 'en' ? 'en.html' : './')}
      faqHref={url('faq.html')}
      aboutHref={url('about.html')}
      migrationPolicyKnown={metadata.siteManifest !== null}
      migrationHref={
        metadata.siteManifest &&
        siteOriginMigrationAvailable(metadata.siteManifest, window.location.protocol)
          ? url('migrate.html')
          : undefined
      }
      menuOpen={menuOpen}
      onMenuOpenChange={setMenuOpen}
      lessMotion={prefs.lessMotion}
      onToggleMotion={session.preferences.toggleMotion}
      noticeEnabled={prefs.noticeEnabled}
      onToggleNotice={session.preferences.toggleNotice}
      diagnosticsEnabled={diagnosticState?.enabled ?? false}
      onToggleDiagnostics={() => diagnostics?.toggle()}
      donationAvailable={donationAvailable}
      onDonation={() => navigation.openInfoDialog('donationDialog')}
      onFirstUse={() => {
        void firstUse.current?.showManual();
      }}
      languageControl={
        <MainSelect
          id="uiLanguageSelect"
          className="option-select ui-language-select"
          data-trigger-i18n="ui.language.menu"
          aria-label={t('ui.language')}
          value={locale}
          onChange={(event) => session.setLocale(event.target.value === 'en' ? 'en' : 'zh-CN')}
        >
          <option value="zh-CN">{t('ui.language.zhCN')}</option>
          <option value="en">{t('ui.language.en')}</option>
        </MainSelect>
      }
    />
  );
}
function ServerStatusNote({ session }: { session: BrowserSession }) {
  const { status } = useSyncExternalStore(
    session.appShell.subscribe,
    session.appShell.getSnapshot,
    session.appShell.getSnapshot,
  );
  return (
    <div
      className="server-status-note"
      id="serverStatusNote"
      role="status"
      aria-live="polite"
      hidden={!status.text}
      data-kind={status.text ? status.kind : undefined}
    >
      {status.text}
    </div>
  );
}
function Footer({ donationAvailable }: { donationAvailable: boolean }) {
  const navigation = useSurfaceNavigation();
  return (
    <SiteFooter
      donationAvailable={donationAvailable}
      onDonation={() => navigation.openInfoDialog('donationDialog')}
    />
  );
}
function Directory({
  session,
  network,
  ...props
}: LauncherDirectorySlots & {
  session: BrowserSession;
  network: React.RefObject<NetworkDiagnosticsHandle | null>;
  networkRunning: boolean;
}) {
  const navigation = useSurfaceNavigation();
  const directory = useSyncExternalStore(
    session.directory.subscribe,
    session.directory.getSnapshot,
    session.directory.getSnapshot,
  );
  useEffect(() => {
    let active = true;
    queueMicrotask(() => {
      if (active) session.directory.resumeView();
    });
    const pageHide = () => session.directory.pageHide(),
      pageShow = (event: PageTransitionEvent) => session.directory.pageShow(event.persisted);
    const visible = () => session.directory.visibilityChanged(!document.hidden);
    window.addEventListener('pagehide', pageHide);
    window.addEventListener('pageshow', pageShow);
    window.addEventListener('online', session.directory.online);
    window.addEventListener('offline', session.directory.offline);
    document.addEventListener('visibilitychange', visible);
    const connection = (navigator as Navigator & { connection?: EventTarget }).connection;
    connection?.addEventListener('change', session.directory.networkChanged);
    return () => {
      active = false;
      session.directory.pageHide();
      window.removeEventListener('pagehide', pageHide);
      window.removeEventListener('pageshow', pageShow);
      window.removeEventListener('online', session.directory.online);
      window.removeEventListener('offline', session.directory.offline);
      document.removeEventListener('visibilitychange', visible);
      connection?.removeEventListener('change', session.directory.networkChanged);
    };
  }, [session]);
  useEffect(() => {
    if (navigation.filterProductId && isMultiplayerProductId(navigation.filterProductId))
      session.directory.selectProduct(navigation.filterProductId);
  }, [session, navigation.filterProductId]);
  return (
    <LobbyDirectoryView
      {...props}
      initialRows={document.documentElement.hasAttribute('data-lobby-boot')}
      service={session.directory}
      products={createLibraryProducts(
        directory.products,
        (path) => new URL(path, session.baseUrl).href,
        (id) => `?game=${id}`,
      )}
      assetUrl={(path) => new URL(path, session.baseUrl).href}
      onGuide={() => navigation.openInfoDialog('mpGuideDialog')}
      onNetwork={() => {
        navigation.openInfoDialog('lobbyNetworkDialog');
        void network.current?.run();
      }}
      onOpenForm={(mode) => {
        if (session.directory.prepareForm(mode)) navigation.openDirectoryForm(mode);
      }}
    />
  );
}
function SessionLaunchActions({ session }: { session: BrowserSession }) {
  const navigation = useSurfaceNavigation(),
    { t } = useLocale();
  const state = useSyncExternalStore(session.subscribe, session.getSnapshot, session.getSnapshot);
  const settings = useSyncExternalStore(
    session.settings.subscribe,
    session.settings.getSnapshot,
    session.settings.getSnapshot,
  );
  if (!settings || settings.multiplayer || navigation.context === 'lobby') return null;
  return (
    <LaunchActions
      primary={{
        id: 'launch',
        textId: 'launchText',
        label: t(
          session.metadata.getSnapshot().siteManifest?.shared.resourceMode === 'import' &&
            !session.metadata.getSnapshot().installed.has(settings.gameId)
            ? 'action.importGameData'
            : settings.multiplayer
              ? 'action.startMultiplayer'
              : 'action.start',
        ),
        onClick: () => {
          if (navigation.productId)
            void session.launch(navigation.productId).catch(session.reportError);
        },
      }}
      secondary={{
        id: 'gamePackageImport',
        label: t('action.import'),
        onClick: session.gameData.openManual,
      }}
    />
  );
}
function SessionOverlays({
  session,
  firstUse,
  notice,
  assetUrl,
  onDonationUnavailable,
  network,
  onNetworkRunning,
}: {
  session: BrowserSession;
  firstUse: React.RefObject<FirstUseNoticeHandle | null>;
  notice: React.RefObject<SiteNoticeHandle | null>;
  assetUrl(path: string): string;
  onDonationUnavailable(): void;
  network: React.RefObject<NetworkDiagnosticsHandle | null>;
  onNetworkRunning(running: boolean): void;
}) {
  const navigation = useSurfaceNavigation(),
    location = useLocation(),
    { t } = useLocale();
  const state = useSyncExternalStore(session.subscribe, session.getSnapshot, session.getSnapshot);
  const directory = useSyncExternalStore(
    session.directory.subscribe,
    session.directory.getSnapshot,
    session.directory.getSnapshot,
  );
  // Main lobby.mts113 uses the directory filter; its shared Launcher settings
  // selection may belong to another product or a late Host fallback.
  const guideProduct =
    navigation.context === 'lobby' ? directory.selectedProduct : navigation.productId;
  const replays = session.getReplays();
  const releaseNavigation = useRef<(() => void) | null>(null);
  useLayoutEffect(() => {
    // A child dialog completes its close in the same layout phase. Keep the
    // last committed Router port usable until this atomic replacement instead
    // of exposing a detached session between every route cleanup and setup.
    releaseNavigation.current = session.bindNavigation({
      ...navigation,
      navigateToRoom: (url) => {
        navigation.navigateAddress(routerDestination(url, session.baseUrl));
      },
    });
  }, [session, navigation]);
  useLayoutEffect(
    () => () => {
      releaseNavigation.current?.();
      releaseNavigation.current = null;
    },
    [session],
  );
  // Initial room restoration writes the original replace/push seed. Wait for
  // the Router's own layout activation before using its navigation port; the
  // session still restores membership synchronously before requesting Host.
  useEffect(() => {
    void session.initialize().catch(session.reportError);
  }, [session]);
  useEffect(() => {
    document.body.classList.toggle('player-active', state.playerOpen);
    return () => {
      document.body.classList.remove('player-active');
    };
  }, [state.playerOpen]);
  useLayoutEffect(
    () =>
      session.startup.attach({
        context: navigation.context,
        url: window.location.href,
        boot: (window as Window & { __eaglerBoot?: { ready(): void } }).__eaglerBoot ?? {
          ready() {
            throw new Error('Original launcher bootstrap is not installed');
          },
        },
        waitDirectoryRendered: () =>
          new Promise<void>((resolve) => {
            if (session.directory.getSnapshot().initialized) return resolve();
            const unsubscribe = session.directory.subscribe(() => {
              if (session.directory.getSnapshot().initialized) {
                unsubscribe();
                resolve();
              }
            });
          }),
        warnDiscouragedBrowser: session.warnDiscouragedBrowser,
        showFirstUseAutomatically: (isCurrent) =>
          firstUse.current?.maybeShowAutomatically(isCurrent) ?? Promise.resolve(false),
        loadSiteNotice: () => notice.current?.load() ?? Promise.resolve(false),
        onError: session.reportError,
      }),
    [session, navigation.context, firstUse, notice],
  );
  useEffect(() => {
    void session.restoreRoomFromUrl(window.location.href).catch(session.reportError);
  }, [session, navigation.roomCode, navigation.productId]);
  useLayoutEffect(
    () => session.settleRoomReturnPresentation(navigation.surface, navigation.productId),
    [session, navigation.surface, navigation.productId],
  );
  useLayoutEffect(() => {
    if (navigation.directoryFormMode)
      session.directory.restoreFormMode(navigation.directoryFormMode);
  }, [session, navigation.directoryFormMode]);
  return (
    <>
      <input id="fileInput" type="file" hidden />
      <FullscreenTransient>
        <Feedback model={session.feedback} />
        <StartupError model={session.startupError} />
        <GameDataImportWindows
          model={session.gameData}
          getRuntimeReady={() => session.getRuntime()?.getSnapshot().ready === true}
        />
      </FullscreenTransient>
      {navigation.context === 'lobby' && (
        <NetworkDiagnosticsDialog
          model={session.networkDiagnostics}
          ref={network}
          open={navigation.infoDialogOpen('lobbyNetworkDialog')}
          onCloseRequest={() => navigation.closeInfoDialog('lobbyNetworkDialog')}
          getRelayUrl={() => {
            const relay = session.directory.getSnapshot().siteManifest?.shared.netplayRelay;
            return relay ? buildMultiplayerDiagnosticRelayUrl(relay) : '';
          }}
          onRunningChange={onNetworkRunning}
        />
      )}
      {navigation.context === 'lobby' && (
        <DirectoryRoomDialog
          service={session.directory}
          open={navigation.directoryFormMode !== null}
          onCloseRequest={navigation.closeDirectoryForm}
          completionContext={{
            locationKey: location.key,
            parentKey: navigation.directoryFormParentKey,
            href: location.pathname + location.search + location.hash,
          }}
        />
      )}
      <FirstUseNotice
        ref={firstUse}
        open={navigation.infoDialogOpen('firstUseNoticeDialog')}
        onCloseRequest={() => navigation.closeInfoDialog('firstUseNoticeDialog')}
        onOpenRequest={() => navigation.openInfoDialog('firstUseNoticeDialog')}
        storage={session.storage}
        contentUrl={assetUrl('content/FIRST_USE_NOTICE.html')}
        edgeGestures={navigation.context === 'library'}
      />
      <SiteNotice
        ref={notice}
        preferences={session.preferences}
        assetUrl={assetUrl}
        visible={
          navigation.context === 'library' && navigation.surface === 'library' && !state.playerOpen
        }
        edgeGestures={navigation.context === 'library' && navigation.surface === 'library'}
        onOptOut={session.feedback.toast}
      />
      <DonationDialog
        open={navigation.infoDialogOpen('donationDialog')}
        onCloseRequest={() => navigation.closeInfoDialog('donationDialog')}
        closeDurationMs={navigation.context === 'lobby' ? 0 : 220}
        assetUrl={assetUrl}
        onArtworkUnavailable={onDonationUnavailable}
      />
      <AppleRefreshDialog
        open={navigation.infoDialogOpen('appleRefreshDialog')}
        onCloseRequest={() => navigation.closeInfoDialog('appleRefreshDialog')}
      />
      <MultiplayerGuideDialog
        open={navigation.infoDialogOpen('mpGuideDialog')}
        onCloseRequest={() => navigation.closeInfoDialog('mpGuideDialog')}
        gameId={guideProduct ? gameIdForProduct(guideProduct) : 'th06'}
        contentUrl={assetUrl('content/MULTIPLAYER.html')}
      />
      {replays && (
        <ReplayDialog
          model={replays}
          open={navigation.infoDialogOpen('replayDialog')}
          onCloseRequest={() => navigation.closeInfoDialog('replayDialog')}
          onImportRequest={() => {
            const product = replays.getSnapshot().productId;
            if (product) void session.chooseReplayFile(product).catch(session.reportError);
          }}
          onDropFile={async (file) => {
            const product = replays.getSnapshot().productId;
            if (product) await session.importFile('replay', file, product);
          }}
          onImportError={(error) => {
            session.feedback.status(t('status.errorReason', { reason: errorText(error) }));
            session.feedback.toast(t('replay.importFailed', { reason: errorText(error) }));
          }}
        />
      )}
    </>
  );
}

import { useEffect, useLayoutEffect, useRef, useState, useSyncExternalStore } from 'react';
import {
  isProductId,
  isMultiplayerProductId,
  type ProductId,
} from '@eagler/contracts/product-catalog';
import { LocaleProvider, translate } from './i18n';
import { PlayerSurface } from './components/player/PlayerSurface';
import { LibrarySurface } from './components/launcher/LibrarySurface';

import { OptionsPanel } from './components/launcher/OptionsPanel';
import { LobbyOptionsHost } from './components/launcher/LobbyOptionsHost';
import { OptionsOwnership } from './components/launcher/options-ownership';
import { useLibraryOptionsPresence } from './components/launcher/use-library-options-presence';
import { SettingsBody } from './components/settings/SettingsBody';
import { TouchLayoutEditor } from './components/settings/TouchLayoutEditor';
import { SurfaceNavigationProvider, useSurfaceNavigation } from './navigation/surface-navigation';

import type { LauncherModels, LauncherViews, LauncherDirectorySlots } from './launcher-bindings';

export function LauncherApplication({
  models,
  views,
}: {
  models: LauncherModels;
  views: LauncherViews;
}) {
  const layout = useSyncExternalStore(
    models.touch.model.subscribe,
    models.touch.model.getSnapshot,
    models.touch.model.getSnapshot,
  );
  return (
    <LocaleProvider locale={models.locale}>
      <SurfaceNavigationProvider
        dirty={layout.dirty}
        isEditing={layout.isEditing}
        onDiscard={models.touch.model.discard}
        decisions={models.decisions}
        playerOpen={models.player.open}
        requestRuntimeClose={models.player.requestClose}
        requestRoomLeave={models.room.requestLeave}
        titleOverlayOpen={models.player.titleOverlayOpen}
        productAvailable={models.catalog.available}
        onRouteSelection={models.catalog.select}
      >
        <LauncherSurfaces models={models} views={views} />
      </SurfaceNavigationProvider>
    </LocaleProvider>
  );
}

function LauncherSurfaces({ models, views }: { models: LauncherModels; views: LauncherViews }) {
  const navigation = useSurfaceNavigation();
  const settings = useSyncExternalStore(
    models.settings.model.subscribe,
    models.settings.model.getSnapshot,
    models.settings.model.getSnapshot,
  );
  const layout = useSyncExternalStore(
    models.touch.model.subscribe,
    models.touch.model.getSnapshot,
    models.touch.model.getSnapshot,
  );
  const [preview, setPreview] = useState<string | undefined>(
    navigation.productId ?? models.catalog.products[0]?.id,
  );
  const [playerElement, setPlayerElement] = useState<HTMLElement | null>(null);
  const roomOpen =
    navigation.roomCode !== null &&
    navigation.productId !== null &&
    isMultiplayerProductId(navigation.productId);
  const catalog = models.catalog.allProducts;
  const optionsOpen = !roomOpen && navigation.hasSelection;
  const retainedProduct = useRef<ProductId | undefined>(models.catalog.products[0]?.id);
  if (navigation.productId && catalog.some((product) => product.id === navigation.productId))
    retainedProduct.current = navigation.productId;
  // Keep the selected panel mounted until its closing transition finishes.
  const selectedProduct = catalog.find((product) => product.id === retainedProduct.current);
  const presence = useLibraryOptionsPresence({
    open: optionsOpen,
    context: navigation.context,
    product: selectedProduct,
    lessMotion: models.lessMotion === true,
    concealed: models.player.launched === true && navigation.surface !== 'touch',
  });
  useEffect(() => {
    // Draft lifetime ends only after Router commits leaving the editor. A
    // cancelled blocker leaves both the surface and draft intact.
    if (navigation.surface !== 'touch' && layout.isEditing) {
      models.touch.model.discard();
      models.settings.actions.feedback(translate(models.locale, 'touch.layoutEditorClosed'));
    }
  }, [navigation.surface, layout.isEditing, models.touch.model]);
  useLayoutEffect(() => {
    document.body.classList.toggle('lobby-page', navigation.context === 'lobby');
    document.body.classList.toggle('mp-room-active', roomOpen);
    return () => {
      document.body.classList.remove('lobby-page', 'mp-room-active');
    };
  }, [navigation.context, optionsOpen, roomOpen]);
  const activate = (id: string) => {
    if (!isProductId(id)) return;
    if (navigation.context === 'library' && isMultiplayerProductId(id))
      navigation.openDirectory(id);
    else {
      const previous = models.settings.model.getSnapshot()?.context.productId;
      navigation.openOptions(id);
      if (navigation.context === 'library' && !isMultiplayerProductId(id)) {
        models.catalog.reportSelection(id, previous !== id);
      }
      requestAnimationFrame(() =>
        presence.targets.panel
          ?.querySelector<HTMLElement>('#libraryBack')
          ?.focus({ preventScroll: true }),
      );
    }
  };
  const openTouchLayout = () => {
    if (models.player.launched) {
      const reason = translate(models.locale, 'touch.editWhileRunning');
      models.settings.actions.feedback(
        reason,
        translate(models.locale, 'status.errorReason', { reason }),
      );
      return;
    }
    navigation.openTouchLayout();
  };
  const settingsPanel = presence.product && (
    <OptionsPanel
      product={presence.product}
      open={presence.open || (roomOpen && navigation.context === 'lobby')}
      onBack={roomOpen ? navigation.closeRoomSettings : navigation.closeSurface}
      roomDrawer={roomOpen}
      multiplayerDiagnostics={views.options.diagnostics}
      multiplayerOnline={views.options.online}
      lobbyHref={views.lobbyHref}
      embeddedInLobby={navigation.context === 'lobby'}
      assetUrl={views.assetUrl}
      actions={views.options.actions}
    >
      <SettingsBody
        model={models.settings.model}
        actions={models.settings.actions}
        onOpenTouchLayout={openTouchLayout}
        multiplayerControls={views.options.multiplayerControls}
      />
    </OptionsPanel>
  );
  const editor = playerElement && navigation.surface === 'touch' && settings && (
    <TouchLayoutEditor
      playerElement={playerElement}
      model={models.touch.model}
      settings={models.settings.model}
      actions={models.settings.actions}
      native={models.touch.native}
      onCloseIntent={navigation.closeSurface}
      fireEnabled={models.touch.fireEnabled}
      previewImage={
        models.touch.previewImage ??
        (selectedProduct?.artwork ? `url(${JSON.stringify(selectedProduct.artwork)})` : 'none')
      }
    />
  );
  const lobbyProps: LauncherDirectorySlots = {
    masthead: views.library.masthead,
    products: models.catalog.products.filter((product) => product.multiplayer),
    selectedProduct: navigation.filterProductId ?? undefined,
    openedProduct: presence.openedProduct,
    onSelect: (id) => {
      if (isProductId(id)) navigation.selectLobbyProduct(id);
    },
    onActivate: activate,
    roomUsersArtwork: views.assetUrl('assets/room-users.svg'),
    children: (
      <LobbyOptionsHost
        open={presence.open}
        foregroundActive={navigation.surface === 'touch' || models.player.open === true}
        onCloseRequest={navigation.closeSurface}
      >
        {settingsPanel}
      </LobbyOptionsHost>
    ),
  };
  return (
    <OptionsOwnership value={presence.targets}>
      {navigation.context === 'lobby' ? (
        views.renderLobby(lobbyProps)
      ) : (
        <LibrarySurface
          products={models.catalog.products}
          selectedProduct={preview}
          openedProduct={presence.openedProduct}
          onSelect={setPreview}
          onActivate={activate}
          masthead={views.library.masthead}
          footer={views.library.footer}
          serverStatusNote={views.library.serverStatusNote}
          lobbyHref={views.lobbyHref}
          roomUsersArtwork={views.assetUrl('assets/room-users.svg')}
          optionsOpen={presence.open}
          roomOpen={roomOpen}
          roomDrawer={views.room.drawer}
          onBack={navigation.closeSurface}
        >
          {settingsPanel}
          {views.room.surface}
        </LibrarySurface>
      )}
      <PlayerSurface
        preview={models.touch.bootPreview}
        previewImage={models.touch.bootPreviewImage}
        onElement={setPlayerElement}
        onFrame={models.player.frame}
        open={navigation.surface === 'touch' || models.player.open === true}
        editing={navigation.surface === 'touch'}
      >
        {views.playerControls}
      </PlayerSurface>
      {editor}
      {views.overlays}
    </OptionsOwnership>
  );
}

import type { ReactNode, RefCallback } from 'react';
import type { ProductId } from '@eagler/contracts/product-catalog';
import type { UiLocale } from '../src/launcher/i18n.mts';
import type { LibraryProduct } from './components/launcher/products';
import type { LobbySurfaceProps } from './components/launcher/LobbySurface';
import type { SettingsActions, TouchEditorNativePorts } from './components/settings/types';
import type { GameSettingsModel } from './models/game-settings';
import type { TouchLayoutModel } from './models/touch-layout';
import type { DecisionStore } from './models/decisions';
import type { SurfaceContext } from './navigation/surface-navigation';

/** Domain owners supplied by the document session. Views never hydrate settings. */
export interface LauncherModels {
  locale: UiLocale;
  lessMotion: boolean;
  catalog: {
    products: readonly LibraryProduct[];
    allProducts: readonly LibraryProduct[];
    available(product: ProductId): boolean;
    select(product: ProductId, context?: SurfaceContext): void;
    reportSelection(product: ProductId, changed: boolean): void;
  };
  settings: { model: GameSettingsModel; actions: SettingsActions };
  touch: {
    model: TouchLayoutModel;
    native: TouchEditorNativePorts;
    fireEnabled?: boolean;
    previewImage?: string;
    bootPreview?: boolean;
    bootPreviewImage?: string;
  };
  player: {
    open: boolean;
    launched: boolean;
    titleOverlayOpen: boolean;
    requestClose(): Promise<boolean>;
    frame?: RefCallback<HTMLIFrameElement>;
  };
  room: { requestLeave(): void };
  decisions?: DecisionStore;
}

export type LauncherDirectorySlots = Pick<
  LobbySurfaceProps,
  | 'products'
  | 'selectedProduct'
  | 'openedProduct'
  | 'onSelect'
  | 'onActivate'
  | 'children'
  | 'roomUsersArtwork'
  | 'masthead'
>;

/** React composition slots. No storage, navigation or runtime state is owned here. */
export interface LauncherViews {
  assetUrl(path: string): string;
  lobbyHref: string;
  library: { masthead: ReactNode; footer: ReactNode; serverStatusNote?: ReactNode };
  options: {
    actions?: ReactNode;
    multiplayerControls?: ReactNode;
    diagnostics?: ReactNode;
    online?: ReactNode;
  };
  room: { surface?: ReactNode; drawer?: ReactNode };
  renderLobby(props: LauncherDirectorySlots): ReactNode;
  playerControls?: ReactNode;
  overlays?: ReactNode;
}

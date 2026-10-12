import type { Location } from 'react-router';
import {
  isProductId,
  isMultiplayerProductId,
  DEFAULT_MULTIPLAYER_PRODUCT_ID,
  type ProductId,
} from '@eagler/contracts/product-catalog';
import {
  resolveRoomInvite,
  normalizeRoomCode,
  type HistoryOperation,
} from '../../src/launcher/route-state.mts';
import { encodeRoomInvite, ROOM_INVITE_KEY } from '../../src/launcher/room-invite.mts';

export type Surface = 'library' | 'options' | 'touch' | 'room';
export type SurfaceContext = 'library' | 'lobby';
export type RoomPanelKind = 'personal' | 'network' | 'spectators' | 'game';
export type DirectoryFormMode = 'create' | 'join';
export const INFORMATION_DIALOG_IDS = [
  'mpGuideDialog',
  'firstUseNoticeDialog',
  'donationDialog',
  'appleRefreshDialog',
  'replayDialog',
  'lobbyNetworkDialog',
] as const;
export type InformationDialogId = (typeof INFORMATION_DIALOG_IDS)[number];
export interface SurfaceAddress {
  surface: Surface;
  productId: ProductId | null;
  context: SurfaceContext;
  roomCode: string | null;
  fromDirectory: boolean;
}
export function readSurface(location: Pick<Location, 'pathname' | 'search'>): SurfaceAddress {
  const params = new URLSearchParams(location.search);
  const game = params.get('game') || '';
  const productId = isProductId(game) ? game : null;
  const context = /\/(?:lobby|lobby\.html)\/?$/.test(location.pathname) ? 'lobby' : 'library';
  const invite =
    context === 'library'
      ? resolveRoomInvite(new URL(location.pathname + location.search, 'https://launcher.invalid'))
      : null;
  const roomCode = invite ? normalizeRoomCode(invite.r) : '';
  if (invite && roomCode)
    return {
      productId: isMultiplayerProductId(invite.g) ? invite.g : DEFAULT_MULTIPLAYER_PRODUCT_ID,
      context,
      surface: 'room',
      roomCode,
      fromDirectory: invite.f === true,
    };
  const options = productId !== null && context === 'library';
  return {
    productId,
    context,
    surface: options ? 'options' : 'library',
    roomCode: null,
    fromDirectory: false,
  };
}
/** Preserve directory origin only when the normalized room identity is unchanged. */
export function preserveSameRoomDirectoryOrigin(
  operation: HistoryOperation,
  source: string | URL,
): HistoryOperation {
  const prior = resolveRoomInvite(source),
    url = new URL(operation.url),
    next = resolveRoomInvite(url);
  if (
    !prior?.f ||
    !next ||
    prior.g !== next.g ||
    !normalizeRoomCode(next.r) ||
    normalizeRoomCode(prior.r) !== normalizeRoomCode(next.r)
  )
    return operation;
  url.searchParams.set(ROOM_INVITE_KEY, encodeRoomInvite({ g: next.g, r: next.r, f: true }));
  return { ...operation, url: url.href };
}
export const entryKey = 'launcherSurfaceEntry';
export const touchEntryKey = 'launcherTouchEntry';
export const infoEntryKey = 'launcherInformationDialogs';
export const directoryFormKey = 'launcherDirectoryForm';
export const roomPanelKey = 'launcherRoomPanel';
export const roomSettingsKey = 'launcherRoomSettings';
export const roomProductKey = 'launcherRoomProductSelection';
export const hiddenSelectionKey = 'launcherClosedPlayerSelection';
export const gameEntryKey = 'launcherGameEntry';
export const href = (location: Pick<Location, 'pathname' | 'search' | 'hash'>) =>
  location.pathname + location.search + location.hash;
export function plainState(value: unknown): Record<string, unknown> {
  return value && typeof value === 'object' && !Array.isArray(value) ? { ...value } : {};
}
export function withoutEntry(value: unknown) {
  const state = plainState(value);
  delete state[entryKey];
  delete state[touchEntryKey];
  delete state[infoEntryKey];
  delete state[directoryFormKey];
  delete state[roomPanelKey];
  delete state[roomSettingsKey];
  delete state[roomProductKey];
  delete state[hiddenSelectionKey];
  delete state[gameEntryKey];
  return state;
}
export interface HostSelection {
  productId: ProductId;
  hasSelection: boolean;
}

export function resolveSurface(value: Location, dialogSession: string): SurfaceAddress {
  const parsed = readSurface(value),
    hidden = plainState(plainState(value.state)[hiddenSelectionKey]);
  if (hidden.session === dialogSession)
    return {
      ...parsed,
      surface: 'library',
      roomCode: null,
      fromDirectory: false,
      productId:
        typeof hidden.productId === 'string' && isProductId(hidden.productId)
          ? hidden.productId
          : parsed.productId,
    };
  const selected = plainState(plainState(value.state)[roomProductKey]);
  return parsed.surface === 'room' &&
    selected.session === dialogSession &&
    selected.code === parsed.roomCode &&
    selected.requestedProduct ===
      resolveRoomInvite(new URL(href(value), 'https://launcher.invalid'))?.g &&
    typeof selected.productId === 'string' &&
    isMultiplayerProductId(selected.productId)
    ? { ...parsed, productId: selected.productId }
    : parsed;
}

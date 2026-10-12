import type { Location } from 'react-router';
import type { PageHistory } from './page-history';
import { readPageHistory, PAGE_PARENTS } from './page-history';
import { MP_PANEL_HISTORY_KEY, MP_SETTINGS_HISTORY_KEY } from '../../src/launcher/route-state.mts';
import {
  plainState,
  href,
  resolveSurface,
  touchEntryKey,
  entryKey,
  infoEntryKey,
  gameEntryKey,
  type SurfaceAddress,
} from './surface-state';

export type NavigationBlockReason = 'touch' | 'runtime' | 'room' | 'page';
interface NavigationPolicyInput {
  transition: { currentLocation: Location; nextLocation: Location; historyAction: string };
  guards: { dirty: boolean; isEditing: boolean; playerOpen: boolean; titleOverlayOpen: boolean };
  surface: {
    address: SurfaceAddress;
    baseAddress: SurfaceAddress;
    dialogSession: string;
    currentEntry: Record<string, unknown>;
    ownedTouch: boolean;
    ownedOptions: boolean;
    validInfo: boolean;
    validForm: boolean;
    validSettings: boolean;
    validPanel: boolean;
    topGame: boolean;
    surfaceOpenIssuedKey: string | null;
    manualRoomPlayerExit: boolean;
    pageHistory: PageHistory | null;
  };
}

/** Pure departure policy. History writes, dialogs and asynchronous close work
 * remain at their own boundaries; this function returns only the required guard. */
export function navigationBlockReason({
  transition,
  guards,
  surface,
}: NavigationPolicyInput): NavigationBlockReason | null {
  const { currentLocation, nextLocation, historyAction } = transition;
  const { dirty, isEditing, playerOpen, titleOverlayOpen } = guards;
  const {
    address,
    baseAddress,
    dialogSession,
    currentEntry,
    ownedTouch,
    ownedOptions,
    validInfo,
    validForm,
    validSettings,
    validPanel,
    topGame,
    surfaceOpenIssuedKey,
    manualRoomPlayerExit,
    pageHistory,
  } = surface;
  const nextInformation = plainState(plainState(nextLocation.state)[infoEntryKey]);
  const nextTouch = plainState(plainState(nextLocation.state)[touchEntryKey]);
  const nextAddress = resolveSurface(nextLocation, dialogSession);
  const nextOptionsOwner = plainState(plainState(nextLocation.state)[entryKey]);
  const nextOwnedProduct =
    nextOptionsOwner.version === 1 && nextOptionsOwner.href === href(nextLocation)
      ? nextOptionsOwner.productId
      : nextAddress.productId;
  const sameTouch =
    nextTouch.version === currentEntry.version &&
    nextTouch.surface === 'touch' &&
    nextTouch.parentKey === currentEntry.parentKey &&
    nextTouch.href === href(nextLocation) &&
    currentEntry.href === href(currentLocation) &&
    nextAddress.context === address.context &&
    nextOwnedProduct === baseAddress.productId;
  if (dirty && isEditing && ownedTouch && !sameTouch) {
    return 'touch';
  }
  const sameRoom = address.roomCode !== null && nextAddress.roomCode === address.roomCode;
  // Live child dismissals take priority, but a POP into a
  // retired same-room entry still closes the running MP Player.
  const roomChildConsumesPop =
    !topGame &&
    (ownedTouch ||
      validInfo ||
      nextInformation.session === dialogSession ||
      (validSettings && !plainState(nextLocation.state)[MP_SETTINGS_HISTORY_KEY]) ||
      (validPanel && !plainState(nextLocation.state)[MP_PANEL_HISTORY_KEY]));
  const sameRoomPlayerPop = historyAction === 'POP' && sameRoom && !roomChildConsumesPop;
  if (
    titleOverlayOpen &&
    address.surface === 'room' &&
    (!sameRoom || sameRoomPlayerPop) &&
    !manualRoomPlayerExit
  ) {
    return 'room';
  }
  const openingTouch =
    nextTouch.version === 1 &&
    nextTouch.surface === 'touch' &&
    nextTouch.parentKey === currentLocation.key &&
    nextTouch.href === href(nextLocation) &&
    href(currentLocation) === href(nextLocation) &&
    surfaceOpenIssuedKey === currentLocation.key;
  // Settings intent rejects launched Runtime at its real owner. An open
  // preparation/preview Player alone does not mean gameplay is running.
  if (playerOpen && !openingTouch) {
    const nextOptions = plainState(plainState(nextLocation.state)[entryKey]);
    const nextProduct = resolveSurface(nextLocation, dialogSession).productId;
    const sameProductEntry =
      address.surface !== 'room' &&
      nextOptions.version === 1 &&
      nextOptions.surface === 'options' &&
      nextOptions.productId === baseAddress.productId &&
      nextProduct === baseAddress.productId;
    // Informational windows retain the exact underlying native-entry state,
    // including direct options without an owned options marker.
    const sameDirectInfoLayer =
      !ownedOptions &&
      href(currentLocation) === href(nextLocation) &&
      (infoEntryKey in plainState(currentLocation.state) ||
        infoEntryKey in plainState(nextLocation.state));
    const closingGameEntry =
      topGame && historyAction === 'REPLACE' && !(gameEntryKey in plainState(nextLocation.state));
    const leavingGame =
      (topGame && historyAction === 'POP' && !roomChildConsumesPop) || closingGameEntry;
    if (
      leavingGame ||
      (!sameProductEntry && !sameDirectInfoLayer && !sameRoom) ||
      sameRoomPlayerPop
    ) {
      return 'runtime';
    }
  }
  const backwards =
    historyAction === 'POP' &&
    (readPageHistory(nextLocation.state)?.position ?? -1) < (pageHistory?.position ?? 0);
  if (
    !playerOpen &&
    address.roomCode !== null &&
    (!sameRoom || (backwards && !roomChildConsumesPop))
  ) {
    return 'room';
  }
  const currentPage = readPageHistory(currentLocation.state),
    nextPage = readPageHistory(nextLocation.state);
  if (
    historyAction === 'POP' &&
    currentPage &&
    nextPage &&
    nextPage.position < currentPage.position &&
    !playerOpen &&
    !validInfo &&
    !validForm &&
    !ownedTouch &&
    !validPanel &&
    !validSettings
  ) {
    const parent = PAGE_PARENTS[currentPage.page];
    if (
      parent &&
      (nextPage.page !== parent || nextPage.position !== currentPage.ancestors[parent])
    ) {
      return 'page';
    }
  }
  return null;
}

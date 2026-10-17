import { useLayoutEffect, useRef, useState } from 'react';
import type { Location, useNavigationType } from 'react-router';
import {
  DEFAULT_PRODUCT_ID,
  isProductId,
  isMultiplayerProductId,
  type ProductId,
} from '@eagler/contracts/product-catalog';
import {
  href,
  plainState,
  entryKey,
  touchEntryKey,
  infoEntryKey,
  roomPanelKey,
  roomSettingsKey,
  directoryFormKey,
  gameEntryKey,
  type HostSelection,
  type Surface,
  type SurfaceAddress,
  type SurfaceContext,
} from './surface-state';

interface SurfaceSelectionOptions {
  location: Location;
  navigationType: ReturnType<typeof useNavigationType>;
  urlAddress: SurfaceAddress;
  baseAddress: SurfaceAddress;
  pendingLocaleAddress: { sourceKey: string; target: string } | null;
  ownedOptions: boolean;
  ownedTouch: boolean;
  staleTouch: boolean;
  productAvailable(product: ProductId): boolean;
  onRouteSelection?(product: ProductId, context?: SurfaceContext): void;
}

/** Host 选择属于当前文档；地址、临时覆盖层和语言切换按各自寿命合并。 */
export function useSurfaceSelection({
  location,
  navigationType,
  urlAddress,
  baseAddress,
  pendingLocaleAddress,
  ownedOptions,
  ownedTouch,
  staleTouch,
  productAvailable,
  onRouteSelection,
}: SurfaceSelectionOptions) {
  const [hostSelection, setHostSelection] = useState<HostSelection | null>(null);
  const selectionLocation = useRef(location);
  const previousSelectedProduct = useRef<ProductId>(
    urlAddress.productId && productAvailable(urlAddress.productId)
      ? urlAddress.productId
      : DEFAULT_PRODUCT_ID,
  );
  const previousSurface = useRef<Surface>(urlAddress.surface);
  const locationChanged = selectionLocation.current.key !== location.key;
  const previousState = plainState(selectionLocation.current.state),
    nextState = plainState(location.state);
  const sameAddress = href(selectionLocation.current) === href(location);
  const overlayTransition =
    sameAddress &&
    (previousSurface.current === 'touch' ||
      (navigationType === 'PUSH' && touchEntryKey in nextState) ||
      [infoEntryKey, roomPanelKey, roomSettingsKey, directoryFormKey, gameEntryKey].some(
        (key) => JSON.stringify(previousState[key]) !== JSON.stringify(nextState[key]),
      ));
  const localeTransition =
    pendingLocaleAddress?.sourceKey === selectionLocation.current.key &&
    pendingLocaleAddress.target === href(location);
  const retainHostSelection = !locationChanged || overlayTransition || localeTransition;
  const activeHostSelection = retainHostSelection ? hostSelection : null;
  useLayoutEffect(() => {
    if (!locationChanged) return;
    selectionLocation.current = location;
    if (!retainHostSelection) {
      setHostSelection(null);
      const owner = plainState(nextState[entryKey]);
      const selected =
        urlAddress.context === 'lobby' &&
        owner.surface === 'options' &&
        isProductId(String(owner.productId))
          ? (owner.productId as ProductId)
          : urlAddress.productId;
      const freshLobbyOptions =
        urlAddress.context === 'lobby' &&
        owner.surface === 'options' &&
        JSON.stringify(previousState[entryKey]) !== JSON.stringify(nextState[entryKey]);
      if (
        selected &&
        productAvailable(selected) &&
        (freshLobbyOptions || selected !== previousSelectedProduct.current)
      )
        onRouteSelection?.(selected, urlAddress.context);
    }
  }, [location.key]);
  const routeHasSelection =
    baseAddress.surface !== 'library' &&
    !!baseAddress.productId &&
    productAvailable(baseAddress.productId);
  const hasSelection = activeHostSelection?.hasSelection ?? routeHasSelection;
  const selectedProductId =
    activeHostSelection?.productId ??
    (baseAddress.productId &&
    (baseAddress.surface === 'room' || productAvailable(baseAddress.productId))
      ? baseAddress.productId
      : previousSelectedProduct.current);
  const roomVisible =
    baseAddress.roomCode !== null &&
    selectedProductId !== null &&
    isMultiplayerProductId(selectedProductId);
  const address: SurfaceAddress = {
    ...baseAddress,
    productId: selectedProductId,
    surface:
      ownedTouch && !staleTouch
        ? 'touch'
        : roomVisible
          ? 'room'
          : hasSelection
            ? 'options'
            : 'library',
  };
  useLayoutEffect(() => {
    previousSurface.current = address.surface;
    if (address.productId) previousSelectedProduct.current = address.productId;
  }, [address.surface, address.productId, location.key]);
  function applyHostSelection(selection: HostSelection) {
    setHostSelection((previous) =>
      previous?.productId === selection.productId &&
      previous.hasSelection === selection.hasSelection
        ? previous
        : { ...selection },
    );
  }
  function syncSelectionFromRoute(): ProductId | null {
    // Main lobby.mts434–450/795–800 keeps ?game as its directory filter;
    // only the explicit options carrier selects a Launcher settings surface.
    if (baseAddress.context === 'lobby' && !ownedOptions) return null;
    const productId = baseAddress.productId;
    if (!productId || !productAvailable(productId)) return null;
    applyHostSelection({ productId, hasSelection: true });
    if (productId !== address.productId) onRouteSelection?.(productId, address.context);
    return productId;
  }
  return {
    address,
    hasSelection,
    activeHostSelection,
    applyHostSelection,
    syncSelectionFromRoute,
    clearHostSelection: () => setHostSelection(null),
  };
}

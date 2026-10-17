import { PRODUCT_GAMES } from './contracts/product-catalog.mjs';

export const RUNTIME_VARIANTS = Object.freeze([
  Object.freeze({ name: 'normal', field: 'runtime', suffix: '' }),
  Object.freeze({ name: 'multiplayer', field: 'multiplayerRuntime', suffix: '/multiplayer' }),
]);

export function availableRuntimeVariants(entry) {
  return RUNTIME_VARIANTS.filter(({ field }) => entry?.[field] != null)
    .map(variant => ({ ...variant, runtime: entry[variant.field] }));
}

/** Product support is a ceiling; each release declares the variants it contains. */
export function validateRuntimeVariants(game, entry) {
  const variants = availableRuntimeVariants(entry);
  if (!variants.length) throw new Error(game + ': at least one Runtime variant is required');
  if (!PRODUCT_GAMES[game]?.multiplayerRuntime && entry.multiplayerRuntime != null)
    throw new Error(game + ': unexpected multiplayer Runtime');
  return variants;
}

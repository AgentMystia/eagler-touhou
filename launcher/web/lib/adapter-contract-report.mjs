import { PRODUCT_GAMES, isGameId } from './contracts/product-catalog.mjs';
import {
  RUNTIME_PROTOCOL_COMMAND_BEHAVIOR,
  RUNTIME_PROTOCOL_EVENT_BEHAVIOR,
  RUNTIME_CONFIGURE_OPTION_BEHAVIOR,
  RUNTIME_CONFIGURE_LEGACY_OPTION_KEYS,
  RUNTIME_CONFIGURE_LEGACY_MUSIC_MODES,
  RUNTIME_PROTOCOL_LEGACY_EVENTS,
} from './contracts/runtime-protocol.mjs';
import { PRODUCT_CONTENT } from './content-definition.mjs';
import { runtimeAdapterProfile } from './runtime-data-provider.mjs';

export const ADAPTER_CONTRACT_REPORT_SCHEMA = 'eagler-touhou/adapter-contract-report/2';

/** 展示当前登记信息；构建、测试和实机验收状态由项目进度表记录。 */
export function createAdapterContractReport(game) {
  if (!isGameId(game)) throw new Error('unknown game: ' + game);
  const product = PRODUCT_GAMES[game];
  return {
    schema: ADAPTER_CONTRACT_REPORT_SCHEMA,
    game,
    product: structuredClone(product),
    adapter: runtimeAdapterProfile(game),
    content: structuredClone(PRODUCT_CONTENT[game]),
    protocol: {
      commands: structuredClone(RUNTIME_PROTOCOL_COMMAND_BEHAVIOR),
      events: structuredClone(RUNTIME_PROTOCOL_EVENT_BEHAVIOR),
      configure: structuredClone(RUNTIME_CONFIGURE_OPTION_BEHAVIOR),
      legacyConfigureAliases: [...RUNTIME_CONFIGURE_LEGACY_OPTION_KEYS],
      legacyMusicModes: [...RUNTIME_CONFIGURE_LEGACY_MUSIC_MODES],
      legacyEvents: [...RUNTIME_PROTOCOL_LEGACY_EVENTS],
    },
  };
}

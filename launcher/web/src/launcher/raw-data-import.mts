import { PACKAGE_DESCRIPTOR_SCHEMA } from '../../package/package-descriptor.mjs';
import { HOST_PROTOCOL, PRODUCT_GAMES } from '@eagler/contracts/product-catalog';
import type { GameId } from '@eagler/contracts/product-catalog';
import type { SiteGameData } from '@eagler/contracts/site-manifest';
import type { PackageDescriptor } from './package-read-models.mjs';

export function rawDataImportFileNames(game: GameId): readonly string[] {
  const packagePolicy = PRODUCT_GAMES[game].package;
  return 'rawDataImport' in packagePolicy && packagePolicy.rawDataImport
    ? packagePolicy.rawDataImport.fileNames
    : [];
}

export function rawDataImportMatchesFileName(game: GameId, fileName: string): boolean {
  const normalized = String(fileName || '').toLowerCase();
  return (
    !!normalized && rawDataImportFileNames(game).some((name) => name.toLowerCase() === normalized)
  );
}

export function rawDataImportSizeMatches(expected: SiteGameData, actualBytes: number): boolean {
  return Number.isInteger(actualBytes) && actualBytes === expected.bytes;
}

export function rawDataImportHashMatches(expected: SiteGameData, actualSha256: string): boolean {
  return (
    /^[a-f0-9]{64}$/i.test(actualSha256) &&
    actualSha256.toLowerCase() === expected.sha256.toLowerCase()
  );
}

export function createRawDataImportPackageDescriptor(
  game: GameId,
  expected: SiteGameData,
  actualSha256: string,
): PackageDescriptor {
  if (rawDataImportFileNames(game).length === 0)
    throw new Error(`${game}: raw DATA import is not declared`);
  if (!rawDataImportHashMatches(expected, actualSha256))
    throw new Error(`${game}: raw DATA identity does not match Site Manifest`);
  const product = PRODUCT_GAMES[game];
  const dataFileId = product.package.dataFileId;
  return {
    schema: PACKAGE_DESCRIPTOR_SCHEMA,
    game,
    revision: `raw-${actualSha256.toLowerCase().slice(0, 16)}`,
    runtimeRequirement: {
      protocol: HOST_PROTOCOL,
      target: game,
      dataFile: dataFileId,
      dataLayout: expected.layout,
    },
    files: {
      [dataFileId]: {
        revision: expected.version,
        source: expected.path,
        target: product.package.dataTarget,
        bytes: expected.bytes,
      },
    },
    base: { files: [dataFileId] },
    components: {},
  };
}

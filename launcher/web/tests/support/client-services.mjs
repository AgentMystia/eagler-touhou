import { pathToFileURL } from 'node:url';
import { ensureClientServicesBuild } from '../../lib/client-services-build.mjs';

const build = await ensureClientServicesBuild();
export const resolveClientService = path => build.resolve('assets/' + path);
export const importClientService = path => import(pathToFileURL(resolveClientService(path)).href);

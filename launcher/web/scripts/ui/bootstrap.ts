import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import type { Plugin } from 'vite';

const moduleId = 'virtual:launcher-bootstrap';
const resolvedId = '\0' + moduleId;
const directory = new URL('../../app/bootstrap/', import.meta.url);
const files = [
  'library-boot.js',
  'compatibility-gate.js',
  'library-preload.css',
  'library-preload.html',
  'lobby-boot.js',
  'lobby-preload.css',
  'lobby-preload.html',
] as const;

export function readBootstrapAssets() {
  const read = (name: (typeof files)[number]) => readFileSync(new URL(name, directory), 'utf8');
  return {
    library: {
      script: read('library-boot.js'),
      compatibility: read('compatibility-gate.js'),
      css: read('library-preload.css'),
      preload: read('library-preload.html'),
    },
    lobby: {
      script: read('lobby-boot.js'),
      compatibility: '',
      css: read('lobby-preload.css'),
      preload: read('lobby-preload.html'),
    },
  };
}

export function bootstrapPlugin(): Plugin {
  return {
    name: 'launcher-bootstrap',
    resolveId(id) {
      if (id === moduleId) return resolvedId;
    },
    load(id) {
      if (id !== resolvedId) return;
      for (const name of files) this.addWatchFile(fileURLToPath(new URL(name, directory)));
      return 'export const bootstrapAssets = ' + JSON.stringify(readBootstrapAssets()) + ';';
    },
  };
}

import { existsSync } from "node:fs";
import { fileURLToPath, pathToFileURL } from "node:url";
import { resolve } from "node:path";
import {contractsSourceDirectory} from '../source-layout.mjs';

const project = resolve(fileURLToPath(new URL("../..", import.meta.url)));

export async function loadCompiledContract(name) {
  if (!/^[a-z][a-z0-9-]*(?:\/[a-z][a-z0-9-]*)*$/.test(name)) throw new Error(`invalid compiled contract name: ${name}`);
  const published = resolve(project, "assets", "contracts", `${name}.mjs`);
  const source = contractsSourceDirectory(project);
  if (source) return import(pathToFileURL(resolve(source, `${name}.mts`)).href);
  if (existsSync(published)) return import(pathToFileURL(published).href);
  throw new Error(`Missing published contract: ${name}`);
}

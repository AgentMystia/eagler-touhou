import {compatibilitySourcePath} from '../compat/path-aliases.mjs';
import { existsSync } from "node:fs";
import { resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";

const project = resolve(fileURLToPath(new URL("..", import.meta.url)));

export const PUBLIC_SOURCE_ROOT = resolve(project, "public");

function normalizePublicPath(value) {
  const path = String(value || "").replaceAll("\\", "/").replace(/^\.\//, "");
  if (!path || path.startsWith("/") || path.split("/").some(part => !part || part === "." || part === "..")) {
    throw new Error(`invalid public source path: ${value}`);
  }
  return path;
}

export function resolvePublicSource(value, {project: sourceProject = project} = {}) {
  const publicRoot = resolve(sourceProject, "public");
  const path = normalizePublicPath(value);
  const source = resolve(publicRoot, path);
  if (source === publicRoot || !source.startsWith(publicRoot + sep)) {
    throw new Error(`public source path escapes its root: ${value}`);
  }
  return source;
}

export function resolvePublicOrRepositorySource(value, {project: sourceProject = project} = {}) {
  const path = normalizePublicPath(value);
  if (path === "favicon.ico" || path === "assets/favicon.ico") return resolve(sourceProject, "favicon.ico");
  if (path === "LICENSE") {
    const shared = resolve(sourceProject, "../..", path);
    if (existsSync(shared)) return shared;
  }
  const alias = compatibilitySourcePath(path);
  if (alias) return resolve(sourceProject, alias);
  const publicSource = resolvePublicSource(path, {project: sourceProject});
  return existsSync(publicSource) ? publicSource : resolve(sourceProject, path);
}

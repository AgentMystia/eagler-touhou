import { readFileSync } from 'node:fs';

/** Observe the loaded document: Runtime navigation may leave the src attribute unchanged. */
export function runtimeDocumentObservationScript() {
  return readFileSync(new URL('./runtime-document-observation.js', import.meta.url), 'utf8');
}
export async function installRuntimeDocumentObservation(page) {
  await page.evaluateOnNewDocument(runtimeDocumentObservationScript());
}

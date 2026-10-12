import { defineConfig } from 'vite';
import { reactRouter } from '@react-router/dev/vite';
import { bootstrapPlugin } from './scripts/ui/bootstrap.ts';
import { mainAssets } from './scripts/ui/main-assets.ts';
import { authoredSources } from './scripts/ui/authored-sources.ts';
import { reactDeploymentConfig } from './scripts/ui/deployment-config.ts';
const deployment = reactDeploymentConfig();
export default defineConfig({
  base: deployment.mountPath,
  // The build's preview fetch and listener share one explicit loopback family.
  preview: { host: '127.0.0.1' },
  // Keep generated JavaScript within the browser floor admitted by bootstrap.
  build: { manifest: true, target: 'chrome108' },
  define: { __REACT_APP_SHELL_DEPLOYMENT__: JSON.stringify(deployment.appShell) },
  plugins: [bootstrapPlugin(), mainAssets(), authoredSources(), reactRouter()],
  publicDir: false,
});

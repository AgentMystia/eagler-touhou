import {resolve} from 'node:path';
import {build} from 'esbuild';
import {authoredSourcesPlugin} from './authored-source-resolution.mjs';

/** The standalone pages bundle the same React masthead as the live launcher. */
export async function buildStandalonePageOutputs({project,outputDirectory}) {
  const result = await build({entryPoints:[resolve(project,'app/standalone-pages.tsx')],outfile:resolve(outputDirectory,'site-pages.mjs'),
    bundle:true,format:'esm',platform:'browser',target:'chrome108',jsx:'automatic',minify:true,write:false,
    define:{'process.env.NODE_ENV':'"production"'},external:['*.woff2','*.webp','*.svg','*.png'],
    plugins:[authoredSourcesPlugin(project)],logLevel:'warning'});
  return result.outputFiles;
}

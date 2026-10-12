#!/usr/bin/env node
import {ensureClientServicesBuild} from '../lib/client-services-build.mjs';
import {spawn} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {dependencyExecutable, WEB_ROOT} from '../lib/source-layout.mjs';

const result = await ensureClientServicesBuild({force:process.argv.includes('--force')});
console.log(`Client services: ${result.sourceCount} TypeScript modules`);
await new Promise((yes,no)=>{
  const child=spawn(process.execPath,[dependencyExecutable('@react-router/dev','react-router'),'build'],{cwd:WEB_ROOT,stdio:'inherit'});
  child.once('error',no);child.once('exit',code=>code===0?yes():no(Error(`React build failed (${code})`)));
});

import {execFileSync} from 'node:child_process';
import {existsSync,mkdirSync} from 'node:fs';
import {resolve} from 'node:path';
import assert from 'node:assert/strict';
const root=resolve(import.meta.dirname,'../..'),sdk=process.env.EMSDK;
assert(sdk,'Set EMSDK to the pinned Emscripten SDK');
const emcc=['install','upstream'].map(layout=>resolve(sdk,layout,'emscripten/emcc.py')).find(existsSync);
assert(emcc,'Emscripten compiler not found');
const out=resolve(root,'artifacts/issue-153');mkdirSync(out,{recursive:true});
const output=resolve(out,'extrapolation-check.cjs');
execFileSync('python',[emcc,resolve(import.meta.dirname,'extrapolation-check.cpp'),
 '-I'+resolve(root,'common/include'),'-std=c++17','-O1','-sDEFAULT_TO_CXX=1','-sENVIRONMENT=node','-o',output],
 {cwd:root,env:process.env,stdio:'inherit',windowsHide:true});
execFileSync(process.execPath,[output],{stdio:'inherit',windowsHide:true});

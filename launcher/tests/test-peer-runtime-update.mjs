import assert from 'node:assert/strict';
import {updatePeerRuntime} from '../scripts/update-peer-runtime.mjs';
const fixture=`const unchanged=42; export default async function runtime(Module){
function eagler_peer_connect(a,b,c,d,e){return 'old';}
function eagler_peer_send(a,b,c){return 1;}
function eagler_peer_send_to(a,b,c){return 1;}
function eagler_peer_send_repair_to(a,b,c){return 1;}
return Module;}`;
const result=updatePeerRuntime(fixture);
assert.match(result.output,/createAdaptivePeerRoute/);
assert.match(result.output,/const unchanged=42/);
assert.match(result.output,/return Module;/);
assert.equal(result.receipt.bindings.length,4);
assert.equal(updatePeerRuntime(result.output).output,result.output);
assert.throws(()=>updatePeerRuntime(fixture.replace('a,b,c,d,e','a,b,c,d')),/ABI mismatch/);
assert.throws(()=>updatePeerRuntime(fixture.replace('eagler_peer_send_to','absent')),/binding missing/);
assert.throws(()=>updatePeerRuntime(fixture+'function eagler_peer_send_to(a,b,c){}'),/Ambiguous binding/);
console.log('Shared browser bridge update | native signatures | unrelated content | repeatability');

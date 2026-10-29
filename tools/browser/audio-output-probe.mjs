// Test-side observation of actual SDL Web Audio output. Installed before the
// Runtime starts; no player build, native export or game state is modified.
export function installPcmProbe(){
  const probe={frames:0,peak:0,reset(){this.frames=0;this.peak=0;}};
  globalThis.__sdlAudioProbe=probe;
  const prototype=globalThis.AudioContext?.prototype;
  if(!prototype?.createScriptProcessor)return;
  const create=prototype.createScriptProcessor;
  prototype.createScriptProcessor=function(...args){
    const node=create.apply(this,args);
    queueMicrotask(()=>{
      const process=node.onaudioprocess;
      if(typeof process!=='function')return;
      node.onaudioprocess=function(event){
        process.call(this,event);
        const buffer=event.outputBuffer;probe.frames+=buffer.length;
        for(let channel=0;channel<buffer.numberOfChannels;channel++){
          for(const sample of buffer.getChannelData(channel))probe.peak=Math.max(probe.peak,Math.abs(sample));
        }
      };
    });
    return node;
  };
}

export async function samplePcm(frame){
  // Retire the old device queue before opening the observation window.
  await new Promise(resolve=>setTimeout(resolve,250));
  await frame.evaluate(()=>globalThis.__sdlAudioProbe.reset());
  await new Promise(resolve=>setTimeout(resolve,300));
  return frame.evaluate(()=>({frames:__sdlAudioProbe.frames,peak:__sdlAudioProbe.peak}));
}

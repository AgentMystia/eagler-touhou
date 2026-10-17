/** Explicit dependency integration gate: real locked wheels and interpreter. */
import test from 'node:test';
import assert from 'node:assert/strict';
import { appendFile, mkdtemp, readFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { ensurePythonEnvironment } from '../host/lib/python-environment.mjs';
import { run } from '../host/lib/process.mjs';

const projectRoot = fileURLToPath(new URL('../', import.meta.url));
test('locked Python tools execute, reuse a complete environment and replace altered dependencies', async () => {
  const hostRoot = await mkdtemp(resolve(tmpdir(), 'eagler-python-environment-'));
  try {
    const options = {projectRoot, hostRoot,
      python: process.env.PYTHON ?? (process.platform === 'win32' ? 'python' : 'python3'),
      wheelhouse: process.env.EAGLER_PYTHON_WHEELHOUSE};
    const [first, concurrent] = await Promise.all([ensurePythonEnvironment(options), ensurePythonEnvironment(options)]);
    assert.equal(concurrent, first);
    assert.equal(await ensurePythonEnvironment(options), first, 'a complete locked environment is reused');
    const smoke = String.raw`import io, json, numpy as np, soundfile as sf
from PIL import Image
from fontTools.ttLib import TTFont
image = Image.new("RGB", (3, 2), (80, 100, 120))
png = io.BytesIO(); image.save(png, format="PNG")
assert Image.open(io.BytesIO(png.getvalue())).size == (3, 2)
signal = np.zeros((4800, 2), dtype=np.float32)
audio = io.BytesIO(); sf.write(audio, signal, 48000, format="OGG", subtype="VORBIS")
decoded, rate = sf.read(io.BytesIO(audio.getvalue()))
assert decoded.shape == signal.shape and rate == 48000
print(json.dumps({"image": "PNG", "audio": "OGG/VORBIS", "font": TTFont.__module__}))`;
    const output = JSON.parse((await run(first, ['-I', '-c', smoke], {capture: true})).stdout);
    assert.equal(output.image, 'PNG'); assert.equal(output.audio, 'OGG/VORBIS');
    const module = (await run(first, ['-I', '-c', 'import PIL.Image; print(PIL.Image.__file__)'], {capture: true})).stdout;
    await appendFile(module, '\n# private integration fixture: altered installed file\n');
    const original = await readFile(module);
    const replacement = await ensurePythonEnvironment(options);
    assert.notEqual(replacement, first, 'version metadata alone must not bless modified dependency files');
    assert.deepEqual(await readFile(module), original, 'in-flight callers keep their own environment');
    await run(replacement, ['-I', '-c', smoke], {capture: true});
    assert.equal(await ensurePythonEnvironment(options), replacement);
  } finally { await rm(hostRoot, {recursive: true, force: true, maxRetries: 10, retryDelay: 100}); }
});

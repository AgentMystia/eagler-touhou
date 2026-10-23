// 从 raw 解包目录（artifacts/reference/raw）拷贝网页运行时所需资源到 ./assets。
// 产物为原版权材料，git 不入库（.gitignore 已忽略 assets/）。
// 用法（容器内，仓库根）：node eagler-touhou/ports/th13/web/sdl-runtime/prepare-assets.mjs
import {copyFileSync, mkdirSync, existsSync} from 'node:fs';
import {resolve, dirname} from 'node:path';
import {fileURLToPath} from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const workspace = resolve(here, '../../../../..');  // 工作区根（容器内 /work）
const raw = resolve(workspace, 'artifacts/reference/raw');
const out = resolve(here, 'assets');
if (!existsSync(raw)) throw Error('raw 解包目录不存在：' + raw + '（先做资源提取）');
mkdirSync(out, {recursive: true});

const files = [
  'text.anm', 'sig.anm', 'ascii.anm', 'effect.anm', 'title.anm', 'title_v.anm', 'help.anm',
  'front.anm', 'enemy.anm', 'st01enm.anm', 'bullet.anm',
  'pl00.anm', 'pl01.anm', 'pl02.anm', 'pl03.anm',
  'st01.ecl', 'default.ecl', 'st01bs.ecl', 'st01.std', 'st01wl.anm',
  'st01a.msg', 'st01b.msg', 'st01c.msg', 'st01d.msg', 'st01logo.anm',
  'st02.ecl', 'st02bs.ecl', 'st02mbs.ecl', 'st02.std', 'st02wl.anm', 'st02enm.anm', 'st02logo.anm',
  'st02a.msg', 'st02b.msg', 'st02c.msg', 'st02d.msg',
  'staff.msg',
  'pl00a.sht', 'pl01a.sht', 'pl02a.sht', 'pl03a.sht',
];
for (const name of files) {
  copyFileSync(resolve(raw, name), resolve(out, name));
  console.log('assets/' + name);
}
console.log('完成：' + out);

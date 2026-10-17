// Opt-in display experiment. No canvas buffer or game renderer state is changed.
const panel = document.createElement('aside');
panel.id = 'pixel-scaling-experiment';
panel.style.cssText = 'position:fixed;left:8px;top:8px;z-index:2147483647;background:#181818ee;color:white;padding:10px;border:1px solid #888;border-radius:8px;font:13px system-ui;max-width:340px';
panel.innerHTML = `<strong>画面缩放实验</strong> <button data-close>关闭并恢复</button>
<div style="display:flex;gap:6px;margin:8px 0"><button data-mode="original">原始</button><button data-mode="pixel">清晰像素</button><button data-mode="integer">整数倍</button></div>
<output style="display:block;white-space:pre-line">等待游戏画布</output>`;
document.body.append(panel);
let mode = 'original';
const records = new Map();
const properties = ['image-rendering', 'width', 'height', 'min-width', 'min-height', 'max-width', 'max-height', 'position', 'left', 'top', 'margin', 'transform'];
function restore(canvas, record) {
  for (const [key, value, priority] of record.original) {
    if (value) canvas.style.setProperty(key, value, priority);
    else canvas.style.removeProperty(key);
  }
}
function collect(win, found) {
  try {
    // Launcher helper canvases are intentionally excluded.
    if (win !== window || !document.querySelector('iframe')) {
      for (const canvas of win.document.querySelectorAll('canvas')) {
        if (canvas.width >= 320 && canvas.height >= 240 && canvas.getBoundingClientRect().width > 0) found.push(canvas);
      }
    }
    for (const frame of win.document.querySelectorAll('iframe')) {
      if (frame.contentWindow) collect(frame.contentWindow, found);
    }
  } catch { /* Cross-origin frames cannot participate in this local experiment. */ }
}
function refresh() {
  // The Launcher enters fullscreen on its player section. Controls outside
  // that element would disappear even with a high z-index.
  const fullscreen = document.fullscreenElement;
  const container = fullscreen?.tagName === 'IFRAME'
    ? fullscreen.contentDocument?.body || document.body
    : fullscreen || document.body;
  if (panel.parentNode !== container) container.append(panel);
  const found = [];
  collect(window, found);
  const lines = [];
  for (const [canvas, record] of records) {
    if (!found.includes(canvas)) { restore(canvas, record); records.delete(canvas); }
  }
  for (const canvas of found) {
    let record = records.get(canvas);
    if (!record) {
      record = { original: properties.map(key => [key, canvas.style.getPropertyValue(key), canvas.style.getPropertyPriority(key)]) };
      records.set(canvas, record);
    }
    restore(canvas, record);
    const win = canvas.ownerDocument.defaultView;
    const rect = canvas.getBoundingClientRect();
    const dpr = win.devicePixelRatio || 1;
    let note = '';
    if (mode !== 'original') canvas.style.setProperty('image-rendering', 'pixelated', 'important');
    if (mode === 'integer') {
      // Fit inside the game's current allocation, accounting for outer transforms.
      const scale = Math.floor(Math.min(rect.width * dpr / canvas.width, rect.height * dpr / canvas.height) + 1e-6);
      if (scale >= 1) {
        const layout = win.getComputedStyle(canvas);
        const extra = keys => layout.boxSizing === 'border-box' ? 0 : keys.reduce((sum, key) => sum + (parseFloat(layout[key]) || 0), 0);
        // offsetWidth/Height round fractional CSS pixels and can turn an exact
        // 2x target into 2.001x. Keep the fractional layout dimensions here.
        const sx = rect.width / (parseFloat(layout.width) + extra(['paddingLeft', 'paddingRight', 'borderLeftWidth', 'borderRightWidth']));
        const sy = rect.height / (parseFloat(layout.height) + extra(['paddingTop', 'paddingBottom', 'borderTopWidth', 'borderBottomWidth']));
        const width = canvas.width * scale / dpr / sx;
        const height = canvas.height * scale / dpr / sy;
        for (const [key, value] of Object.entries({ width: `${width}px`, height: `${height}px`, 'min-width': '0', 'min-height': '0', 'max-width': 'none', 'max-height': 'none' })) canvas.style.setProperty(key, value, 'important');
        // Preserve the original visual center, then align to physical pixels.
        const after = canvas.getBoundingClientRect();
        const x = (Math.round((rect.left + (rect.width - after.width) / 2) * dpr) / dpr - after.left) / sx;
        const y = (Math.round((rect.top + (rect.height - after.height) / 2) * dpr) / dpr - after.top) / sy;
        const computed = win.getComputedStyle(canvas);
        canvas.style.setProperty('transform', `translate(${x}px,${y}px) ${computed.transform === 'none' ? '' : computed.transform}`, 'important');
      } else note = '\n空间不足一个原始像素倍数，回退清晰缩放';
    }
    const actual = canvas.getBoundingClientRect();
    lines.push(`画布 ${canvas.width} × ${canvas.height}\n显示 ${actual.width.toFixed(1)} × ${actual.height.toFixed(1)} CSS px · DPR ${dpr}\n物理像素倍率 ${(actual.width * dpr / canvas.width).toFixed(3)} × ${(actual.height * dpr / canvas.height).toFixed(3)}${note}`);
  }
  panel.querySelector('output').textContent = lines.join('\n\n') || '等待游戏画布（仅支持同源页面）';
  for (const button of panel.querySelectorAll('[data-mode]')) button.setAttribute('aria-pressed', String(button.dataset.mode === mode));
}
panel.addEventListener('click', event => {
  const button = event.target.closest('button');
  if (!button) return;
  if (button.hasAttribute('data-close')) {
    clearInterval(timer);
    for (const [canvas, record] of records) restore(canvas, record);
    panel.remove();
    return;
  }
  mode = button.dataset.mode;
  refresh();
  // Return keyboard ownership to the game after changing the comparison mode.
  records.keys().next().value?.focus({ preventScroll: true });
});
const timer = setInterval(refresh, 500);
refresh();

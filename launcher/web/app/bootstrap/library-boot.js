(function () {
  var startedAt = Date.now();
  var watchdog = null;
  var boot = {
    version: '20260828-4',
    stage: 'html',
    kind: '',
    detail: '',
    resource: '',
    done: false
  };
  function compactResource(value) {
    if (!value) return '';
    try {
      var url = new URL(String(value), location.href);
      return (url.origin === location.origin ? '' : url.origin) + url.pathname;
    } catch (_) {
      return String(value).slice(0, 240);
    }
  }
  function dismissPreload() {
    var preload = document.getElementById('eaglerPreload');
    if (!preload || preload.classList.contains('is-done')) return;
    preload.classList.add('is-done');
    setTimeout(function () {
      if (preload.isConnected) preload.remove();
    }, 240);
  }
  function waitForCriticalVisuals() {
    var sources = ['assets/launcher-background.webp'];
    var cards = document.querySelectorAll('.game-shelf .game:not([hidden]) .card-art-image');
    for (var i = 0; i < cards.length; i++) {
      var card = cards[i].closest('.game');
      var rect = card && card.getBoundingClientRect();
      if (
        !rect ||
        rect.right < 0 ||
        rect.left > innerWidth + 80 ||
        rect.bottom < 0 ||
        rect.top > innerHeight + 80
      )
        continue;
      var source = cards[i].currentSrc || cards[i].src;
      if (source && sources.indexOf(source) < 0) sources.push(source);
    }
    var images = sources.map(function (source) {
      var image = new Image();
      image.src = source;
      if (image.decode) return image.decode().catch(function () {});
      return new Promise(function (resolve) {
        image.onload = resolve;
        image.onerror = resolve;
      });
    });
    var fonts = document.fonts
      ? Promise.all([
          document.fonts.load('400 16px "touhou98"', 'EAGLER TOUHOU'),
          document.fonts.load('400 16px "ET Yatra"', 'EAGLER TOUHOU'),
          document.fonts.load('700 24px "ET Chill Round"', '東方紅魔郷'),
          document.fonts.ready
        ]).catch(function () {})
      : Promise.resolve();
    return Promise.race([
      Promise.all(images.concat(fonts)),
      new Promise(function (resolve) {
        setTimeout(resolve, 3000);
      })
    ]);
  }
  function diagnosis() {
    if (boot.kind === 'script-load') return '有一项启动文件未能载入。请检查网络后重新加载。';
    if (boot.kind === 'manifest') return '服务器信息暂时不可用。已安装的游戏和存档不会受到影响。';
    if (boot.kind === 'javascript')
      return '启动器初始化时遇到错误，可能是缓存版本不一致或浏览器兼容问题。';
    if (boot.stage === 'catalog-request' || boot.stage === 'manifest-request')
      return '启动器初始化超时。已安装的游戏和存档不会受到影响。';
    if (boot.stage === 'app-module-request') return '启动器主程序未能开始运行。请重新加载页面。';
    return '启动器没有在预期时间内准备完成。请重新加载页面。';
  }
  function diagnosticText() {
    var online = typeof navigator.onLine === 'boolean' ? String(navigator.onLine) : 'unknown';
    return [
      'EAGLER-BOOT/1',
      'stage=' + boot.stage,
      'kind=' + (boot.kind || 'watchdog'),
      'elapsed_ms=' + (Date.now() - startedAt),
      'online=' + online,
      'resource=' + (boot.resource || '-'),
      'detail=' + (boot.detail || '-'),
      'ua=' + String(navigator.userAgent || '-').slice(0, 320)
    ].join('\n');
  }
  function show() {
    if (boot.done) return;
    var old = document.getElementById('eaglerBootEmergency');
    if (old) {
      var oldText = document.getElementById('eaglerBootEmergencyText');
      var oldCode = document.getElementById('eaglerBootEmergencyCode');
      if (oldText) oldText.textContent = diagnosis();
      if (oldCode) oldCode.textContent = diagnosticText();
      return;
    }
    var panel = document.createElement('section');
    panel.id = 'eaglerBootEmergency';
    panel.setAttribute('role', 'alert');
    panel.style.cssText =
      'position:fixed;z-index:2147483647;inset:0;box-sizing:border-box;display:grid;place-items:center;padding:20px;background:transparent;font:13px/1.6 system-ui,-apple-system,sans-serif;text-align:left;white-space:normal';
    var card = document.createElement('div');
    card.style.cssText =
      'box-sizing:border-box;width:min(460px,100%);max-height:min(82vh,620px);overflow:auto;padding:24px;border:1px solid rgba(255,255,255,.1);border-radius:22px;background:#191a17;color:#d8d5ce;box-shadow:0 24px 70px rgba(0,0,0,.62),inset 0 1px rgba(255,255,255,.035)';
    var heading = document.createElement('div');
    heading.style.cssText = 'display:flex;align-items:center;gap:11px;margin-bottom:10px';
    var status = document.createElement('i');
    status.setAttribute('aria-hidden', 'true');
    status.style.cssText =
      'display:grid;flex:0 0 28px;width:28px;height:28px;place-items:center;border-radius:50%;background:rgba(239,106,88,.13);color:#ef6a58;font-style:normal;font-weight:800';
    status.textContent = '!';
    var title = document.createElement('strong');
    title.textContent = '启动器未能载入';
    title.style.cssText = 'display:block;color:#f2eee8;font-size:17px;line-height:1.3';
    heading.appendChild(status);
    heading.appendChild(title);
    var text = document.createElement('div');
    text.id = 'eaglerBootEmergencyText';
    text.textContent = diagnosis();
    text.style.cssText = 'color:#bdb8b0';
    var hint = document.createElement('div');
    hint.textContent = '如果重新加载后仍然失败，请复制诊断信息并反馈。';
    hint.style.cssText = 'margin-top:5px;color:#85827c;font-size:12px';
    var details = document.createElement('details');
    details.style.cssText = 'margin-top:15px';
    var summary = document.createElement('summary');
    summary.textContent = '查看诊断信息';
    summary.style.cssText = 'color:#a6a29b;cursor:pointer;font-size:12px;user-select:none';
    var code = document.createElement('pre');
    code.id = 'eaglerBootEmergencyCode';
    code.textContent = diagnosticText();
    code.style.cssText =
      'margin:10px 0 0;padding:11px;overflow:auto;border-radius:10px;background:#10110f;color:#aaa69f;font:10px/1.5 ui-monospace,SFMono-Regular,Consolas,monospace;white-space:pre-wrap;word-break:break-word';
    details.appendChild(summary);
    details.appendChild(code);
    var actions = document.createElement('div');
    actions.style.cssText = 'display:flex;gap:9px;flex-wrap:wrap;margin-top:18px';
    function button(label, action, primary) {
      var item = document.createElement('button');
      item.type = 'button';
      item.textContent = label;
      item.style.cssText =
        'min-height:38px;padding:7px 14px;border:0;border-radius:11px;background:' +
        (primary ? '#ef6a58' : '#292b27') +
        ';color:' +
        (primary ? '#17100e' : '#e8e4dd') +
        ';font:700 12px/1 system-ui,-apple-system,sans-serif;cursor:pointer';
      item.addEventListener('click', action);
      return item;
    }
    actions.appendChild(
      button(
        '重新加载',
        function () {
          location.reload();
        },
        true
      )
    );
    actions.appendChild(
      button(
        '复制诊断信息',
        function (event) {
          var value = diagnosticText();
          var target = event.currentTarget;
          if (navigator.clipboard && navigator.clipboard.writeText) {
            navigator.clipboard.writeText(value).then(
              function () {
                target.textContent = '已复制';
              },
              function () {}
            );
            return;
          }
          var area = document.createElement('textarea');
          area.value = value;
          area.style.position = 'fixed';
          area.style.opacity = '0';
          document.body.appendChild(area);
          area.select();
          try {
            document.execCommand('copy');
            target.textContent = '已复制';
          } catch (_) {}
          area.remove();
        },
        false
      )
    );
    card.appendChild(heading);
    card.appendChild(text);
    card.appendChild(hint);
    card.appendChild(details);
    card.appendChild(actions);
    panel.appendChild(card);
    (document.body || document.documentElement).appendChild(panel);
  }
  boot.mark = function (stage) {
    if (boot.done) return;
    boot.stage = String(stage || boot.stage);
  };
  boot.fail = function (kind, detail, resource) {
    if (boot.done) return;
    boot.kind = String(kind || 'javascript');
    boot.detail = String(detail || '').slice(0, 600);
    boot.resource = compactResource(resource);
    dismissPreload();
    show();
  };
  boot.ready = function () {
    boot.done = true;
    if (watchdog) clearTimeout(watchdog);
    Promise.resolve()
      .then(waitForCriticalVisuals)
      .then(function () {
        requestAnimationFrame(function () {
          requestAnimationFrame(dismissPreload);
        });
      }, dismissPreload);
    var panel = document.getElementById('eaglerBootEmergency');
    if (panel) panel.remove();
  };
  window.__eaglerBoot = boot;
  window.addEventListener(
    'error',
    function (event) {
      if (boot.done) return;
      var target = event.target;
      if (target && target !== window && target.tagName === 'SCRIPT') {
        boot.fail('script-load', 'SCRIPT load error', target.src || '');
        return;
      }
      if (target && target !== window) return;
      boot.fail('javascript', event.message || 'JavaScript error', event.filename || '');
    },
    true
  );
  window.addEventListener('unhandledrejection', function (event) {
    if (boot.done) return;
    var reason = event.reason;
    boot.fail(
      'javascript',
      (reason && (reason.stack || reason.message)) ||
        String(reason || 'Unhandled promise rejection'),
      ''
    );
  });
  watchdog = setTimeout(function () {
    if (!boot.done) boot.fail('watchdog', '12 秒内启动器没有完成初始化', '');
  }, 12000);
})();

import backgroundUrl from '../../../public/assets/launcher-background.webp?url';
import touhouFontUrl from '../../../public/assets/fonts/touhou98.woff2?url';
import yatraFontUrl from '../../../public/assets/fonts/yatra-one-latin.woff2?url';
import mediumFontUrl from '../../../public/assets/fonts/chill-round-gothic-site-medium-critical.woff2?url';
import boldFontUrl from '../../../public/assets/fonts/chill-round-gothic-site-bold-critical.woff2?url';
import { bootstrapAssets as bootstrap } from 'virtual:launcher-bootstrap';
import { useSyncExternalStore } from 'react';
import { useLocation } from 'react-router';
import { isUiLocale, type UiLocale } from '../../../src/launcher/i18n.mts';
import { translate } from '../../i18n';

function subscribeLocale(listener: () => void) {
  window.addEventListener('eagler-ui-locale-change', listener);
  return () => window.removeEventListener('eagler-ui-locale-change', listener);
}

// 兼容性探测先于模块加载；静态回退页面同时服务游戏库和大厅入口。
const dispatch = `(function(){var lobby=/\\/(?:lobby|lobby\\.html)\\/?$/.test(location.pathname);document.documentElement.setAttribute('data-bootstrap-entry',lobby?'lobby':'library');if(lobby){document.documentElement.setAttribute('data-lobby-boot','loading');}else{${bootstrap.library.compatibility}\n${bootstrap.library.script}\n}}());`;
const tail = `(function(){var module=document.querySelector('script[type="module"][data-eagler-entry]');if(module)module.id='lobbyModule';if(document.documentElement.getAttribute('data-bootstrap-entry')==='lobby'){${bootstrap.lobby.script}\n}}());`;
// One authored loading presentation for both document entries and warm routes.
// Keep the lobby's failure carrier, while deriving its motion from Launcher.
const sharedLobbyPreloadCss =
  bootstrap.library.css
    .replaceAll('#eaglerPreload', '#lobbyPreload')
    .replaceAll('eagler-preload-', 'lobby-preload-') +
  '\n.lobby-preload-card{padding:0;gap:normal}html[data-lobby-boot="failed"] .lobby-preload-card{width:min(420px,calc(100vw - 48px));height:auto;min-height:132px;padding:24px;gap:18px;animation:none}';
/** 模块加载前的兼容性检查与加载界面。 */
export function BootstrapHead() {
  const { pathname } = useLocation();
  const publishedLocale: UiLocale = /\/en\.html\/?$/.test(pathname) ? 'en' : 'zh-CN';
  const locale = useSyncExternalStore(
    subscribeLocale,
    () => {
      const value = document.documentElement.dataset.uiLocale;
      return isUiLocale(value) ? value : publishedLocale;
    },
    () => publishedLocale,
  );
  const lobby = /\/(?:lobby|lobby\.html)\/?$/.test(pathname);
  const title = lobby
    ? `${translate(locale, 'lobby.title')} ~ EAGLER TOUHOU`
    : translate(locale, 'site.documentTitle');
  const description = translate(locale, 'site.description');
  return (
    <>
      <script dangerouslySetInnerHTML={{ __html: dispatch }} />
      <style
        dangerouslySetInnerHTML={{
          __html:
            bootstrap.library.css +
            '\n' +
            bootstrap.lobby.css +
            '\n' +
            sharedLobbyPreloadCss +
            '\nhtml[data-bootstrap-entry="lobby"] #eaglerPreload,html:not([data-bootstrap-entry="lobby"]) #lobbyPreload{display:none}',
        }}
      />
      <style id="launcherNavigationTransition">{'@view-transition{navigation:auto}'}</style>
      <title data-i18n={lobby ? undefined : 'site.documentTitle'}>{title}</title>
      <meta name="theme-color" content={lobby ? '#111210' : '#10100f'} />
      {lobby ? (
        <meta name="robots" content="noindex" />
      ) : (
        <>
          <meta name="color-scheme" content="dark" />
          <meta name="description" content={description} data-i18n-content="site.description" />
          <meta property="og:type" content="website" />
          <meta property="og:site_name" content="EAGLER TOUHOU" />
          <meta property="og:title" content={title} data-i18n-content="site.documentTitle" />
          <meta
            property="og:description"
            content={description}
            data-i18n-content="site.description"
          />
          <meta name="mobile-web-app-capable" content="yes" />
          <meta name="apple-mobile-web-app-capable" content="yes" />
          <meta name="apple-mobile-web-app-title" content="EAGLER TOUHOU" />
          <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent" />
          <link rel="manifest" href="site.webmanifest" />
          <link rel="apple-touch-icon" sizes="180x180" href="assets/pwa/apple-touch-icon.png" />
        </>
      )}
      <link
        rel="icon"
        href={lobby ? 'favicon.ico' : 'assets/favicon.ico'}
        type={lobby ? undefined : 'image/x-icon'}
      />
      <link rel="preload" href={backgroundUrl} as="image" fetchPriority="high" />
      <link
        rel="preload"
        href={touhouFontUrl}
        as="font"
        type="font/woff2"
        crossOrigin="anonymous"
      />
      <link rel="preload" href={yatraFontUrl} as="font" type="font/woff2" crossOrigin="anonymous" />
      <link
        rel="preload"
        href={mediumFontUrl}
        as="font"
        type="font/woff2"
        crossOrigin="anonymous"
      />
      <link rel="preload" href={boldFontUrl} as="font" type="font/woff2" crossOrigin="anonymous" />
    </>
  );
}
export function BootstrapPreloads() {
  // 冷启动和站内导航共用加载界面，站内导航不重启冷启动看门狗。
  return (
    <div
      style={{ display: 'contents' }}
      data-surface-preload-host=""
      dangerouslySetInnerHTML={{
        __html:
          bootstrap.library.preload +
          bootstrap.lobby.preload +
          `<template data-surface-preload="library">${bootstrap.library.preload}</template>` +
          `<template data-surface-preload="lobby">${bootstrap.lobby.preload}</template>`,
      }}
    />
  );
}
export function ModuleRequestMark() {
  return (
    <script
      dangerouslySetInnerHTML={{
        __html: 'window.__eaglerBoot && window.__eaglerBoot.mark("app-module-request");',
      }}
    />
  );
}
export function BootstrapTail() {
  return <script dangerouslySetInnerHTML={{ __html: tail }} />;
}

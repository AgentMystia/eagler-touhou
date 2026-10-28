import {useEffect, useRef, useState, useSyncExternalStore} from 'react';
import {createRoot} from 'react-dom/client';
import {LauncherMasthead} from './components/launcher/LauncherMasthead';
import {MainSelect} from './components/launcher/MainSelect';
import {DonationDialog} from './components/notices/DonationDialog';
import {FirstUseNotice, type FirstUseNoticeHandle} from './components/notices/FirstUseNotice';
import {LocaleProvider, useLocale} from './i18n';
import {UI_LOCALE_STORAGE_KEY, type UiLocale} from '../src/launcher/i18n.mts';
import {createSitePreferencesModel, bindSitePreferencesToDocument} from './models/site-preferences';
import {RUNTIME_DIAGNOSTICS_STORAGE_KEY as diagnosticsKey} from './models/runtime-diagnostics';
import {validateSiteManifest, siteOriginMigrationAvailable} from '../src/contracts/site-manifest.mts';
import {UptimePage} from './components/uptime/UptimePage';
import './site-navigation.css';
import './components/uptime/uptime.css';

const base = new URL('.', document.baseURI), url = (path:string) => new URL(path, base).href;
let storage:Storage|null = null; try {storage = window.localStorage;} catch {}
const preferences = createSitePreferencesModel({storage});
bindSitePreferencesToDocument(preferences);
const read = (key:string) => {try {return storage?.getItem(key);} catch {return null;}};
const write = (key:string, value:string) => {try {storage?.setItem(key,value);} catch {}};
function StandaloneHeader() {
  const {locale, t} = useLocale(), prefs = useSyncExternalStore(preferences.subscribe, preferences.getSnapshot);
  const [open,setOpen] = useState(false), [donation,setDonation] = useState(false), [notice,setNotice] = useState(false);
  const [donationAvailable,setDonationAvailable] = useState(true), [diagnostics,setDiagnostics] = useState(read(diagnosticsKey)==='1');
  const [migration,setMigration] = useState({known:false,available:false});
  const firstUse = useRef<FirstUseNoticeHandle>(null);
  useEffect(() => {
    document.documentElement.lang = locale;
  }, [locale]);
  useEffect(() => {
    const controller = new AbortController();
    void fetch(url('site-manifest.json'), {signal:controller.signal}).then(async r => {
      if (!r.ok) return; const site = validateSiteManifest(await r.json());
      setMigration({known:true,available:siteOriginMigrationAvailable(site,window.location.protocol)});
    }).catch(() => {});
    return () => controller.abort();
  }, []);
  return <>
    <LauncherMasthead variant="help" nativeLinks assetUrl={url} launcherHref={url(locale==='en'?'en.html':'./')} faqHref={url('faq.html')} aboutHref={url('about.html')}
      migrationPolicyKnown={migration.known} migrationHref={migration.available?url('migrate.html'):undefined}
      menuOpen={open} onMenuOpenChange={setOpen} lessMotion={prefs.lessMotion} onToggleMotion={preferences.toggleMotion} noticeEnabled={prefs.noticeEnabled} onToggleNotice={preferences.toggleNotice}
      diagnosticsEnabled={diagnostics} onToggleDiagnostics={() => {write(diagnosticsKey,diagnostics?'0':'1');setDiagnostics(!diagnostics);}}
      onDonation={() => setDonation(true)} donationAvailable={donationAvailable} onFirstUse={() => {void firstUse.current?.showManual();}}
      languageControl={<MainSelect id="uiLanguageSelect" className="option-select ui-language-select" aria-label={t('ui.language')} value={locale} onChange={event => {write(UI_LOCALE_STORAGE_KEY,event.target.value);window.dispatchEvent(new Event('site-pages-locale'));}}><option value="zh-CN">简体中文</option><option value="en">English</option></MainSelect>}/>
    <DonationDialog open={donation} onCloseRequest={() => setDonation(false)} assetUrl={url} onArtworkUnavailable={() => setDonationAvailable(false)}/>
    <FirstUseNotice ref={firstUse} open={notice} onOpenRequest={() => setNotice(true)} onCloseRequest={() => setNotice(false)} contentUrl={url('content/FIRST_USE_NOTICE.html')} edgeGestures={false}/>
  </>;
}
function Page({kind}:{kind:'header'|'uptime'}) {
  const [locale,setLocale] = useState<UiLocale>(() => read(UI_LOCALE_STORAGE_KEY)==='en'?'en':'zh-CN');
  useEffect(() => {
    const changed = () => setLocale(read(UI_LOCALE_STORAGE_KEY)==='en'?'en':'zh-CN');
    window.addEventListener('site-pages-locale',changed); window.addEventListener('storage',changed);
    return () => {window.removeEventListener('site-pages-locale',changed);window.removeEventListener('storage',changed);};
  }, []);
  return <LocaleProvider locale={locale}>{kind==='header'?<StandaloneHeader/>:<UptimePage endpoint={url('uptime/summary')}/>}</LocaleProvider>;
}
const header = document.getElementById('site-page-header'); if (header) createRoot(header).render(<Page kind="header"/>);
const content = document.getElementById('site-page-content'); if (content && document.body.dataset.sitePage==='uptime') createRoot(content).render(<Page kind="uptime"/>);

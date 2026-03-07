/**
 * Ghost Browser — Settings Bridge
 *
 * TypeScript module for the ghost://settings internal page.
 * Communicates with the C++ backend via QWebChannel.
 * Compiled to JS and embedded in Qt resources.
 */

interface GhostSettings {
  version: number;
  general: {
    startupBehavior: 'newTab' | 'lastSession' | 'specificPages';
    homePage: string;
    searchEngine: string;
  };
  appearance: {
    theme: 'dark' | 'light' | 'system';
    showBookmarksBar: boolean;
    fontSize: number;
    zoomLevel: number;
  };
  content: {
    autoplay: boolean;
    fullScreenVideo: boolean;
    youtubeShortsAsNormalVideos: boolean;
    siteSettings: {
      javascript: 'allow' | 'block';
      popups: 'allow' | 'block';
      notifications: 'ask' | 'allow' | 'block';
      location: 'ask' | 'allow' | 'block';
      camera: 'ask' | 'allow' | 'block';
      microphone: 'ask' | 'allow' | 'block';
    };
  };
  privacy: {
    doNotTrack: boolean;
    blockThirdPartyCookies: boolean;
    clearDataOnExit: boolean;
    httpsOnly: boolean;
  };
  downloads: {
    askWhereToSave: boolean;
    defaultPath: string;
  };
  languages: {
    spellCheck: boolean;
  };
  system: {
    hardwareAcceleration: boolean;
    backgroundApps: boolean;
    proxyMode: 'system' | 'none' | 'manual';
  };
  protection: {
    trackingLevel: 'standard' | 'aggressive' | 'disabled';
    httpsUpgrade: boolean;
    blockFingerprinting: boolean;
    blockScripts: boolean;
    safeBrowsing: boolean;
  };
  accessibility: {
    caretBrowsing: boolean;
    highContrast: boolean;
  };
}

declare interface Window {
  GhostSettingsBridge: {
    initializeSettingsPage: () => Promise<void>;
    refreshHistory?: () => Promise<void>;
    refreshCookies?: () => Promise<void>;
  };
  QWebChannel?: new (transport: unknown, callback: (channel: { objects: Record<string, ChannelObjects> }) => void) => void;
  qt?: { webChannelTransport?: unknown };
}

interface ChannelObjects {
  [key: string]: unknown;
}

interface GhostBridge {
  // Qt 6 QWebChannel: all invokable methods return Promise<T> when called without a callback.
  getSettingsJson(): Promise<string>;
  updateSetting(path: string, value: unknown): void;
  chooseDownloadPath(): Promise<string>;
  resetToDefaults(): Promise<boolean>;
  requestClearBrowsingData(): void;
}

interface GhostHistoryBridge {
  getHistoryJson(): Promise<string>;
  clearAll(): void;
  clearByAge(range: string): void;
  deleteEntry(index: number): void;
  historyChanged: { connect: (callback: () => void) => void };
}

interface GhostCookieBridge {
  getCookiesJson(): Promise<string>;
  deleteByIndex(index: number): void;
  clearByAge(range: string): void;
  clearAll(): void;
  reload(): void;
  cookiesChanged: { connect: (callback: () => void) => void };
}

interface CookieEntry {
  index: number;
  name: string;
  domain: string;
  path: string;
  value: string;
  secure: boolean;
  httpOnly: boolean;
  session: boolean;
  expires: string;
}

interface HistoryEntry {
  url: string;
  title: string;
  time: string;
}

type GhostTheme = 'dark' | 'light' | 'system';

function defaultSettings(): GhostSettings {
  return {
    version: 1,
    general: {
      startupBehavior: 'newTab',
      homePage: 'ghost://newtab',
      searchEngine: 'duckduckgo',
    },
    appearance: {
      theme: 'dark',
      showBookmarksBar: false,
      fontSize: 16,
      zoomLevel: 100,
    },
    content: {
      autoplay: true,
      fullScreenVideo: true,
      youtubeShortsAsNormalVideos: true,
      siteSettings: {
        javascript: 'allow',
        popups: 'block',
        notifications: 'ask',
        location: 'ask',
        camera: 'ask',
        microphone: 'ask',
      },
    },
    privacy: {
      doNotTrack: true,
      blockThirdPartyCookies: true,
      clearDataOnExit: false,
      httpsOnly: false,
    },
    downloads: {
      askWhereToSave: false,
      defaultPath: '',
    },
    languages: {
      spellCheck: true,
    },
    system: {
      hardwareAcceleration: true,
      backgroundApps: false,
      proxyMode: 'system',
    },
    protection: {
      trackingLevel: 'aggressive',
      httpsUpgrade: true,
      blockFingerprinting: true,
      blockScripts: false,
      safeBrowsing: true,
    },
    accessibility: {
      caretBrowsing: false,
      highContrast: false,
    },
  };
}

function getAtPath(settings: Record<string, unknown>, path: string): unknown {
  return path.split('.').reduce<unknown>((current, key) => {
    if (current && typeof current === 'object' && key in current) {
      return (current as Record<string, unknown>)[key];
    }
    return undefined;
  }, settings);
}

function coerceValue(rawValue: string): string | number {
  return /^\d+$/.test(rawValue) ? Number(rawValue) : rawValue;
}

interface Bridges {
  settings: GhostBridge | null;
  history: GhostHistoryBridge | null;
  cookies: GhostCookieBridge | null;
}

function connectBridge(): Promise<Bridges> {
  return new Promise((resolve) => {
    // Qt injects qt.webChannelTransport before page scripts run, but on some
    // QRC pages with named profiles the injection can lag by a few ticks.
    // Retry every 50 ms for up to 5 s before giving up.
    let attempts = 0;
    const MAX_ATTEMPTS = 100;
    function tryConnect(): void {
      if (window.QWebChannel && window.qt?.webChannelTransport) {
        new window.QWebChannel(window.qt.webChannelTransport, (channel: { objects: Record<string, unknown> }) => {
          resolve({
            settings: (channel.objects.ghostSettings as GhostBridge) ?? null,
            history: (channel.objects.ghostHistory as GhostHistoryBridge) ?? null,
            cookies: (channel.objects.ghostCookies as GhostCookieBridge) ?? null,
          });
        });
        return;
      }
      if (++attempts >= MAX_ATTEMPTS) {
        resolve({ settings: null, history: null, cookies: null });
        return;
      }
      setTimeout(tryConnect, 50);
    }
    tryConnect();
  });
}

function applyState(settings: GhostSettings): void {
  document.querySelectorAll<HTMLElement>('[data-setting-path]').forEach((element) => {
    const path = element.dataset.settingPath;
    if (!path) {
      return;
    }

    const value = getAtPath(settings as unknown as Record<string, unknown>, path);

    if (element.classList.contains('toggle')) {
      element.classList.toggle('on', Boolean(value));
      return;
    }

    if (element.classList.contains('radio-row')) {
      const selected = String(value) === element.dataset.settingValue;
      element.querySelector('.radio-dot')?.classList.toggle('selected', selected);
      return;
    }

    if (element instanceof HTMLSelectElement && value !== undefined) {
      element.value = String(value);
    }
  });

  const downloadPathLabel = document.getElementById('downloadPathValue');
  if (downloadPathLabel) {
    downloadPathLabel.textContent = settings.downloads.defaultPath || 'Downloads';
  }

  const notificationsDefaultSummary = document.getElementById('notificationsDefaultSummary');
  if (notificationsDefaultSummary) {
    notificationsDefaultSummary.textContent = formatPermissionSummary(
      settings.content.siteSettings.notifications,
      'notifications'
    );
  }

  const cameraMicrophoneDefaultSummary = document.getElementById('cameraMicrophoneDefaultSummary');
  if (cameraMicrophoneDefaultSummary) {
    cameraMicrophoneDefaultSummary.textContent = `Camera: ${formatPermissionSummary(settings.content.siteSettings.camera, 'camera')} Microphone: ${formatPermissionSummary(settings.content.siteSettings.microphone, 'microphone')}`;
  }

  applyTheme(settings.appearance.theme);
}

function formatPermissionSummary(
  value: 'ask' | 'allow' | 'block',
  target: 'notifications' | 'camera' | 'microphone'
): string {
  switch (target) {
    case 'notifications':
      if (value === 'allow') return 'Sites can send notifications automatically.';
      if (value === 'block') return 'Sites are blocked from sending notifications.';
      return 'Ask before sites can send notifications.';
    case 'camera':
      if (value === 'allow') return 'Allow automatically.';
      if (value === 'block') return 'Blocked automatically.';
      return 'Ask before access.';
    case 'microphone':
      if (value === 'allow') return 'Allow automatically.';
      if (value === 'block') return 'Blocked automatically.';
      return 'Ask before access.';
  }
}

function applyTheme(theme: GhostTheme): void {
  const useLight = theme === 'light'
    || (theme === 'system' && window.matchMedia('(prefers-color-scheme: light)').matches);
  document.body.classList.toggle('light-theme', useLight);
}

// ── History grouping helpers ──

function getTimeGroup(utcIso: string): string {
  const visited = new Date(utcIso);
  const now = new Date();
  const diffMs = now.getTime() - visited.getTime();
  const diffHours = diffMs / (1000 * 60 * 60);
  const diffDays = diffMs / (1000 * 60 * 60 * 24);

  if (diffHours < 1) return 'Just now';
  if (diffHours < 3) return 'A few hours ago';
  if (diffHours < 24) return 'Earlier today';

  const yesterday = new Date(now);
  yesterday.setDate(yesterday.getDate() - 1);
  if (visited.toDateString() === yesterday.toDateString()) return 'Yesterday';

  if (diffDays < 7) return `${Math.floor(diffDays)} days ago`;
  if (diffDays < 14) return 'Last week';
  if (diffDays < 30) return `${Math.floor(diffDays / 7)} weeks ago`;
  if (diffDays < 60) return 'Last month';
  return 'Older';
}

function formatTime(utcIso: string): string {
  const d = new Date(utcIso);
  return d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
}

function domainFromUrl(url: string): string {
  try { return new URL(url).hostname; } catch { return ''; }
}

function escapeHtml(text: string): string {
  const el = document.createElement('span');
  el.textContent = text;
  return el.innerHTML;
}

function renderHistory(entries: HistoryEntry[], container: HTMLElement, filter: string, onDelete: (idx: number) => void): void {
  container.innerHTML = '';

  const filtered = filter
    ? entries.filter((e) => e.title.toLowerCase().includes(filter) || e.url.toLowerCase().includes(filter))
    : entries;

  if (filtered.length === 0) {
    container.innerHTML = `<div class="history-empty">${filter ? 'No matching history.' : 'No browsing history yet.'}</div>`;
    return;
  }

  let currentGroup = '';
  const fragment = document.createDocumentFragment();

  filtered.forEach((entry, visibleIdx) => {
    const group = getTimeGroup(entry.time);
    if (group !== currentGroup) {
      currentGroup = group;
      const header = document.createElement('div');
      header.className = 'history-group-header';
      header.textContent = group;
      fragment.appendChild(header);
    }

    // Find original index in unfiltered list for deletion
    const originalIdx = filter ? entries.indexOf(entry) : visibleIdx;

    const row = document.createElement('div');
    row.className = 'history-entry';
    row.innerHTML = `
      <span class="h-time">${escapeHtml(formatTime(entry.time))}</span>
      <span class="h-favicon">${escapeHtml(domainFromUrl(entry.url).charAt(0).toUpperCase())}</span>
      <span class="h-title">${escapeHtml(entry.title)}</span>
      <span class="h-url">${escapeHtml(domainFromUrl(entry.url))}</span>
      <button class="h-delete" data-idx="${originalIdx}" title="Remove">✕</button>
    `;

    row.addEventListener('click', (e) => {
      if ((e.target as HTMLElement).closest('.h-delete')) return;
      window.location.href = entry.url;
    });

    const delBtn = row.querySelector('.h-delete') as HTMLButtonElement;
    delBtn.addEventListener('click', (e) => {
      e.stopPropagation();
      onDelete(originalIdx);
    });

    fragment.appendChild(row);
  });

  container.appendChild(fragment);
}

function renderCookies(entries: CookieEntry[], container: HTMLElement, filter: string, onDelete: (idx: number) => void): void {
  container.innerHTML = '';

  const filtered = filter
    ? entries.filter((c) => c.domain.toLowerCase().includes(filter) || c.name.toLowerCase().includes(filter))
    : entries;

  if (filtered.length === 0) {
    container.innerHTML = `<div class="cookie-empty">${filter ? 'No matching cookies.' : 'No cookies stored.'}</div>`;
    return;
  }

  // Group by domain
  const groups = new Map<string, CookieEntry[]>();
  for (const c of filtered) {
    const key = c.domain.replace(/^\./, '');
    if (!groups.has(key)) groups.set(key, []);
    groups.get(key)!.push(c);
  }

  const fragment = document.createDocumentFragment();

  for (const [domain, cookies] of groups) {
    const header = document.createElement('div');
    header.className = 'cookie-group-header';
    header.textContent = `${domain}  (${cookies.length})`;
    fragment.appendChild(header);

    for (const cookie of cookies) {
      const row = document.createElement('div');
      row.className = 'cookie-entry';

      const flags: string[] = [];
      if (cookie.secure) flags.push('Secure');
      if (cookie.httpOnly) flags.push('HttpOnly');
      if (cookie.session) flags.push('Session');

      row.innerHTML = `
        <span class="c-name" title="${escapeHtml(cookie.name)}">${escapeHtml(cookie.name)}</span>
        <span class="c-domain">${escapeHtml(cookie.domain)}</span>
        <span class="c-flags">${flags.map((f) => `<span>${f}</span>`).join('')}</span>
        <button class="c-delete" data-idx="${cookie.index}" title="Remove">✕</button>
      `;

      const delBtn = row.querySelector('.c-delete') as HTMLButtonElement;
      delBtn.addEventListener('click', (e) => {
        e.stopPropagation();
        onDelete(cookie.index);
      });

      fragment.appendChild(row);
    }
  }

  container.appendChild(fragment);
}

async function initializeSettingsPage(): Promise<void> {
  const { settings: bridge, history: historyBridge, cookies: cookieBridge } = await connectBridge();
  // Qt 6 QWebChannel methods return Promises — must be awaited.
  let settings = bridge ? JSON.parse(await bridge.getSettingsJson()) as GhostSettings : defaultSettings();

  applyState(settings);

  document.addEventListener('click', (event) => {
    const toggle = (event.target as HTMLElement).closest<HTMLElement>('.toggle[data-setting-path]');
    if (!toggle) {
      return;
    }

    const path = toggle.dataset.settingPath;
    if (!path) {
      return;
    }

    const nextValue = !toggle.classList.contains('on');
    toggle.classList.toggle('on', nextValue);
    bridge?.updateSetting(path, nextValue);
  });

  document.addEventListener('click', (event) => {
    const row = (event.target as HTMLElement).closest<HTMLElement>('.radio-row[data-setting-path]');
    if (!row) {
      return;
    }

    const path = row.dataset.settingPath;
    const value = row.dataset.settingValue;
    if (!path || value === undefined) {
      return;
    }

    row.closest('.radio-group')?.querySelectorAll('.radio-dot').forEach((dot) => dot.classList.remove('selected'));
    row.querySelector('.radio-dot')?.classList.add('selected');
    if (path === 'appearance.theme') {
      applyTheme(value as GhostTheme);
    }
    bridge?.updateSetting(path, value);
  });

  document.querySelectorAll<HTMLSelectElement>('select[data-setting-path]').forEach((select) => {
    select.addEventListener('change', () => {
      const path = select.dataset.settingPath;
      if (!path) {
        return;
      }

      bridge?.updateSetting(path, coerceValue(select.value));
    });
  });

  document.querySelectorAll<HTMLButtonElement>('button[data-action]').forEach((button) => {
    button.addEventListener('click', async () => {
      if (!bridge) {
        return;
      }

      const action = button.dataset.action;
      if (action === 'chooseDownloadPath') {
        const selectedPath = await bridge.chooseDownloadPath();
        if (!selectedPath) {
          return;
        }
      } else if (action === 'resetSettings') {
        if (!(await bridge.resetToDefaults())) {
          return;
        }
      } else if (action === 'clearBrowsingData') {
        bridge.requestClearBrowsingData();
      }

      settings = JSON.parse(await bridge.getSettingsJson()) as GhostSettings;
      applyState(settings);
    });
  });

  // ── History wiring ──
  const historyList = document.getElementById('historyList');
  const historySearch = document.getElementById('historySearch') as HTMLInputElement | null;
  const historyClearRange = document.getElementById('historyClearRange') as HTMLSelectElement | null;
  let historyEntries: HistoryEntry[] = [];
  let historyFilter = '';

  async function refreshHistory(): Promise<void> {
    if (!historyBridge || !historyList) return;
    historyEntries = JSON.parse(await historyBridge.getHistoryJson()) as HistoryEntry[];
    renderHistory(historyEntries, historyList, historyFilter, (idx: number) => {
      historyBridge!.deleteEntry(idx);
    });
  }

  // Expose for top-bar click trigger
  window.GhostSettingsBridge.refreshHistory = refreshHistory;

  if (historyBridge) {
    historyBridge.historyChanged.connect(refreshHistory);
  }

  if (historySearch) {
    historySearch.addEventListener('input', () => {
      historyFilter = historySearch.value.toLowerCase();
      if (historyList) {
        renderHistory(historyEntries, historyList, historyFilter, (idx: number) => {
          historyBridge?.deleteEntry(idx);
        });
      }
    });
  }

  if (historyClearRange) {
    historyClearRange.addEventListener('change', () => {
      const range = historyClearRange.value;
      if (!range || !historyBridge) return;
      historyBridge.clearByAge(range);
      historyClearRange.selectedIndex = 0;
    });
  }

  // ── Cookie wiring ──
  const cookieList = document.getElementById('cookieList');
  const cookieSearch = document.getElementById('cookieSearch') as HTMLInputElement | null;
  const cookieClearRange = document.getElementById('cookieClearRange') as HTMLSelectElement | null;
  let cookieEntries: CookieEntry[] = [];
  let cookieFilter = '';

  async function refreshCookies(): Promise<void> {
    if (!cookieBridge || !cookieList) return;
    cookieEntries = JSON.parse(await cookieBridge.getCookiesJson()) as CookieEntry[];
    renderCookies(cookieEntries, cookieList, cookieFilter, (idx: number) => {
      cookieBridge!.deleteByIndex(idx);
    });
  }

  window.GhostSettingsBridge.refreshCookies = refreshCookies;

  if (cookieBridge) {
    cookieBridge.cookiesChanged.connect(refreshCookies);
  }

  if (cookieSearch) {
    cookieSearch.addEventListener('input', () => {
      cookieFilter = cookieSearch.value.toLowerCase();
      if (cookieList) {
        renderCookies(cookieEntries, cookieList, cookieFilter, (idx: number) => {
          cookieBridge?.deleteByIndex(idx);
        });
      }
    });
  }

  if (cookieClearRange) {
    cookieClearRange.addEventListener('change', () => {
      const range = cookieClearRange.value;
      if (!range || !cookieBridge) return;
      cookieBridge.clearByAge(range);
      cookieClearRange.selectedIndex = 0;
    });
  }

  // Eagerly populate as soon as bridge is ready, regardless of which tab is visible
  refreshHistory();
  refreshCookies();
}

window.GhostSettingsBridge = {
  initializeSettingsPage,
};

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
    profileName: string;
    newTabModules: {
      weather: boolean;
      shortcuts: boolean;
      briefing: boolean;
      focus: boolean;
    };
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
    sitePermissionRules: SitePermissionRules;
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
    refreshCookies?: (forceReload?: boolean) => Promise<void>;
    refreshProtection?: () => Promise<void>;
    refreshBookmarks?: () => Promise<void>;
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
  getSitePermissionRulesJson(): Promise<string>;
  updateSetting(path: string, value: unknown): void;
  importSettingsFromFile(): Promise<string>;
  chooseDownloadPath(): Promise<string>;
  openDefaultAppsSettings(): Promise<boolean>;
  getDefaultBrowserStatus(): Promise<string>;
  resetToDefaults(): Promise<boolean>;
  requestClearBrowsingData(): void;
  upsertSitePermissionRule(permissionType: string, origin: string, policy: string): Promise<boolean>;
  removeSitePermissionRule(permissionType: string, origin: string): Promise<boolean>;
  settingsChanged: { connect: (callback: (json: string) => void) => void };
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

interface GhostProtectionBridge {
  getEventsJson(): Promise<string>;
  clear(): void;
  eventsChanged: { connect: (callback: () => void) => void };
}

interface GhostBookmarkEntry {
  id: string;
  title: string;
  url: string;
  createdAt?: string;
}

interface GhostBookmarkBridge {
  getBookmarksJson(): Promise<string>;
  addBookmark(title: string, url: string): Promise<boolean>;
  updateBookmark(id: string, title: string, url: string): Promise<boolean>;
  deleteBookmark(id: string): Promise<boolean>;
  importBookmarksFromFile(): Promise<string>;
  exportBookmarksToFile(): Promise<string>;
  bookmarksChanged: { connect: (callback: () => void) => void };
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

interface ProtectionEvent {
  action: 'blocked' | 'upgraded';
  category: string;
  url: string;
  host: string;
  page: string;
  detail: string;
  time: string;
}

type SitePermissionPolicy = 'ask' | 'allow' | 'block';
type SitePermissionType = 'notifications' | 'location' | 'camera' | 'microphone';

interface SitePermissionRule {
  origin: string;
  policy: SitePermissionPolicy;
}

interface SitePermissionRules {
  notifications: SitePermissionRule[];
  location: SitePermissionRule[];
  camera: SitePermissionRule[];
  microphone: SitePermissionRule[];
}

type GhostTheme = 'dark' | 'light' | 'system';

function defaultSettings(): GhostSettings {
  return {
    version: 1,
    general: {
      startupBehavior: 'newTab',
      homePage: 'ghost://newtab',
      searchEngine: 'duckduckgo',
      profileName: 'Ghost User',
      newTabModules: {
        weather: true,
        shortcuts: true,
        briefing: true,
        focus: true,
      },
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
      sitePermissionRules: {
        notifications: [],
        location: [],
        camera: [],
        microphone: [],
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

function normalizeSettings(raw: unknown): GhostSettings {
  const defaults = defaultSettings();
  const source = raw && typeof raw === 'object' ? raw as Record<string, unknown> : {};

  return {
    ...defaults,
    ...source,
    general: {
      ...defaults.general,
      ...(source.general as Record<string, unknown> | undefined),
      newTabModules: {
        ...defaults.general.newTabModules,
        ...((source.general as { newTabModules?: Record<string, unknown> } | undefined)?.newTabModules ?? {}),
      },
    },
    appearance: {
      ...defaults.appearance,
      ...(source.appearance as Record<string, unknown> | undefined),
    },
    content: {
      ...defaults.content,
      ...(source.content as Record<string, unknown> | undefined),
      siteSettings: {
        ...defaults.content.siteSettings,
        ...((source.content as { siteSettings?: Record<string, unknown> } | undefined)?.siteSettings ?? {}),
      },
      sitePermissionRules: {
        ...defaults.content.sitePermissionRules,
        ...((source.content as { sitePermissionRules?: Partial<SitePermissionRules> } | undefined)?.sitePermissionRules ?? {}),
      },
    },
    privacy: {
      ...defaults.privacy,
      ...(source.privacy as Record<string, unknown> | undefined),
    },
    downloads: {
      ...defaults.downloads,
      ...(source.downloads as Record<string, unknown> | undefined),
    },
    languages: {
      ...defaults.languages,
      ...(source.languages as Record<string, unknown> | undefined),
    },
    system: {
      ...defaults.system,
      ...(source.system as Record<string, unknown> | undefined),
    },
    protection: {
      ...defaults.protection,
      ...(source.protection as Record<string, unknown> | undefined),
    },
    accessibility: {
      ...defaults.accessibility,
      ...(source.accessibility as Record<string, unknown> | undefined),
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
  protection: GhostProtectionBridge | null;
  bookmarks: GhostBookmarkBridge | null;
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
            protection: (channel.objects.ghostProtection as GhostProtectionBridge) ?? null,
            bookmarks: (channel.objects.ghostBookmarks as GhostBookmarkBridge) ?? null,
          });
        });
        return;
      }
      if (++attempts >= MAX_ATTEMPTS) {
        resolve({ settings: null, history: null, cookies: null, protection: null, bookmarks: null });
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
      return;
    }

    if (element instanceof HTMLInputElement && value !== undefined) {
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

  const locationDefaultSummary = document.getElementById('locationDefaultSummary');
  if (locationDefaultSummary) {
    locationDefaultSummary.textContent = formatPermissionSummary(
      settings.content.siteSettings.location,
      'location'
    );
  }

  const cameraMicrophoneDefaultSummary = document.getElementById('cameraMicrophoneDefaultSummary');
  if (cameraMicrophoneDefaultSummary) {
    cameraMicrophoneDefaultSummary.textContent = `Camera: ${formatPermissionSummary(settings.content.siteSettings.camera, 'camera')} Microphone: ${formatPermissionSummary(settings.content.siteSettings.microphone, 'microphone')}`;
  }

  renderSitePermissionPanel('notifications', settings.content.sitePermissionRules.notifications);
  renderSitePermissionPanel('location', settings.content.sitePermissionRules.location);
  renderSitePermissionPanel('camera', settings.content.sitePermissionRules.camera);
  renderSitePermissionPanel('microphone', settings.content.sitePermissionRules.microphone);

  applyTheme(settings.appearance.theme);
}

function permissionTypeLabel(permissionType: SitePermissionType): string {
  switch (permissionType) {
    case 'notifications':
      return 'Notifications';
    case 'location':
      return 'Location';
    case 'camera':
      return 'Camera';
    case 'microphone':
      return 'Microphone';
  }
}

function policyLabel(policy: SitePermissionPolicy): string {
  switch (policy) {
    case 'allow':
      return 'Allow';
    case 'block':
      return 'Block';
    case 'ask':
      return 'Ask';
  }
}

function renderSitePermissionPanel(permissionType: SitePermissionType, rules: SitePermissionRule[]): void {
  const list = document.getElementById(`${permissionType}SitePermissionList`);
  if (!list) {
    return;
  }

  const sortedRules = [...rules].sort((left, right) => left.origin.localeCompare(right.origin));
  if (sortedRules.length === 0) {
    list.innerHTML = `<div class="site-permission-empty">No saved ${permissionTypeLabel(permissionType).toLowerCase()} site rules yet.</div>`;
    return;
  }

  list.innerHTML = '';
  const fragment = document.createDocumentFragment();
  for (const rule of sortedRules) {
    const row = document.createElement('div');
    row.className = 'site-permission-entry';
    row.innerHTML = `
      <div>
        <div class="entry-title">${escapeHtml(rule.origin)}</div>
        <div class="entry-meta">Current rule: ${escapeHtml(policyLabel(rule.policy))}</div>
      </div>
      <div class="site-permission-actions">
        <select class="setting-select site-permission-select" data-permission-type="${permissionType}" data-origin="${escapeHtml(rule.origin)}">
          <option value="ask" ${rule.policy === 'ask' ? 'selected' : ''}>Ask</option>
          <option value="allow" ${rule.policy === 'allow' ? 'selected' : ''}>Allow</option>
          <option value="block" ${rule.policy === 'block' ? 'selected' : ''}>Block</option>
        </select>
        <button class="secondary-button site-permission-remove" data-permission-type="${permissionType}" data-origin="${escapeHtml(rule.origin)}">Remove</button>
      </div>
    `;
    fragment.appendChild(row);
  }

  list.appendChild(fragment);
}

function formatPermissionSummary(
  value: 'ask' | 'allow' | 'block',
  target: 'notifications' | 'location' | 'camera' | 'microphone'
): string {
  switch (target) {
    case 'notifications':
      if (value === 'allow') return 'Sites can send notifications automatically.';
      if (value === 'block') return 'Sites are blocked from sending notifications.';
      return 'Ask before sites can send notifications.';
    case 'location':
      if (value === 'allow') return 'Sites can access your location automatically.';
      if (value === 'block') return 'Sites are blocked from accessing your location.';
      return 'Ask before sharing your location.';
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

function renderCookieLoading(container: HTMLElement): void {
  container.innerHTML = '<div class="cookie-empty">Loading cookies…</div>';
}

function renderProtectionDiagnostics(
  entries: ProtectionEvent[],
  list: HTMLElement,
  summary: HTMLElement | null,
  siteFilter: string,
  categoryFilter: string
): void {
  const normalizedSiteFilter = siteFilter.trim().toLowerCase();
  const filtered = entries.filter((entry) => {
    const host = (entry.host || domainFromUrl(entry.url)).toLowerCase();
    const page = entry.page.toLowerCase();
    const siteMatches = !normalizedSiteFilter
      || host.includes(normalizedSiteFilter)
      || entry.url.toLowerCase().includes(normalizedSiteFilter)
      || page.includes(normalizedSiteFilter);

    let categoryMatches = true;
    if (categoryFilter.startsWith('action:')) {
      categoryMatches = entry.action === categoryFilter.slice(7);
    } else if (categoryFilter.startsWith('category:')) {
      categoryMatches = entry.category === categoryFilter.slice(9);
    }

    return siteMatches && categoryMatches;
  });

  const blockedCount = filtered.filter((entry) => entry.action === 'blocked').length;
  const upgradedCount = filtered.filter((entry) => entry.action === 'upgraded').length;
  const latestCategory = filtered[0]?.category || 'none';

  if (summary) {
    summary.innerHTML = `
      <div class="protection-stat">
        <div class="stat-label">Blocked Requests</div>
        <div class="stat-value">${blockedCount}</div>
      </div>
      <div class="protection-stat">
        <div class="stat-label">HTTPS Upgrades</div>
        <div class="stat-value">${upgradedCount}</div>
      </div>
      <div class="protection-stat">
        <div class="stat-label">Latest Category</div>
        <div class="stat-value">${escapeHtml(latestCategory.replace(/-/g, ' '))}</div>
      </div>
    `;
  }

  if (entries.length === 0) {
    list.innerHTML = '<div class="protection-empty">No protection activity recorded yet.</div>';
    return;
  }

  if (filtered.length === 0) {
    list.innerHTML = '<div class="protection-empty">No protection activity matches the current filters.</div>';
    return;
  }

  list.innerHTML = '';
  const fragment = document.createDocumentFragment();
  filtered.slice(0, 20).forEach((entry) => {
    const row = document.createElement('div');
    row.className = 'protection-entry';
    const title = entry.action === 'blocked'
      ? `Blocked ${entry.category.replace(/-/g, ' ')}`
      : 'Upgraded insecure request';
    row.innerHTML = `
      <div class="protection-badge ${entry.action}">${escapeHtml(entry.action)}</div>
      <div>
        <div class="entry-title">${escapeHtml(title)}</div>
        <div class="entry-meta">${escapeHtml(entry.host || entry.url)}</div>
        <div class="entry-detail">${escapeHtml(entry.detail)}</div>
        ${entry.page ? `<div class="entry-meta">From ${escapeHtml(entry.page)}</div>` : ''}
      </div>
      <div class="entry-time">${escapeHtml(formatTime(entry.time))}</div>
    `;
    fragment.appendChild(row);
  });

  list.appendChild(fragment);
}

async function initializeSettingsPage(): Promise<void> {
  const {
    settings: bridge,
    history: historyBridge,
    cookies: cookieBridge,
    protection: protectionBridge,
    bookmarks: bookmarkBridge,
  } = await connectBridge();
  // Qt 6 QWebChannel methods return Promises — must be awaited.
  let settings = bridge ? normalizeSettings(JSON.parse(await bridge.getSettingsJson())) : defaultSettings();


  async function refreshSettingsState(): Promise<void> {
    if (!bridge) {
      return;
    }

    settings = normalizeSettings(JSON.parse(await bridge.getSettingsJson()));
    applyState(settings);
  }

  applyState(settings);

  if (bridge) {
    bridge.settingsChanged.connect((json: string) => {
      settings = normalizeSettings(JSON.parse(json));
      applyState(settings);
    });
  }

  const defaultBrowserSummary = document.getElementById('defaultBrowserSummary');
  const defaultBrowserStatusBadge = document.getElementById('defaultBrowserStatusBadge');
  async function refreshDefaultBrowserStatus(): Promise<void> {
    if (!defaultBrowserSummary || !bridge) {
      return;
    }

    const status = await bridge.getDefaultBrowserStatus();
    if (status === 'default') {
      defaultBrowserSummary.textContent = 'Ghost is currently the default browser — HTTP and HTTPS links open here.';
      if (defaultBrowserStatusBadge) {
        defaultBrowserStatusBadge.textContent = 'Default';
        defaultBrowserStatusBadge.className = 'status-tag tag-ok';
      }
    } else if (status === 'not-default') {
      defaultBrowserSummary.textContent = 'Ghost is not the default browser. Click "Open Defaults" and set Ghost for HTTP and HTTPS, then come back to confirm.';
      if (defaultBrowserStatusBadge) {
        defaultBrowserStatusBadge.textContent = 'Not Default';
        defaultBrowserStatusBadge.className = 'status-tag tag-err';
      }
    } else if (status === 'unsupported') {
      defaultBrowserSummary.textContent = 'Default browser detection is only available on Windows.';
      if (defaultBrowserStatusBadge) {
        defaultBrowserStatusBadge.textContent = 'Unsupported';
        defaultBrowserStatusBadge.className = 'status-tag tag-dim';
      }
    } else {
      defaultBrowserSummary.textContent = 'Could not confirm the default browser association.';
      if (defaultBrowserStatusBadge) {
        defaultBrowserStatusBadge.textContent = 'Unknown';
        defaultBrowserStatusBadge.className = 'status-tag tag-dim';
      }
    }
  }

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

  document.querySelectorAll<HTMLInputElement>('input[data-setting-path]').forEach((input) => {
    const commit = () => {
      const path = input.dataset.settingPath;
      if (!path) {
        return;
      }

      const nextValue = input.value.trim();
      if (!nextValue) {
        input.value = String(getAtPath(settings as unknown as Record<string, unknown>, path) || '');
        return;
      }

      bridge?.updateSetting(path, nextValue);
    };

    input.addEventListener('change', commit);
    input.addEventListener('blur', commit);
  });

  document.querySelectorAll<HTMLButtonElement>('button[data-action]').forEach((button) => {
    button.addEventListener('click', async () => {
      if (!bridge) {
        return;
      }

      const action = button.dataset.action;
      if (action === 'importSettings') {
        const importedPath = await bridge.importSettingsFromFile();
        if (!importedPath) {
          return;
        }
      } else if (action === 'openDefaultAppsSettings') {
        await bridge.openDefaultAppsSettings();
        await refreshDefaultBrowserStatus();
        return;
      } else if (action === 'chooseDownloadPath') {
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
      } else if (action === 'clearProtectionDiagnostics') {
        protectionBridge?.clear();
        return;
      }

      await refreshSettingsState();
    });
  });

  document.querySelectorAll<HTMLButtonElement>('button[data-bookmark-action]').forEach((button) => {
    button.addEventListener('click', async () => {
      if (!bookmarkBridge) {
        return;
      }

      const action = button.dataset.bookmarkAction;
      if (action === 'import') {
        const importedPath = await bookmarkBridge.importBookmarksFromFile();
        if (!importedPath) {
          return;
        }
        await refreshBookmarks();
        return;
      }

      if (action === 'export') {
        await bookmarkBridge.exportBookmarksToFile();
      }
    });
  });

  document.addEventListener('change', async (event) => {
    const select = (event.target as HTMLElement).closest<HTMLSelectElement>('.site-permission-select');
    if (!select || !bridge) {
      return;
    }

    const permissionType = select.dataset.permissionType;
    const origin = select.dataset.origin;
    if (!permissionType || !origin) {
      return;
    }

    await bridge.upsertSitePermissionRule(permissionType, origin, select.value);
    await refreshSettingsState();
  });

  document.addEventListener('click', async (event) => {
    const button = (event.target as HTMLElement).closest<HTMLButtonElement>('.site-permission-remove');
    if (!button || !bridge) {
      return;
    }

    const permissionType = button.dataset.permissionType;
    const origin = button.dataset.origin;
    if (!permissionType || !origin) {
      return;
    }

    await bridge.removeSitePermissionRule(permissionType, origin);
    await refreshSettingsState();
  });

  document.querySelectorAll<HTMLFormElement>('form[data-site-permission-form]').forEach((form) => {
    form.addEventListener('submit', async (event) => {
      event.preventDefault();
      if (!bridge) {
        return;
      }

      const permissionType = form.dataset.sitePermissionForm as SitePermissionType | undefined;
      const originInput = form.querySelector<HTMLInputElement>('input[name="origin"]');
      const policySelect = form.querySelector<HTMLSelectElement>('select[name="policy"]');
      if (!permissionType || !originInput || !policySelect) {
        return;
      }

      const rawOrigin = originInput.value.trim();
      try {
        const parsed = new URL(rawOrigin);
        if (!parsed.protocol || !parsed.hostname) {
          throw new Error('Invalid origin');
        }
      } catch {
        originInput.setCustomValidity('Enter a full origin like https://example.com');
        originInput.reportValidity();
        return;
      }

      originInput.setCustomValidity('');
      await bridge.upsertSitePermissionRule(permissionType, rawOrigin, policySelect.value);
      form.reset();
      policySelect.value = 'ask';
      await refreshSettingsState();
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
  const bookmarkList = document.getElementById('bookmarkList');
  const bookmarkForm = document.getElementById('bookmarkForm') as HTMLFormElement | null;
  const cookieList = document.getElementById('cookieList');
  const cookieSearch = document.getElementById('cookieSearch') as HTMLInputElement | null;
  const cookieClearRange = document.getElementById('cookieClearRange') as HTMLSelectElement | null;
  const protectionList = document.getElementById('protectionDiagnosticsList');
  const protectionSummary = document.getElementById('protectionSummary');
  const protectionSiteFilter = document.getElementById('protectionSiteFilter') as HTMLInputElement | null;
  const protectionCategoryFilter = document.getElementById('protectionCategoryFilter') as HTMLSelectElement | null;
  let cookieEntries: CookieEntry[] = [];
  let cookieFilter = '';
  let cookieRefreshToken = 0;
  let bookmarkEntries: GhostBookmarkEntry[] = [];
  let protectionEntries: ProtectionEvent[] = [];
  let protectionSiteQuery = '';
  let protectionCategoryQuery = 'all';

  async function refreshCookies(forceReload = false): Promise<void> {
    if (!cookieBridge || !cookieList) return;

    if (forceReload) {
      cookieRefreshToken += 1;
      renderCookieLoading(cookieList);
      cookieBridge.reload();
      return;
    }

    const refreshToken = ++cookieRefreshToken;
    cookieEntries = JSON.parse(await cookieBridge.getCookiesJson()) as CookieEntry[];
    if (refreshToken !== cookieRefreshToken) {
      return;
    }

    renderCookies(cookieEntries, cookieList, cookieFilter, (idx: number) => {
      cookieBridge!.deleteByIndex(idx);
    });
  }

  window.GhostSettingsBridge.refreshCookies = refreshCookies;

  async function refreshBookmarks(): Promise<void> {
    if (!bookmarkBridge || !bookmarkList) {
      return;
    }

    bookmarkEntries = JSON.parse(await bookmarkBridge.getBookmarksJson()) as GhostBookmarkEntry[];
    if (bookmarkEntries.length === 0) {
      bookmarkList.innerHTML = '<div class="cookie-empty">No bookmarks yet. Add one below to populate the toolbar.</div>';
      return;
    }

    bookmarkList.innerHTML = '';
    const fragment = document.createDocumentFragment();
    bookmarkEntries.forEach((entry) => {
      const row = document.createElement('div');
      row.className = 'cookie-entry';
      row.innerHTML = `
        <div style="display:flex;flex-direction:column;gap:4px;min-width:0;flex:1;">
          <span class="c-name" title="${escapeHtml(entry.title)}">${escapeHtml(entry.title)}</span>
          <span class="h-url">${escapeHtml(entry.url)}</span>
        </div>
        <div style="display:flex;gap:8px;align-items:center;">
          <button class="action-btn bookmark-edit" data-id="${escapeHtml(entry.id)}" type="button">Edit</button>
          <button class="action-btn danger bookmark-delete" data-id="${escapeHtml(entry.id)}" type="button">Remove</button>
        </div>
      `;
      row.addEventListener('click', (event) => {
        if ((event.target as HTMLElement).closest('button')) {
          return;
        }
        window.location.href = entry.url;
      });
      fragment.appendChild(row);
    });
    bookmarkList.appendChild(fragment);
  }

  window.GhostSettingsBridge.refreshBookmarks = refreshBookmarks;

  if (bookmarkBridge) {
    bookmarkBridge.bookmarksChanged.connect(refreshBookmarks);
  }

  if (bookmarkForm) {
    bookmarkForm.addEventListener('submit', async (event) => {
      event.preventDefault();
      if (!bookmarkBridge) {
        return;
      }

      const titleInput = bookmarkForm.querySelector<HTMLInputElement>('input[name="title"]');
      const urlInput = bookmarkForm.querySelector<HTMLInputElement>('input[name="url"]');
      if (!titleInput || !urlInput) {
        return;
      }

      const title = titleInput.value.trim();
      const url = urlInput.value.trim();
      if (!url) {
        urlInput.setCustomValidity('Enter a full URL like https://example.com');
        urlInput.reportValidity();
        return;
      }

      urlInput.setCustomValidity('');
      const editingId = bookmarkForm.dataset.editingId;
      const ok = editingId
        ? await bookmarkBridge.updateBookmark(editingId, title, url)
        : await bookmarkBridge.addBookmark(title, url);
      if (!ok) {
        urlInput.setCustomValidity('Ghost could not save that bookmark. Check the URL or try a different entry.');
        urlInput.reportValidity();
        return;
      }

      delete bookmarkForm.dataset.editingId;
      const submitButton = bookmarkForm.querySelector<HTMLButtonElement>('button[type="submit"]');
      if (submitButton) {
        submitButton.textContent = 'Add bookmark';
      }
      bookmarkForm.reset();
      await refreshBookmarks();
    });
  }

  document.addEventListener('click', async (event) => {
    const deleteButton = (event.target as HTMLElement).closest<HTMLButtonElement>('.bookmark-delete');
    if (deleteButton && bookmarkBridge) {
      await bookmarkBridge.deleteBookmark(deleteButton.dataset.id || '');
      await refreshBookmarks();
      return;
    }

    const editButton = (event.target as HTMLElement).closest<HTMLButtonElement>('.bookmark-edit');
    if (editButton && bookmarkForm) {
      const bookmark = bookmarkEntries.find((entry) => entry.id === (editButton.dataset.id || ''));
      if (!bookmark) {
        return;
      }

      const titleInput = bookmarkForm.querySelector<HTMLInputElement>('input[name="title"]');
      const urlInput = bookmarkForm.querySelector<HTMLInputElement>('input[name="url"]');
      const submitButton = bookmarkForm.querySelector<HTMLButtonElement>('button[type="submit"]');
      if (!titleInput || !urlInput) {
        return;
      }

      bookmarkForm.dataset.editingId = bookmark.id;
      titleInput.value = bookmark.title;
      urlInput.value = bookmark.url;
      if (submitButton) {
        submitButton.textContent = 'Save bookmark';
      }
    }
  });

  function refreshProtectionCategoryOptions(): void {
    if (!protectionCategoryFilter) {
      return;
    }

    const previousValue = protectionCategoryFilter.value || protectionCategoryQuery;
    const categories = Array.from(new Set(protectionEntries.map((entry) => entry.category))).sort();
    protectionCategoryFilter.innerHTML = `
      <option value="all">All activity</option>
      <option value="action:blocked">Blocked only</option>
      <option value="action:upgraded">HTTPS upgrades only</option>
      ${categories.map((category) => `<option value="category:${escapeHtml(category)}">${escapeHtml(category.replace(/-/g, ' '))}</option>`).join('')}
    `;

    const hasPreviousValue = Array.from(protectionCategoryFilter.options).some((option) => option.value === previousValue);
    protectionCategoryQuery = hasPreviousValue ? previousValue : 'all';
    protectionCategoryFilter.value = protectionCategoryQuery;
  }

  async function refreshProtection(): Promise<void> {
    if (!protectionBridge || !protectionList) return;
    protectionEntries = JSON.parse(await protectionBridge.getEventsJson()) as ProtectionEvent[];
    refreshProtectionCategoryOptions();
    renderProtectionDiagnostics(
      protectionEntries,
      protectionList,
      protectionSummary,
      protectionSiteQuery,
      protectionCategoryQuery
    );
  }

  window.GhostSettingsBridge.refreshProtection = refreshProtection;

  if (cookieBridge) {
    cookieBridge.cookiesChanged.connect(() => {
      refreshCookies();
    });
  }

  if (protectionBridge) {
    protectionBridge.eventsChanged.connect(refreshProtection);
  }

  if (protectionSiteFilter) {
    protectionSiteFilter.addEventListener('input', () => {
      protectionSiteQuery = protectionSiteFilter.value.toLowerCase();
      if (protectionList) {
        renderProtectionDiagnostics(
          protectionEntries,
          protectionList,
          protectionSummary,
          protectionSiteQuery,
          protectionCategoryQuery
        );
      }
    });
  }

  if (protectionCategoryFilter) {
    protectionCategoryFilter.addEventListener('change', () => {
      protectionCategoryQuery = protectionCategoryFilter.value;
      if (protectionList) {
        renderProtectionDiagnostics(
          protectionEntries,
          protectionList,
          protectionSummary,
          protectionSiteQuery,
          protectionCategoryQuery
        );
      }
    });
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

  await refreshDefaultBrowserStatus();

  // Eagerly populate as soon as bridge is ready, regardless of which tab is visible
  refreshHistory();
  refreshCookies(true);
  refreshProtection();
  refreshBookmarks();
}

window.GhostSettingsBridge = {
  initializeSettingsPage,
};

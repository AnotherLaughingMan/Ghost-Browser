"use strict";
/**
 * Ghost Browser — Settings Bridge
 *
 * TypeScript module for the ghost://settings internal page.
 * Communicates with the C++ backend via QWebChannel.
 * Compiled to JS and embedded in Qt resources.
 */
function defaultSettings() {
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
            statusOverlayMode: 'frosted',
            statusOverlayOpacity: 42,
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
function getAtPath(settings, path) {
    return path.split('.').reduce((current, key) => {
        if (current && typeof current === 'object' && key in current) {
            return current[key];
        }
        return undefined;
    }, settings);
}
function coerceValue(rawValue) {
    return /^\d+$/.test(rawValue) ? Number(rawValue) : rawValue;
}
function connectBridge() {
    return new Promise((resolve) => {
        // Qt injects qt.webChannelTransport before page scripts run, but on some
        // QRC pages with named profiles the injection can lag by a few ticks.
        // Retry every 50 ms for up to 5 s before giving up.
        let attempts = 0;
        const MAX_ATTEMPTS = 100;
        function tryConnect() {
            if (window.QWebChannel && window.qt?.webChannelTransport) {
                new window.QWebChannel(window.qt.webChannelTransport, (channel) => {
                    resolve({
                        settings: channel.objects.ghostSettings ?? null,
                        history: channel.objects.ghostHistory ?? null,
                        cookies: channel.objects.ghostCookies ?? null,
                        protection: channel.objects.ghostProtection ?? null,
                    });
                });
                return;
            }
            if (++attempts >= MAX_ATTEMPTS) {
                resolve({ settings: null, history: null, cookies: null, protection: null });
                return;
            }
            setTimeout(tryConnect, 50);
        }
        tryConnect();
    });
}
function applyState(settings) {
    document.querySelectorAll('[data-setting-path]').forEach((element) => {
        const path = element.dataset.settingPath;
        if (!path) {
            return;
        }
        const value = getAtPath(settings, path);
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
        notificationsDefaultSummary.textContent = formatPermissionSummary(settings.content.siteSettings.notifications, 'notifications');
    }
    const locationDefaultSummary = document.getElementById('locationDefaultSummary');
    if (locationDefaultSummary) {
        locationDefaultSummary.textContent = formatPermissionSummary(settings.content.siteSettings.location, 'location');
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
function permissionTypeLabel(permissionType) {
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
function policyLabel(policy) {
    switch (policy) {
        case 'allow':
            return 'Allow';
        case 'block':
            return 'Block';
        case 'ask':
            return 'Ask';
    }
}
function renderSitePermissionPanel(permissionType, rules) {
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
function formatPermissionSummary(value, target) {
    switch (target) {
        case 'notifications':
            if (value === 'allow')
                return 'Sites can send notifications automatically.';
            if (value === 'block')
                return 'Sites are blocked from sending notifications.';
            return 'Ask before sites can send notifications.';
        case 'location':
            if (value === 'allow')
                return 'Sites can access your location automatically.';
            if (value === 'block')
                return 'Sites are blocked from accessing your location.';
            return 'Ask before sharing your location.';
        case 'camera':
            if (value === 'allow')
                return 'Allow automatically.';
            if (value === 'block')
                return 'Blocked automatically.';
            return 'Ask before access.';
        case 'microphone':
            if (value === 'allow')
                return 'Allow automatically.';
            if (value === 'block')
                return 'Blocked automatically.';
            return 'Ask before access.';
    }
}
function applyTheme(theme) {
    const useLight = theme === 'light'
        || (theme === 'system' && window.matchMedia('(prefers-color-scheme: light)').matches);
    document.body.classList.toggle('light-theme', useLight);
}
// ── History grouping helpers ──
function getTimeGroup(utcIso) {
    const visited = new Date(utcIso);
    const now = new Date();
    const diffMs = now.getTime() - visited.getTime();
    const diffHours = diffMs / (1000 * 60 * 60);
    const diffDays = diffMs / (1000 * 60 * 60 * 24);
    if (diffHours < 1)
        return 'Just now';
    if (diffHours < 3)
        return 'A few hours ago';
    if (diffHours < 24)
        return 'Earlier today';
    const yesterday = new Date(now);
    yesterday.setDate(yesterday.getDate() - 1);
    if (visited.toDateString() === yesterday.toDateString())
        return 'Yesterday';
    if (diffDays < 7)
        return `${Math.floor(diffDays)} days ago`;
    if (diffDays < 14)
        return 'Last week';
    if (diffDays < 30)
        return `${Math.floor(diffDays / 7)} weeks ago`;
    if (diffDays < 60)
        return 'Last month';
    return 'Older';
}
function formatTime(utcIso) {
    const d = new Date(utcIso);
    return d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
}
function domainFromUrl(url) {
    try {
        return new URL(url).hostname;
    }
    catch {
        return '';
    }
}
function escapeHtml(text) {
    const el = document.createElement('span');
    el.textContent = text;
    return el.innerHTML;
}
function renderHistory(entries, container, filter, onDelete) {
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
            if (e.target.closest('.h-delete'))
                return;
            window.location.href = entry.url;
        });
        const delBtn = row.querySelector('.h-delete');
        delBtn.addEventListener('click', (e) => {
            e.stopPropagation();
            onDelete(originalIdx);
        });
        fragment.appendChild(row);
    });
    container.appendChild(fragment);
}
function renderCookies(entries, container, filter, onDelete) {
    container.innerHTML = '';
    const filtered = filter
        ? entries.filter((c) => c.domain.toLowerCase().includes(filter) || c.name.toLowerCase().includes(filter))
        : entries;
    if (filtered.length === 0) {
        container.innerHTML = `<div class="cookie-empty">${filter ? 'No matching cookies.' : 'No cookies stored.'}</div>`;
        return;
    }
    // Group by domain
    const groups = new Map();
    for (const c of filtered) {
        const key = c.domain.replace(/^\./, '');
        if (!groups.has(key))
            groups.set(key, []);
        groups.get(key).push(c);
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
            const flags = [];
            if (cookie.secure)
                flags.push('Secure');
            if (cookie.httpOnly)
                flags.push('HttpOnly');
            if (cookie.session)
                flags.push('Session');
            row.innerHTML = `
        <span class="c-name" title="${escapeHtml(cookie.name)}">${escapeHtml(cookie.name)}</span>
        <span class="c-domain">${escapeHtml(cookie.domain)}</span>
        <span class="c-flags">${flags.map((f) => `<span>${f}</span>`).join('')}</span>
        <button class="c-delete" data-idx="${cookie.index}" title="Remove">✕</button>
      `;
            const delBtn = row.querySelector('.c-delete');
            delBtn.addEventListener('click', (e) => {
                e.stopPropagation();
                onDelete(cookie.index);
            });
            fragment.appendChild(row);
        }
    }
    container.appendChild(fragment);
}
function renderCookieLoading(container) {
    container.innerHTML = '<div class="cookie-empty">Loading cookies…</div>';
}
function renderProtectionDiagnostics(entries, list, summary, siteFilter, categoryFilter) {
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
        }
        else if (categoryFilter.startsWith('category:')) {
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
async function initializeSettingsPage() {
    const { settings: bridge, history: historyBridge, cookies: cookieBridge, protection: protectionBridge, } = await connectBridge();
    // Qt 6 QWebChannel methods return Promises — must be awaited.
    let settings = bridge ? JSON.parse(await bridge.getSettingsJson()) : defaultSettings();
    async function refreshSettingsState() {
        if (!bridge) {
            return;
        }
        settings = JSON.parse(await bridge.getSettingsJson());
        applyState(settings);
    }
    applyState(settings);
    if (bridge) {
        bridge.settingsChanged.connect((json) => {
            settings = JSON.parse(json);
            applyState(settings);
        });
    }
    document.addEventListener('click', (event) => {
        const toggle = event.target.closest('.toggle[data-setting-path]');
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
        const row = event.target.closest('.radio-row[data-setting-path]');
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
            applyTheme(value);
        }
        bridge?.updateSetting(path, value);
    });
    document.querySelectorAll('select[data-setting-path]').forEach((select) => {
        select.addEventListener('change', () => {
            const path = select.dataset.settingPath;
            if (!path) {
                return;
            }
            bridge?.updateSetting(path, coerceValue(select.value));
        });
    });
    document.querySelectorAll('button[data-action]').forEach((button) => {
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
            }
            else if (action === 'resetSettings') {
                if (!(await bridge.resetToDefaults())) {
                    return;
                }
            }
            else if (action === 'clearBrowsingData') {
                bridge.requestClearBrowsingData();
            }
            else if (action === 'clearProtectionDiagnostics') {
                protectionBridge?.clear();
                return;
            }
            await refreshSettingsState();
        });
    });
    document.addEventListener('change', async (event) => {
        const select = event.target.closest('.site-permission-select');
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
        const button = event.target.closest('.site-permission-remove');
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
    document.querySelectorAll('form[data-site-permission-form]').forEach((form) => {
        form.addEventListener('submit', async (event) => {
            event.preventDefault();
            if (!bridge) {
                return;
            }
            const permissionType = form.dataset.sitePermissionForm;
            const originInput = form.querySelector('input[name="origin"]');
            const policySelect = form.querySelector('select[name="policy"]');
            if (!permissionType || !originInput || !policySelect) {
                return;
            }
            const rawOrigin = originInput.value.trim();
            try {
                const parsed = new URL(rawOrigin);
                if (!parsed.protocol || !parsed.hostname) {
                    throw new Error('Invalid origin');
                }
            }
            catch {
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
    const historySearch = document.getElementById('historySearch');
    const historyClearRange = document.getElementById('historyClearRange');
    let historyEntries = [];
    let historyFilter = '';
    async function refreshHistory() {
        if (!historyBridge || !historyList)
            return;
        historyEntries = JSON.parse(await historyBridge.getHistoryJson());
        renderHistory(historyEntries, historyList, historyFilter, (idx) => {
            historyBridge.deleteEntry(idx);
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
                renderHistory(historyEntries, historyList, historyFilter, (idx) => {
                    historyBridge?.deleteEntry(idx);
                });
            }
        });
    }
    if (historyClearRange) {
        historyClearRange.addEventListener('change', () => {
            const range = historyClearRange.value;
            if (!range || !historyBridge)
                return;
            historyBridge.clearByAge(range);
            historyClearRange.selectedIndex = 0;
        });
    }
    // ── Cookie wiring ──
    const cookieList = document.getElementById('cookieList');
    const cookieSearch = document.getElementById('cookieSearch');
    const cookieClearRange = document.getElementById('cookieClearRange');
    const protectionList = document.getElementById('protectionDiagnosticsList');
    const protectionSummary = document.getElementById('protectionSummary');
    const protectionSiteFilter = document.getElementById('protectionSiteFilter');
    const protectionCategoryFilter = document.getElementById('protectionCategoryFilter');
    let cookieEntries = [];
    let cookieFilter = '';
    let cookieRefreshToken = 0;
    let protectionEntries = [];
    let protectionSiteQuery = '';
    let protectionCategoryQuery = 'all';
    async function refreshCookies(forceReload = false) {
        if (!cookieBridge || !cookieList)
            return;
        if (forceReload) {
            cookieRefreshToken += 1;
            renderCookieLoading(cookieList);
            cookieBridge.reload();
            return;
        }
        const refreshToken = ++cookieRefreshToken;
        cookieEntries = JSON.parse(await cookieBridge.getCookiesJson());
        if (refreshToken !== cookieRefreshToken) {
            return;
        }
        renderCookies(cookieEntries, cookieList, cookieFilter, (idx) => {
            cookieBridge.deleteByIndex(idx);
        });
    }
    window.GhostSettingsBridge.refreshCookies = refreshCookies;
    function refreshProtectionCategoryOptions() {
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
    async function refreshProtection() {
        if (!protectionBridge || !protectionList)
            return;
        protectionEntries = JSON.parse(await protectionBridge.getEventsJson());
        refreshProtectionCategoryOptions();
        renderProtectionDiagnostics(protectionEntries, protectionList, protectionSummary, protectionSiteQuery, protectionCategoryQuery);
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
                renderProtectionDiagnostics(protectionEntries, protectionList, protectionSummary, protectionSiteQuery, protectionCategoryQuery);
            }
        });
    }
    if (protectionCategoryFilter) {
        protectionCategoryFilter.addEventListener('change', () => {
            protectionCategoryQuery = protectionCategoryFilter.value;
            if (protectionList) {
                renderProtectionDiagnostics(protectionEntries, protectionList, protectionSummary, protectionSiteQuery, protectionCategoryQuery);
            }
        });
    }
    if (cookieSearch) {
        cookieSearch.addEventListener('input', () => {
            cookieFilter = cookieSearch.value.toLowerCase();
            if (cookieList) {
                renderCookies(cookieEntries, cookieList, cookieFilter, (idx) => {
                    cookieBridge?.deleteByIndex(idx);
                });
            }
        });
    }
    if (cookieClearRange) {
        cookieClearRange.addEventListener('change', () => {
            const range = cookieClearRange.value;
            if (!range || !cookieBridge)
                return;
            cookieBridge.clearByAge(range);
            cookieClearRange.selectedIndex = 0;
        });
    }
    // Eagerly populate as soon as bridge is ready, regardless of which tab is visible
    refreshHistory();
    refreshCookies(true);
    refreshProtection();
}
window.GhostSettingsBridge = {
    initializeSettingsPage,
};

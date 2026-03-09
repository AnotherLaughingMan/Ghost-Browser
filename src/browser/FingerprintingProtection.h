#pragma once

class QWebEngineProfile;
class QString;

QString fingerprintingProtectionScriptSource();
void setFingerprintingProtectionEnabled(QWebEngineProfile *profile, bool enabled);
#include "browser/FingerprintingProtection.h"

#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>

namespace {

constexpr auto kFingerprintingScriptName = "GhostFingerprintingProtection";

QWebEngineScript buildFingerprintingProtectionScript()
{
    QWebEngineScript script;
    script.setName(QString::fromLatin1(kFingerprintingScriptName));
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setRunsOnSubFrames(true);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setSourceCode(fingerprintingProtectionScriptSource());
    return script;
}

} // namespace

QString fingerprintingProtectionScriptSource()
{
    return QStringLiteral(R"JS((() => {
  if (window.__ghostFingerprintingProtectionApplied) {
    return;
  }

  Object.defineProperty(window, '__ghostFingerprintingProtectionApplied', {
    value: true,
    configurable: false,
    enumerable: false,
    writable: false,
  });

  const defineGetter = (target, property, getter) => {
    if (!target || !(property in target)) {
      return;
    }

    try {
      Object.defineProperty(target, property, {
        configurable: true,
        enumerable: false,
        get: getter,
      });
    } catch {
    }
  };

  defineGetter(Navigator.prototype, 'hardwareConcurrency', () => 4);
  defineGetter(Navigator.prototype, 'deviceMemory', () => 8);
  defineGetter(Navigator.prototype, 'getBattery', () => undefined);

  const patchWebGL = (contextType) => {
    if (!contextType || !contextType.prototype || contextType.prototype.__ghostFingerprintingPatched) {
      return;
    }

    const originalGetParameter = contextType.prototype.getParameter;
    if (typeof originalGetParameter !== 'function') {
      return;
    }

    Object.defineProperty(contextType.prototype, '__ghostFingerprintingPatched', {
      value: true,
      configurable: false,
      enumerable: false,
      writable: false,
    });

    contextType.prototype.getParameter = function(parameter) {
      switch (parameter) {
        case 7936:
        case 37445:
          return 'Ghost Browser';
        case 7937:
        case 37446:
          return 'Standardized WebGL Renderer';
        default:
          return originalGetParameter.call(this, parameter);
      }
    };
  };

  patchWebGL(window.WebGLRenderingContext);
  patchWebGL(window.WebGL2RenderingContext);
})();)JS");
}

void setFingerprintingProtectionEnabled(QWebEngineProfile *profile, bool enabled)
{
    if (!profile)
        return;

    auto *collection = profile->scripts();
  const QList<QWebEngineScript> existingScripts = collection->toList();
  for (const QWebEngineScript &script : existingScripts) {
    if (script.name() == QLatin1String(kFingerprintingScriptName))
      collection->remove(script);
  }

    if (enabled)
        collection->insert(buildFingerprintingProtectionScript());
}
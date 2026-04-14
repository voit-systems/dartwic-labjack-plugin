import { createModuleUiPlugin, moduleUiPluginMeta } from "./index.jsx";

(function registerModuleUiPlugin() {
    const globalRegistry = (window.__dartwicPluginRegistry__ = window.__dartwicPluginRegistry__ || {});

    globalRegistry[moduleUiPluginMeta.moduleName] = {
        createPlugin: createModuleUiPlugin
    };
})();

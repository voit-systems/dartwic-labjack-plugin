
import path from "node:path";
import { fileURLToPath } from "node:url";
import { cp, mkdir, readFile, writeFile } from "node:fs/promises";
import esbuild from "esbuild";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const buildConfigurationPath = path.resolve(__dirname, "build-configuration.json");
const interfaceRoot = path.resolve(__dirname, "interface");
const runtimeEntryPath = path.resolve(interfaceRoot, "src", "runtime-entry.jsx");
const sourceManifestPath = path.resolve(__dirname, "plugin.json");
const buildConfiguration = JSON.parse(await readFile(buildConfigurationPath, "utf8"));
const sourceManifest = JSON.parse(await readFile(sourceManifestPath, "utf8"));
const pluginId = String(sourceManifest.id ?? "").trim();
const enginePluginsApiVersion = Number.parseInt(String(sourceManifest.engine_plugins_api_version ?? "").trim(), 10);
const interfacePluginsApiVersion = Number.parseInt(String(sourceManifest.interface_plugins_api_version ?? "").trim(), 10);
const releasePluginDir = path.resolve(__dirname, "plugin", "interface", pluginId);
const releaseUiDir = path.resolve(releasePluginDir, "ui");
const releaseManifestPath = path.resolve(releasePluginDir, "plugin.json");
const devInstallPluginDir = path.resolve(__dirname, "..", "interface", "plugins", pluginId);
const shouldInstallDev = process.argv.includes("--install-dev");
const shouldCopyPlugin = buildConfiguration.copy_plugin !== false;

if (
    !pluginId ||
    !Number.isInteger(enginePluginsApiVersion) ||
    !Number.isInteger(interfacePluginsApiVersion) ||
    sourceManifest.contains_interface_plugin !== true
) {
    throw new Error("plugin.json must include id, integer engine/interface plugin API versions, and contains_interface_plugin=true.");
}

await mkdir(releaseUiDir, { recursive: true });

async function ensureDir(dirPath) {
    await mkdir(dirPath, { recursive: true });
}

async function copyDirectory(sourceDir, targetDir) {
    await ensureDir(path.dirname(targetDir));
    await cp(sourceDir, targetDir, { recursive: true, force: true });
}

const buildResult = await esbuild.build({
    entryPoints: [runtimeEntryPath],
    bundle: true,
    write: false,
    format: "iife",
    platform: "browser",
    jsx: "transform",
    jsxFactory: "React.createElement",
    jsxFragment: "React.Fragment",
    target: "es2020"
});

const runtimeCode = buildResult.outputFiles[0].text;
await cp(sourceManifestPath, releaseManifestPath, { force: true });
await writeFile(path.resolve(releaseUiDir, "index.js"), runtimeCode, "utf8");

const localTargets = shouldInstallDev ? [releasePluginDir, devInstallPluginDir] : [releasePluginDir];
const externalPluginTargets = shouldCopyPlugin
    ? [
        buildConfiguration.interface_dir
            ? path.resolve(buildConfiguration.interface_dir, "plugins", pluginId)
            : null,
    ].filter(Boolean)
    : [];

for (const target of localTargets.slice(1)) {
    await copyDirectory(releasePluginDir, target);
}

for (const target of externalPluginTargets) {
    await copyDirectory(releasePluginDir, target);
}

console.log(`Built interface plugin for ${pluginId}.`);
for (const target of localTargets) {
    console.log(`- ${target}`);
}
for (const target of externalPluginTargets) {
    console.log(`- copied interface plugin to ${target}`);
}

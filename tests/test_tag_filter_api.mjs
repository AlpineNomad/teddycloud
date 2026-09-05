// Runs only against a fresh, temporary teddyCloud instance with cloud access off.
import assert from "node:assert/strict";
import { mkdtemp, mkdir, cp, writeFile, readFile, access } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { spawn, execFileSync } from "node:child_process";
import { setTimeout as delay } from "node:timers/promises";

const root = resolve(import.meta.dirname, "..");
const base = await mkdtemp(join(tmpdir(), "teddycloud-filter-api-"));
for (const dir of ["config", "certs/server", "certs/client", "data/content/default", "data/library", "data/cache"])
    await mkdir(join(base, dir), { recursive: true });
await cp(join(root, "contrib/data/www"), join(base, "data/www"), { recursive: true });
await cp(join(root, "teddycloud_web/dist"), join(base, "data/www/web"), { recursive: true });
// Disposable local certificate: never load certificates from a real box.
const cert = join(base, "certs/server/ca-root.pem");
const key = join(base, "certs/server/ca-key.pem");
execFileSync("openssl", ["req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
    "-subj", "/CN=localhost", "-keyout", key, "-out", cert], { stdio: "ignore" });
await cp(cert, join(base, "certs/server/teddy-cert.pem"));
await cp(key, join(base, "certs/server/teddy-key.pem"));
execFileSync("openssl", ["x509", "-in", cert, "-outform", "DER", "-out", join(base, "certs/server/ca.der")]);
await cp(join(base, "certs/server/ca.der"), join(base, "certs/client/ca.der"));
await cp(join(base, "certs/server/ca.der"), join(base, "certs/client/client.der"));
execFileSync("openssl", ["pkey", "-in", key, "-outform", "DER", "-out", join(base, "certs/client/private.der")]);
await writeFile(join(base, "config/config.ini"), [
    "core.server.http_port=18080", "core.server.https_web_port=18443", "core.server.https_api_port=18444",
    "core.server.bind_ip=0.0.0.0", "core.host_url=http://localhost:18080", "cloud.enabled=false",
    "core.tonies_json_auto_update=false", "tonie_json.cache_preload=false", "mqtt.enabled=false",
    "mqtt_server.enabled=false", "log.color=false", "core.settings_level=1",
].join("\n") + "\n");
let logs = "";
const server = spawn(join(root, "bin/teddycloud"), ["--base_path", base], { cwd: base });
server.stdout.on("data", data => { logs += data; });
server.stderr.on("data", data => { logs += data; });
const api = "http://127.0.0.1:18080";
const request = async (path, value) => fetch(api + path, value === undefined ? {} : {
    method: "POST", body: value, headers: { "Content-Type": "text/plain" },
});
const setting = async name => (await request("/api/settings/get/" + name)).text();
const set = async (name, value) => request("/api/settings/set/" + name, value);
const exists = async path => access(path).then(() => true, () => false);
try {
    let ready = false;
    for (let i = 0; i < 100; i++) {
        try { ready = (await request("/api/settings/getIndex")).ok; } catch {}
        if (ready) break;
        if (server.exitCode !== null) throw new Error("Server exited: " + logs);
        await delay(100);
    }
    assert(ready, "Server startup timed out: " + logs);
    const index = await (await request("/api/settings/getIndex")).json();
    assert(index.options.some(option => option.ID === "core.tag_filter.content_id" || option.iD === "core.tag_filter.content_id"));
    assert.equal(await setting("core.tag_filter.enabled"), "true");
    assert.equal(await setting("core.tag_filter.content_id"), "FFFFFFFF");
    for (const invalid of ["FFFFFF", "GGGGGGGG", "FFFFFFFF0", ""]) {
        assert.equal((await set("core.tag_filter.content_id", invalid)).status, 400);
        assert.equal(await setting("core.tag_filter.content_id"), "FFFFFFFF");
    }
    const blockedJson = join(base, "data/content/default/78563412/FFFFFFFF.json");
    assert.equal((await request("/v1/claim/78563412FFFFFFFF")).status, 404);
    assert.equal((await request("/v1/content/78563412FFFFFFFF")).status, 404);
    assert.equal(await exists(blockedJson), false);
    assert.equal((await request("/v1/claim/FFFFFFFF000304E0")).status, 200);
    assert(await exists(join(base, "data/content/default/FFFFFFFF/000304E0.json")));
    assert.equal((await set("core.tag_filter.enabled", "false")).status, 200);
    assert.equal((await request("/v1/claim/78563412FFFFFFFF")).status, 200);
    assert(await exists(blockedJson), "Old save_content_json workaround must be removed");
    assert.equal((await set("core.tag_filter.content_id", "89abcdef")).status, 200);
    assert.equal((await set("core.tag_filter.enabled", "true")).status, 200);
    assert.equal((await request("/v1/claim/1234567889ABCDEF")).status, 404);
    assert.equal((await request("/api/triggerWriteConfig")).status, 200);
    assert.match(await readFile(join(base, "config/config.ini"), "utf8"), /core.tag_filter.content_id=89abcdef/);
    assert.equal((await request("/api/settings/reset/core.tag_filter.content_id", "")).status, 200);
    assert.equal(await setting("core.tag_filter.content_id"), "FFFFFFFF");
    console.log("PASS: real HTTP API, validation, blocking, allowed tags, disabling, saving and resetting");
    assert(!logs.includes("runtime error:"), logs);
    assert(!logs.includes("ERROR: AddressSanitizer"), logs);
    if (process.argv.includes("--serve")) {
        console.log("Preview ready: http://localhost:18080/web/settings");
        await new Promise(resolve => {
            process.once("SIGTERM", resolve);
            process.once("SIGINT", resolve);
        });
    }
} catch (error) {
    console.error(logs.slice(-10000));
    throw error;
} finally {
    server.kill("SIGTERM");
    await writeFile(join(base, "test-server.log"), logs);
}

#!/usr/bin/env node
/** Python-free equivalent of package_release.py, with both native test suites. */
import { execFileSync } from 'node:child_process';
import { copyFileSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { homedir } from 'node:os';
import { basename, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(fileURLToPath(new URL('..', import.meta.url)));
const tools = join(process.env.ANDROID_HOME || join(homedir(), 'Library/Android/sdk'), 'build-tools/37.0.0');
const certificate = 'f3a3227d5836f3906b033b82e894b493f82b779fda1d3972b30ebfd7e2cff3f0';
function capture(command, ...args) {
  return execFileSync(command, args, { cwd: root, encoding: 'utf8', maxBuffer: 32 * 1024 * 1024 });
}
function run(command, ...args) {
  execFileSync(command, args, { cwd: root, stdio: 'inherit' });
}
function requireCondition(condition, message) {
  if (!condition) throw new Error(message);
}
function member(apk, name) {
  return execFileSync('unzip', ['-p', apk, name], { maxBuffer: 32 * 1024 * 1024 });
}
function validate(apk) {
  const badging = capture(join(tools, 'aapt2'), 'dump', 'badging', apk);
  requireCondition(badging.includes("package: name='dev.ryuya.legacygametools'") &&
    badging.includes("targetSdkVersion:'37'") && !badging.includes('application-debuggable'), 'Unexpected APK or variant');
  const manifest = capture(join(tools, 'aapt2'), 'dump', 'xmltree', apk, '--file', 'AndroidManifest.xml');
  requireCondition(!/testactivity|probeactivity|fixtureactivity|instrumentation/i.test(manifest), 'Test component in release');
  const prefix = 'META-INF/xposed/';
  const props = member(apk, prefix + 'module.prop').toString();
  requireCondition(props.includes('minApiVersion=102') && props.includes('targetApiVersion=102'), 'API 102 metadata missing');
  requireCondition(member(apk, prefix + 'java_init.list').toString().trim() === 'dev.ryuya.legacygametools.HookEntry', 'Unexpected Java entry');
  requireCondition(member(apk, prefix + 'native_init.list').toString().trim() === 'liblegacycompat.so', 'Unexpected native entry');
  const scopes = member(apk, prefix + 'scope.list').toString().trim().split(/\s+/).sort();
  requireCondition(scopes.join(',') === ['com.bushiroad.d4djddedition', 'com.sega.pjsekai.re'].sort().join(','), 'Unexpected scope');
  const native = member(apk, 'lib/arm64-v8a/liblegacycompat.so');
  requireCondition(native.subarray(0, 6).equals(Buffer.from([0x7f, 0x45, 0x4c, 0x46, 2, 1])) && native.readUInt16LE(18) === 183, 'Expected arm64 ELF');
  const offset = Number(native.readBigUInt64LE(32));
  const stride = native.readUInt16LE(54), count = native.readUInt16LE(56);
  for (let i = 0; i < count; i++) {
    const header = offset + stride * i;
    if (native.readUInt32LE(header) === 1)
      requireCondition(native.readBigUInt64LE(header + 48) >= 16384n, 'Native ELF requires 16KiB alignment');
  }
  for (const name of ['APP_LICENSE.txt', 'THIRD_PARTY_NOTICES.txt', 'Apache-2.0.txt', 'LIBCXX-NOTICE.txt'])
    requireCondition(member(apk, 'assets/' + name).toString().trim().length > 0, 'Missing license: ' + name);
  run(join(tools, 'zipalign'), '-c', '-P', '16', '4', apk);
}

const args = process.argv.slice(2);
requireCondition(args.length === 0 || (args.length === 2 && args[0] === '--source-ref'), 'Usage: node tools/package_release.mjs [--source-ref HEAD]');
requireCondition(!capture('git', 'status', '--porcelain').trim(), 'Commit release sources first');
const revision = capture('git', 'rev-parse', args[1] || 'HEAD').trim();
requireCondition(revision === capture('git', 'rev-parse', 'HEAD').trim(), 'Build checkout must match source revision');
requireCondition(process.env.ONEUI_SIGNING_PROPERTIES, 'Set ONEUI_SIGNING_PROPERTIES to the private signing configuration');
run(process.execPath, 'tools/check_native.mjs');
run(join(root, 'gradlew'), ':app:assembleRelease', ':app:lintRelease');
const output = join(root, 'app/build/outputs/apk/release');
const metadata = JSON.parse(readFileSync(join(output, 'output-metadata.json')));
requireCondition(metadata.variantName === 'release' && metadata.elements.length === 1, 'Expected one release variant');
const item = metadata.elements[0], apk = join(output, item.outputFile);
validate(apk);
const signature = capture(join(tools, 'apksigner'), 'verify', '--verbose', '--print-certs', apk);
requireCondition(signature.includes('certificate SHA-256 digest: ' + certificate), 'Release signing certificate mismatch');
const name = 'LegacyGameTools-' + item.versionName;
const destination = join(root, 'artifacts/release', item.versionName);
mkdirSync(destination, { recursive: true });
const target = join(destination, name + '-release.apk');
copyFileSync(apk, target);
const source = join(destination, name + '-source.zip');
run('git', 'archive', '--format=zip', '--prefix=' + name + '/', '--output=' + source, revision);
writeFileSync(join(destination, 'SHA256SUMS.txt'), [target, source].map(file =>
  createHash('sha256').update(readFileSync(file)).digest('hex') + '  ' + basename(file) + '\n').join(''));
writeFileSync(join(destination, 'verification.txt'), 'Source revision: ' + revision + '\nVariant: release\n' + signature);
console.log('Verified release: ' + target);

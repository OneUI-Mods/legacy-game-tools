#!/usr/bin/env node
/** Run the native policy/tracker tests without an Android device or game binary. */
import { mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';

const root = resolve(fileURLToPath(new URL('..', import.meta.url)));
const folder = mkdtempSync(join(tmpdir(), 'legacy-native-'));
try {
  for (const name of ['overflow_tracker_test', 'sekai_bpm_policy_test', 'sekai_life_policy_test']) {
    const binary = join(folder, name);
    for (const [command, args] of [
      [process.env.CXX || 'clang++', ['-std=c++20', '-Wall', '-Wextra', '-Werror', join(root, 'tests', name + '.cpp'), '-o', binary]],
      [binary, []],
    ]) {
      const result = spawnSync(command, args, { cwd: root, stdio: 'inherit' });
      if (result.error) throw result.error;
      if (result.status !== 0) throw new Error(`${name} failed: ${result.status}`);
    }
    console.log(`${name}: passed`);
  }
} finally {
  rmSync(folder, { recursive: true, force: true });
}

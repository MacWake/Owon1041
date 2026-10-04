import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import test from 'node:test';

const releaseScript = resolve(import.meta.dirname, 'release.mjs');

function run(cwd, command, args, env = {}) {
  return execFileSync(command, args, {
    cwd,
    encoding: 'utf8',
    env: { ...process.env, ...env },
  }).trim();
}

function commit(cwd, message) {
  writeFileSync(join(cwd, 'change.txt'), `${message}\n`);
  run(cwd, 'git', ['add', 'change.txt']);
  run(cwd, 'git', ['commit', '-m', message]);
}

function output(cwd, env = {}) {
  return Object.fromEntries(run(cwd, 'node', [releaseScript, 'plan'], {
    FORGEJO_RUN_NUMBER: '7',
    ...env,
  }).split('\n').map(line => line.split('=', 2)));
}

test('release plan follows Conventional Commit bumps and supports a retry tag', () => {
  const cwd = mkdtempSync(join(tmpdir(), 'owon-release-test-'));
  try {
    run(cwd, 'git', ['init', '-q']);
    run(cwd, 'git', ['config', 'user.name', 'Test']);
    run(cwd, 'git', ['config', 'user.email', 'test@example.com']);
    commit(cwd, 'chore: initial commit');
    run(cwd, 'git', ['tag', 'v0.9.6']);

    commit(cwd, 'build: update toolchain');
    assert.deepEqual(output(cwd), { release: 'false', version: '0.9.6-build7', tag: '' });

    commit(cwd, 'fix: correct timeout');
    assert.deepEqual(output(cwd), { release: 'true', version: '0.9.7', tag: 'v0.9.7' });

    commit(cwd, 'feat: add logging');
    assert.deepEqual(output(cwd), { release: 'true', version: '0.10.0', tag: 'v0.10.0' });

    commit(cwd, 'feat!: change protocol');
    assert.deepEqual(output(cwd), { release: 'true', version: '1.0.0', tag: 'v1.0.0' });

    run(cwd, 'git', ['tag', 'v1.0.0']);
    assert.deepEqual(output(cwd, { RETRY_RELEASE_TAG: 'v1.0.0' }), {
      release: 'true', version: '1.0.0', tag: 'v1.0.0',
    });
  } finally {
    rmSync(cwd, { recursive: true, force: true });
  }
});

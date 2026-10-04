#!/usr/bin/env node
import { execFileSync } from 'node:child_process';
import { appendFileSync, readFileSync, readdirSync, statSync } from 'node:fs';
import { resolve } from 'node:path';

function git(...args) {
  return execFileSync('git', args, { encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] }).trim();
}

function plan() {
  const retryTag = process.env.RETRY_RELEASE_TAG || '';
  if (retryTag && !/^v\d+\.\d+\.\d+$/.test(retryTag)) {
    throw new Error('Retry tag must be a vMAJOR.MINOR.PATCH tag');
  }
  if (retryTag && git('rev-list', '-n', '1', retryTag) !== git('rev-parse', 'HEAD')) {
    throw new Error('Retry tag must point to the checked-out commit');
  }
  const tags = git('tag', '--merged', 'HEAD', '--list', 'v[0-9]*').split('\n')
    .map(tag => ({ tag, match: /^v(\d+)\.(\d+)\.(\d+)$/.exec(tag) }))
    .filter(item => item.match && item.tag !== retryTag)
    .sort((a, b) => {
      for (let i = 1; i <= 3; i++) {
        const difference = Number(b.match[i]) - Number(a.match[i]);
        if (difference) return difference;
      }
      return 0;
    });
  const previous = tags[0]?.tag;
  const commits = git('log', '--format=%B%x00', ...(previous ? [`${previous}..HEAD`] : ['HEAD']))
    .split('\0').map(message => message.trim()).filter(Boolean);
  let bump = 0;
  for (const message of commits) {
    const header = message.split('\n', 1)[0];
    if (/^[a-z][\w-]*(?:\([^)]+\))?!:/.test(header) || /^BREAKING[ -]CHANGE:/m.test(message)) {
      bump = 3;
    } else if (/^feat(?:\([^)]+\))?:/.test(header)) {
      bump = Math.max(bump, 2);
    } else if (/^(?:fix|perf)(?:\([^)]+\))?:/.test(header)) {
      bump = Math.max(bump, 1);
    }
  }
  const versionParts = previous ? previous.slice(1).split('.').map(Number) : [0, 0, 0];
  if (bump === 3) {
    versionParts[0]++;
    versionParts[1] = versionParts[2] = 0;
  } else if (bump === 2) {
    versionParts[1]++;
    versionParts[2] = 0;
  } else if (bump === 1) {
    versionParts[2]++;
  }
  const next = versionParts.join('.');
  const release = bump > 0 || Boolean(retryTag);
  const runNumber = process.env.FORGEJO_RUN_NUMBER || process.env.GITHUB_RUN_NUMBER || 'local';
  return {
    release,
    version: retryTag ? retryTag.slice(1) : release ? next : `${next}-build${runNumber}`,
    tag: retryTag || (release ? `v${next}` : ''),
    previous,
    commits,
  };
}

function notes(result) {
  const subjects = result.commits.map(message => message.split('\n', 1)[0]);
  return `## Changes since ${result.previous || 'the beginning'}\n\n${subjects.map(subject => `- ${subject}`).join('\n')}\n`;
}

async function api(url, token, options = {}) {
  const response = await fetch(url, {
    ...options,
    headers: {
      Accept: 'application/json',
      Authorization: `token ${token}`,
      ...options.headers,
    },
  });
  if (!response.ok) throw new Error(`API request failed (${response.status}) at ${new URL(url).pathname}`);
  return response.status === 204 ? null : response.json();
}

async function ensureRelease(apiBase, ownerRepo, token, result, gh = false) {
  const base = `${apiBase}/repos/${ownerRepo}/releases`;
  let release;
  const existing = await fetch(`${base}/tags/${result.tag}`, {
    headers: { Accept: 'application/json', Authorization: `token ${token}` },
  });
  if (existing.status === 200) {
    release = await existing.json();
  } else if (existing.status === 404) {
    release = await api(base, token, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        tag_name: result.tag,
        target_commitish: 'main',
        name: result.tag,
        body: notes(result),
        draft: false,
        prerelease: false,
      }),
    });
  } else {
    throw new Error(`Release lookup failed (${existing.status})`);
  }

  const assetDir = resolve('dist/release');
  const files = releaseFiles(assetDir);
  for (const file of files) {
    const path = resolve(assetDir, file);
    const present = (release.assets || []).find(asset => asset.name === file);
    if (present) {
      if (present.size !== statSync(path).size) throw new Error(`Existing asset size differs: ${file}`);
      continue;
    }
    const uploadBase = gh ? release.upload_url.split('{')[0] : `${base}/${release.id}/assets`;
    const separator = uploadBase.includes('?') ? '&' : '?';
    await api(`${uploadBase}${separator}name=${encodeURIComponent(file)}`, token, {
      method: 'POST',
      headers: { 'Content-Type': 'application/octet-stream' },
      body: readFileSync(path),
    });
  }
  return release;
}

function releaseFiles(assetDir) {
  const files = readdirSync(assetDir).filter(file => /\.(?:deb|dmg)$/.test(file)).sort();
  const expected = ['debian-trixie.deb', 'ubuntu22.deb', 'macos-amd64.dmg', 'macos-arm64.dmg'];
  if (files.length !== expected.length || expected.some(suffix => !files.some(file => file.endsWith(suffix)))) {
    throw new Error('Release must contain Ubuntu, Debian, macOS amd64, and macOS arm64 packages');
  }
  return files;
}

async function publish() {
  const result = plan();
  if (!result.release || result.version !== process.env.RELEASE_VERSION || result.tag !== process.env.RELEASE_TAG) {
    throw new Error('Release plan changed between build and publish');
  }
  const ghToken = process.env.GH_TOKEN;
  const forgeToken = process.env.FORGEJO_TOKEN;
  const forgeUrl = process.env.FORGEJO_SERVER_URL;
  const forgeRepo = process.env.FORGEJO_REPOSITORY;
  const ghRepo = process.env.GITHUB_REPOSITORY;
  if (![ghToken, forgeToken, forgeUrl, forgeRepo, ghRepo].every(Boolean)) {
    throw new Error('Missing GitHub token or Forgejo release context');
  }
  const signingSettings = [
    'APPLE_CERTIFICATE_BASE64', 'APPLE_CERTIFICATE_PASSWORD', 'APPLE_ID',
    'APPLE_ID_PASSWORD', 'APPLE_SIGNING_ID', 'APPLE_TEAM_ID',
  ];
  const missingSigningSettings = signingSettings.filter(name => !process.env[name]);
  if (missingSigningSettings.length) {
    throw new Error(`Missing macOS signing settings: ${missingSigningSettings.join(', ')}`);
  }
  releaseFiles(resolve('dist/release'));

  git('config', 'user.name', 'Forgejo Actions');
  git('config', 'user.email', 'forgejo-actions@users.noreply.github.com');
  if (git('tag', '--list', result.tag) !== result.tag) {
    git('tag', '-a', result.tag, '-m', `Release ${result.tag}`);
  }
  const askpass = resolve('scripts/github-askpass.sh');
  const githubRemote = `https://github.com/${ghRepo}.git`;
  execFileSync('git', ['-c', 'credential.helper=', 'push', '--atomic', githubRemote,
    'HEAD:refs/heads/main', `refs/tags/${result.tag}`], {
    stdio: 'inherit',
    env: { ...process.env, GIT_ASKPASS: askpass, GIT_TERMINAL_PROMPT: '0' },
  });
  git('push', 'origin', `refs/tags/${result.tag}`);

  await ensureRelease('https://api.github.com', ghRepo, ghToken, result, true);
  await ensureRelease(`${forgeUrl.replace(/\/$/, '')}/api/v1`, forgeRepo, forgeToken, result);
  process.stdout.write(`Published ${result.tag} to Forgejo and GitHub\n`);
}

const command = process.argv[2];
if (command === 'plan') {
  const result = plan();
  const values = `release=${result.release}\nversion=${result.version}\ntag=${result.tag}\n`;
  if (process.env.GITHUB_OUTPUT) appendFileSync(process.env.GITHUB_OUTPUT, values);
  process.stdout.write(values);
} else if (command === 'publish') {
  await publish();
} else {
  throw new Error('Usage: node scripts/release.mjs plan|publish');
}

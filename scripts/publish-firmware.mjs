import { put } from '@vercel/blob';
import { createHash } from 'node:crypto';
import { createReadStream } from 'node:fs';
import { readFile, stat } from 'node:fs/promises';
import { basename, resolve } from 'node:path';

const RELEASE_PREFIX = 'firmware/life-counter/releases';
const LATEST_PATH = 'firmware/life-counter/latest.json';

function requireEnv(name) {
  const value = process.env[name]?.trim();
  if (!value) throw new Error(`Missing required environment variable: ${name}`);
  return value;
}

async function sha256(path) {
  return createHash('sha256')
    .update(await readFile(path))
    .digest('hex');
}

async function main() {
  const filePath = resolve(requireEnv('FIRMWARE_FILE'));
  const version = requireEnv('FIRMWARE_VERSION');
  const sourceCommit = requireEnv('GITHUB_SHA');
  const repository = requireEnv('GITHUB_REPOSITORY');
  const runId = requireEnv('GITHUB_RUN_ID');

  if (!/^[0-9A-Za-z][0-9A-Za-z._-]*$/.test(version)) {
    throw new Error('Firmware version contains unsupported characters');
  }
  if (!filePath.endsWith('.merged.bin')) {
    throw new Error('Firmware must be an Arduino merged binary ending in .merged.bin');
  }

  const fileStat = await stat(filePath);
  if (!fileStat.isFile() || fileStat.size < 100_000) {
    throw new Error('Firmware file is missing or unexpectedly small');
  }

  const filename = `arcmind-life-counter-${version}.merged.bin`;
  const firmwarePath = `${RELEASE_PREFIX}/${version}/${filename}`;
  const publishedAt = new Date().toISOString();

  console.log(`Uploading ${basename(filePath)} as ${firmwarePath}`);
  const firmware = await put(firmwarePath, createReadStream(filePath), {
    access: 'private',
    addRandomSuffix: false,
    allowOverwrite: true,
    contentType: 'application/octet-stream',
    multipart: true,
  });

  const release = {
    product: 'ArcMind Life Counter',
    version,
    chipFamily: 'ESP32-S3',
    firmwareUrl: firmware.url,
    firmwarePathname: firmware.pathname,
    filename,
    size: fileStat.size,
    sha256: await sha256(filePath),
    publishedAt,
    access: 'private',
    sourceCommit,
    sourceUrl: `https://github.com/${repository}/commit/${sourceCommit}`,
    workflowRunUrl: `https://github.com/${repository}/actions/runs/${runId}`,
  };

  const metadata = await put(LATEST_PATH, JSON.stringify(release, null, 2), {
    access: 'public',
    addRandomSuffix: false,
    allowOverwrite: true,
    contentType: 'application/json',
  });

  console.log(`Published firmware ${firmware.url}`);
  console.log(`Updated release metadata ${metadata.url}`);
  console.log(JSON.stringify(release, null, 2));
}

main().catch((error) => {
  console.error('Firmware publication failed:', error instanceof Error ? error.message : error);
  process.exit(1);
});

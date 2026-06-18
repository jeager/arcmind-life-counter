import { createHash, randomBytes, randomUUID } from 'node:crypto';
import { existsSync } from 'node:fs';
import { readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { SerialPort } from 'serialport';
import { createClient } from '@supabase/supabase-js';

const DEVICE_KEY_HEX_LEN = 64;
const COMMAND_TIMEOUT_MS = 5000;

function parseArgs(argv) {
  const args = { port: '', notes: '' };
  for (let i = 2; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === '--port' && argv[i + 1]) {
      args.port = argv[++i];
    } else if (arg === '--notes' && argv[i + 1]) {
      args.notes = argv[++i];
    } else if (arg === '--help' || arg === '-h') {
      console.log(`Usage: node scripts/register-device.mjs --port /dev/cu.usbmodem101 [--notes "Friend name"]`);
      process.exit(0);
    }
  }
  return args;
}

async function loadEnvFile(path) {
  if (!existsSync(path)) return;
  const text = await readFile(path, 'utf8');
  for (const line of text.split('\n')) {
    const trimmed = line.trim();
    if (!trimmed || trimmed.startsWith('#')) continue;
    const eq = trimmed.indexOf('=');
    if (eq <= 0) continue;
    const key = trimmed.slice(0, eq).trim();
    let value = trimmed.slice(eq + 1).trim();
    if ((value.startsWith('"') && value.endsWith('"')) || (value.startsWith("'") && value.endsWith("'"))) {
      value = value.slice(1, -1);
    }
    if (process.env[key] == null || process.env[key] === '') {
      process.env[key] = value;
    }
  }
}

function normalizeMac(mac) {
  return mac.trim().toUpperCase().replace(/-/g, ':');
}

function sha256Hex(value) {
  return createHash('sha256').update(value, 'utf8').digest('hex');
}

function sleep(ms) {
  return new Promise((resolveSleep) => setTimeout(resolveSleep, ms));
}

async function sendCommand(port, command) {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      cleanup();
      reject(new Error(`Timed out waiting for response to ${command}`));
    }, COMMAND_TIMEOUT_MS);

    const onData = (chunk) => {
      const lines = chunk.toString('utf8').split(/\r?\n/);
      for (const line of lines) {
        const trimmed = line.trim();
        if (!trimmed) continue;
        cleanup();
        resolve(trimmed);
        return;
      }
    };

    const cleanup = () => {
      clearTimeout(timer);
      port.off('data', onData);
    };

    port.on('data', onData);
    port.write(`${command}\n`);
  });
}

async function detectPort(explicitPort) {
  if (explicitPort) return explicitPort;
  const ports = await SerialPort.list();
  const preferred = ports.find((entry) => /usbmodem/i.test(entry.path));
  if (preferred) return preferred.path;
  throw new Error('No usbmodem serial port found. Pass --port explicitly.');
}

async function main() {
  const args = parseArgs(process.argv);
  await loadEnvFile(resolve(process.cwd(), '.env'));
  await loadEnvFile(resolve(process.cwd(), '.env.local'));
  await loadEnvFile(resolve(process.cwd(), '../arcmind/.env.local'));

  const supabaseUrl = process.env.SUPABASE_URL?.trim();
  const supabaseKey = process.env.SUPABASE_SERVICE_ROLE_KEY?.trim();
  if (!supabaseUrl || !supabaseKey) {
    throw new Error('Missing SUPABASE_URL or SUPABASE_SERVICE_ROLE_KEY in .env/.env.local');
  }

  const portPath = await detectPort(args.port);
  const port = new SerialPort({ path: portPath, baudRate: 115200, autoOpen: false });

  await new Promise((resolveOpen, rejectOpen) => {
    port.open((error) => (error ? rejectOpen(error) : resolveOpen()));
  });

  try {
    await sleep(500);
    await sendCommand(port, 'ARC_PING').catch(() => null);
    const infoLine = await sendCommand(port, 'ARC_GET_INFO');
    const infoMatch = infoLine.match(/^ARC_INFO mac=([^ ]+) id=([^ ]*) provisioned=(yes|no)$/);
    if (!infoMatch) {
      throw new Error(`Unexpected device response: ${infoLine}`);
    }

    const macAddress = normalizeMac(infoMatch[1]);
    const provisioned = infoMatch[3] === 'yes';
    if (provisioned) {
      throw new Error(`Device ${macAddress} is already provisioned (${infoMatch[2]}).`);
    }

    const deviceId = randomUUID();
    const deviceKey = randomBytes(32).toString('hex');
    if (deviceKey.length !== DEVICE_KEY_HEX_LEN) {
      throw new Error('Generated device key has unexpected length');
    }

    console.log(`Provisioning ${macAddress} as ${deviceId} on ${portPath}...`);
    const provisionLine = await sendCommand(
      port,
      `ARC_PROVISION id=${deviceId} key=${deviceKey}`
    );
    if (provisionLine !== 'ARC_PROVISION_OK') {
      throw new Error(`Provision failed: ${provisionLine}`);
    }

    const supabase = createClient(supabaseUrl, supabaseKey);
    const notes = args.notes || process.env.LIFE_COUNTER_REGISTER_NOTES || null;
    const { error } = await supabase.from('life_counter_devices').insert({
      device_id: deviceId,
      mac_address: macAddress,
      device_key_hash: sha256Hex(deviceKey),
      enabled: true,
      notes,
    });
    if (error) {
      throw new Error(`Device provisioned on hardware but database insert failed: ${error.message}`);
    }

    console.log('Registered ArcMind Life Counter device:');
    console.log(`  device_id: ${deviceId}`);
    console.log(`  mac:       ${macAddress}`);
    if (notes) console.log(`  notes:     ${notes}`);
    console.log('The device should reboot automatically and start normally.');
  } finally {
    await new Promise((resolveClose) => port.close(() => resolveClose()));
  }
}

main().catch((error) => {
  console.error(error instanceof Error ? error.message : error);
  process.exit(1);
});

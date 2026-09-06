/**
 * injection.cjs — JavaScript bridge to flow-injection.dll via koffi FFI.
 *
 * The native DLL exposes a single C function:
 *   const char* flow_request(const char* json_input)
 *
 * All operations are JSON-in / JSON-out.  Because koffi calls are synchronous
 * and UI Automation work can block, every call is dispatched to a worker thread
 * via Node's built-in worker_threads so the Electron main thread is never
 * stalled.
 */
'use strict';
const path = require('node:path');
const fs = require('node:fs');
const { Worker } = require('node:worker_threads');

const root = (() => {
  try {
    const { app } = require('electron');
    return app.isPackaged
      ? path.resolve(path.dirname(app.getPath('exe')), '../..')
      : path.join(__dirname, '..');
  } catch { return path.join(__dirname, '..'); }
})();

const DLL_PATH = path.join(root, 'native', 'build', 'flow-injection.dll');

// ---------------------------------------------------------------------------
// Worker thread source (stringified, loaded inline to avoid extra file)
// ---------------------------------------------------------------------------
const WORKER_SRC = `
'use strict';
const { parentPort, workerData } = require('node:worker_threads');
const koffi = require('koffi');

const lib = koffi.load(workerData.dll);
const flow_request = lib.func('flow_request', 'str', ['str']);

parentPort.on('message', (msg) => {
  const { id, payload } = msg;
  try {
    const raw = flow_request(payload);
    parentPort.postMessage({ id, result: raw });
  } catch (e) {
    parentPort.postMessage({ id, error: e.message || 'NativeCallFailed' });
  }
});
`;

// ---------------------------------------------------------------------------
// Injection manager
// ---------------------------------------------------------------------------
let worker = null;
let seq = 0;
const pending = new Map();

function ensureWorker() {
  if (worker) return;
  if (!fs.existsSync(DLL_PATH)) {
    throw new Error(
      'Native injection DLL not found. Run: powershell scripts/build-native.ps1'
    );
  }
  worker = new Worker(WORKER_SRC, {
    eval: true,
    workerData: { dll: DLL_PATH },
  });
  worker.on('message', (msg) => {
    const cb = pending.get(msg.id);
    if (!cb) return;
    pending.delete(msg.id);
    if (msg.error) cb.reject(new Error(msg.error));
    else cb.resolve(msg.result);
  });
  worker.on('error', (err) => {
    for (const cb of pending.values()) cb.reject(err);
    pending.clear();
    worker = null;
  });
  worker.on('exit', () => {
    for (const cb of pending.values()) cb.reject(new Error('Worker exited'));
    pending.clear();
    worker = null;
  });
}

/**
 * Send a JSON request to the native DLL and return the parsed response.
 * Runs on a worker thread so the main thread is never blocked.
 */
function call(request) {
  ensureWorker();
  const id = ++seq;
  const payload = JSON.stringify(request);
  return new Promise((resolve, reject) => {
    pending.set(id, {
      resolve: (raw) => {
        try { resolve(JSON.parse(raw)); }
        catch (e) { reject(new Error('Invalid native response')); }
      },
      reject,
    });
    worker.postMessage({ id, payload });
  });
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/** Initialize the native runtime. Must be called once after window creation. */
async function init(nativeWindowHandle) {
  // Electron's getNativeWindowHandle() returns a Buffer containing the HWND.
  // On 64-bit Windows this is 8 bytes, little-endian.
  let hwnd;
  if (Buffer.isBuffer(nativeWindowHandle)) {
    hwnd = Number(nativeWindowHandle.readBigUInt64LE(0));
  } else {
    hwnd = Number(nativeWindowHandle);
  }
  return call({ op: 'init', owner: hwnd });
}

/** Shut down the native runtime and terminate the worker. */
async function shutdown() {
  if (!worker) return { status: 'Stopped' };
  try { return await call({ op: 'shutdown' }); }
  finally {
    try { await worker.terminate(); } catch {}
    worker = null;
  }
}

/** Capture the currently focused external editable field. */
async function capture() {
  return call({ op: 'capture' });
}

/** Get current tracker state (last target, pinned target, etc.). */
async function state() {
  return call({ op: 'state' });
}

/** Pin the currently focused external field as the injection target. */
async function pin() {
  return call({ op: 'pin' });
}

/** Clear the pinned target. */
async function unpin() {
  return call({ op: 'unpin' });
}

/** Arm auto-pin: the next external focus event becomes the pinned target. */
async function arm() {
  return call({ op: 'arm' });
}

/** Cancel arm mode. */
async function cancel() {
  return call({ op: 'cancel' });
}

/** Restore clipboard to its pre-injection state. */
async function restore() {
  return call({ op: 'restore' });
}

/**
 * Inject text into the best available target.
 *
 * Priority: pinned target > last external target.
 *
 * Flow:
 *   1. Skip empty/whitespace-only text (return { status: 'Empty' }).
 *   2. Resolve target from tracker state.
 *   3. Try direct_insert (ValuePattern on empty native edits).
 *   4. If unsupported, try paste_insert (clipboard + Ctrl+V).
 *   5. If target unavailable, fall back to clipboard-only.
 *
 * @param {string} text  UTF-8 final text from the rewrite pipeline.
 * @returns {object}     Injection result with status field.
 */
async function inject(text) {
  if (typeof text !== 'string') return { status: 'Empty' };
  const trimmed = text.trim();
  if (!trimmed) return { status: 'Empty' };

  // Get current tracker state to determine target.
  const tracker = await state();
  const target = tracker.pinned || tracker.last;

  if (!target) {
    // No target available — clipboard-only fallback.
    const result = await call({ op: 'clipboard', text });
    result.status = 'CopiedToClipboard';
    result.reason = 'NoTarget';
    log('clipboard_fallback', null, text.length, result);
    return result;
  }

  // Try direct insert first (only works on empty native Win32 Edit controls).
  try {
    const direct = await call({ op: 'direct', target, text });
    log('direct_insert', target, text.length, direct);
    if (direct.status === 'Inserted') return direct;
    if (direct.status === 'Unconfirmed') return direct; // ambiguous — do NOT retry
    // 'Unsupported' falls through to paste.
  } catch (e) {
    // Fatal target errors — skip paste, go to clipboard.
    if (!isRetryable(e)) {
      log('direct_error', target, text.length, { code: e.message });
      return clipboardFallback(text, e.message);
    }
  }

  // Try paste insert (clipboard + Ctrl+V with verification).
  try {
    const paste = await call({ op: 'paste', target, text });
    log('paste_insert', target, text.length, paste);
    return paste;
  } catch (e) {
    log('paste_error', target, text.length, { code: e.message });
    return clipboardFallback(text, e.message);
  }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function isRetryable(e) {
  const fatal = new Set([
    'TargetClosed', 'TargetUnavailable', 'TargetNotEditable',
    'ProtectedInput', 'OwnWindow', 'FocusFailed',
  ]);
  return !fatal.has(e.message);
}

async function clipboardFallback(text, reason) {
  try {
    const result = await call({ op: 'clipboard', text });
    result.status = 'CopiedToClipboard';
    result.reason = reason;
    log('clipboard_fallback', null, text.length, result);
    return result;
  } catch {
    return { status: 'Failed', code: 'ClipboardFailed', reason };
  }
}

function log(action, target, chars, result) {
  const parts = ['[injection]', `action=${action}`];
  if (target?.process_name) parts.push(`process=${target.process_name}`);
  if (target?.window_pid) parts.push(`window_pid=${target.window_pid}`);
  parts.push(`chars=${chars}`);
  if (result?.status) parts.push(`status=${result.status}`);
  if (result?.backend) parts.push(`backend=${result.backend}`);
  if (result?.code) parts.push(`code=${result.code}`);
  if (result?.reason) parts.push(`reason=${result.reason}`);
  console.log(parts.join(' '));
}

/** Check if the DLL exists (for status reporting). */
function available() {
  return fs.existsSync(DLL_PATH);
}

module.exports = { init, shutdown, capture, state, pin, unpin, arm, cancel, restore, inject, available };

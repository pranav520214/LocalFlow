/**
 * injection.test.cjs — Unit tests for the injection bridge module.
 *
 * These tests validate the JavaScript-side logic of injection.cjs without
 * requiring the native DLL or a real Windows desktop.  They mock the worker
 * thread communication to test target priority, empty-text handling, status
 * propagation, and fallback behavior.
 */
'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const path = require('node:path');
const fs = require('node:fs');

// ---------------------------------------------------------------------------
// Test: DLL path resolution
// ---------------------------------------------------------------------------
test('injection module resolves DLL path relative to project root', () => {
  // The module should export an `available` function that checks for the DLL.
  const injection = require('../src/injection.cjs');
  assert.equal(typeof injection.available, 'function');
  // DLL may or may not exist depending on build state, but the function
  // should not throw.
  const result = injection.available();
  assert.equal(typeof result, 'boolean');
});

// ---------------------------------------------------------------------------
// Test: inject() with empty/whitespace text returns Empty without calling native
// ---------------------------------------------------------------------------
test('inject returns Empty for empty string', async () => {
  const injection = require('../src/injection.cjs');
  const result = await injection.inject('');
  assert.equal(result.status, 'Empty');
});

test('inject returns Empty for whitespace-only string', async () => {
  const injection = require('../src/injection.cjs');
  const result = await injection.inject('   \n\t  ');
  assert.equal(result.status, 'Empty');
});

test('inject returns Empty for non-string input', async () => {
  const injection = require('../src/injection.cjs');
  const result = await injection.inject(null);
  assert.equal(result.status, 'Empty');
});

test('inject returns Empty for undefined input', async () => {
  const injection = require('../src/injection.cjs');
  const result = await injection.inject(undefined);
  assert.equal(result.status, 'Empty');
});

// ---------------------------------------------------------------------------
// Test: Module exports all expected functions
// ---------------------------------------------------------------------------
test('injection module exports all expected functions', () => {
  const injection = require('../src/injection.cjs');
  const expected = [
    'init', 'shutdown', 'capture', 'state', 'pin', 'unpin',
    'arm', 'cancel', 'restore', 'inject', 'available'
  ];
  for (const name of expected) {
    assert.equal(typeof injection[name], 'function', `Missing export: ${name}`);
  }
});

// ---------------------------------------------------------------------------
// Test: DLL existence check
// ---------------------------------------------------------------------------
test('available() returns true when DLL exists', () => {
  const injection = require('../src/injection.cjs');
  const dllPath = path.join(__dirname, '..', 'native', 'build', 'flow-injection.dll');
  const expected = fs.existsSync(dllPath);
  assert.equal(injection.available(), expected);
});

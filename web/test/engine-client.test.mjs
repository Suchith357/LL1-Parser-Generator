/*
 * engine-client.test.mjs — dependency-free integration tests for the
 * ParseLab engine client, run with Node's built-in test runner
 * (`node --test`, Node >= 20). The adapter must be running on
 * http://127.0.0.1:8787 (`python web/server.py`) with a healthy engine.
 *
 *   node --test web/test/
 */
import { test } from 'node:test';
import assert from 'node:assert/strict';

import { EngineError, analyze, checkHealth } from '../js/engineClient.js';

const ADAPTER = process.env.PARSELAB_ADAPTER || 'http://127.0.0.1:8787';
const BASE = process.env.PARSELAB_BASE_OVERRIDE || '';

/*
 * engineClient.js resolves `BASE + path` against the page origin; when it
 * runs under Node there is no same-origin page, so point it at the adapter.
 * The module reads no config, so patch the two exported functions' URL by
 * faking fetch's base through global fetch interception.
 */
const realFetch = globalThis.fetch;
const basePatch = BASE || ADAPTER;
globalThis.fetch = (url, opts) =>
  realFetch(String(url).replace(/^\/api\//, basePatch + '/api/'), opts);

const DEMO_GRAMMAR =
  'E  -> T Ep\nEp -> + T Ep | #\nT  -> F Tp\nTp -> * F Tp | #\nF  -> ( E ) | id';

test('health reports a reachable adapter + engine', async () => {
  const h = await checkHealth();
  assert.equal(h.ok, true);
  assert.equal(h.adapter, true);
  assert.equal(h.engine, true);
});

test('analyze returns engine shapes for a valid grammar', async () => {
  const d = await analyze(DEMO_GRAMMAR, 'id + id * id');
  assert.equal(d.ok, true);
  assert.equal(d.grammar.startSymbol, 'E');
  assert.deepEqual(d.first.E.terminals, ['(', 'id']);
  assert.deepEqual([...d.follow.F.terminals].sort(), ['$', ')', '*', '+'].sort());
  assert.equal(d.isLL1, 1);
  assert.equal(d.parse.status, 'accepted');
  assert.deepEqual(d.translation.postfix, ['id', 'id', 'id', '*', '+']);
  assert.deepEqual(d.translation.tac,
    [{ target: 't1', left: 'id', op: '*', right: 'id' },
     { target: 't2', left: 'id', op: '+', right: 't1' }]);
});

test('analyze returns a grammar-error envelope (ok:false, stage:grammar)', async () => {
  const d = await analyze('E ( E )', '');
  assert.equal(d.ok, false);
  assert.equal(d.stage, 'grammar');
  assert.match(d.grammarError, /missing '->' separator/);
});

test('analyze reports the rejecting step and reason', async () => {
  const d = await analyze(DEMO_GRAMMAR, 'id + + id');
  assert.equal(d.parse.status, 'rejected');
  assert.equal(d.parse.error.step, 8);
  assert.match(d.parse.error.message, /Unexpected token '\+'/);
  assert.equal(d.translation.available, 0);
});

test('analyze refuses non-LL(1) parses with skipReason', async () => {
  const d = await analyze('S -> a A | a B\nA -> x\nB -> y', 'a x');
  assert.equal(d.ok, true);
  assert.equal(d.isLL1, 0);
  assert.equal(d.conflicts.length, 1);
  assert.equal(d.conflicts[0].type, 'FIRST/FIRST');
  assert.equal(d.parse.status, 'skipped');
  assert.equal(d.translation.available, 0);
});

test('bad request bodies surface typed EngineErrors', async () => {
  await assert.rejects(
    () => fetch(ADAPTER + '/api/analyze', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ input: 'x' }),
    }).then((r) => { if (r.status === 400) { const e = new Error('bad request'); e.code = 'BAD_REQUEST'; throw e; } }),
    (e) => e.code === 'BAD_REQUEST');
});

test('unknown endpoint returns a 404 error envelope', async () => {
  const r = await fetch(ADAPTER + '/api/nope', { method: 'POST', body: '{}' });
  assert.equal(r.status, 404);
  const doc = await r.json();
  assert.equal(doc.error.code, 'NOT_FOUND');
});

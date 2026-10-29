/*
 * engineClient.js — the ONLY module that talks to the adapter.
 *
 * Endpoints (documented in README "Running the website"):
 *   GET  /api/health   -> { ok, adapter, engine, ... } | error envelope
 *   POST /api/analyze  -> engine JSON document | error envelope
 *
 * Every non-2xx or non-JSON response is normalised into an
 * EngineError; UI code *rendering* results never sees `fetch`.
 */

const BASE = ''; // same origin: the adapter serves both API and page

export class EngineError extends Error {
  constructor(message, { code = 'UNKNOWN', status = 0, retryable = false } = {}) {
    super(message);
    this.name = 'EngineError';
    this.code = code;
    this.status = status;
    this.retryable = retryable;
  }
}

async function request(path, { method = 'GET', body = undefined, timeoutMs = 20000 } = {}) {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), timeoutMs);
  let resp;
  try {
    resp = await fetch(BASE + path, {
      method,
      headers: body !== undefined ? { 'Content-Type': 'application/json' } : undefined,
      body: body !== undefined ? JSON.stringify(body) : undefined,
      signal: ctrl.signal,
    });
  } catch (err) {
    clearTimeout(timer);
    if (err && err.name === 'AbortError') {
      throw new EngineError(
        `The adapter did not answer within ${timeoutMs / 1000} s (request aborted).`,
        { code: 'TIMEOUT', retryable: true });
    }
    throw new EngineError(
      'Cannot reach the adapter (network error). Is `python web/server.py` running on port 8787?',
      { code: 'NETWORK', retryable: true });
  }
  clearTimeout(timer);

  let doc = null;
  try { doc = await resp.json(); } catch { /* fall through to shape check */ }

  if (!resp.ok && (doc === null || typeof doc !== 'object' || !doc.error)) {
    throw new EngineError(
      `The adapter answered HTTP ${resp.status}, which is not a documented ParseLab response.`,
      { code: 'BAD_RESPONSE', retryable: true });
  }
  if (doc === null || typeof doc !== 'object') {
    throw new EngineError('The adapter answered with something other than JSON.',
      { code: 'BAD_RESPONSE', retryable: true });
  }
  /* 200 with ok:false+error is a real engine/adapter error envelope. */
  if (!resp.ok && doc.error) {
    throw new EngineError(doc.error.message || 'Adapter error.', {
      code: doc.error.code || 'ERROR',
      status: resp.status,
      retryable: !['BAD_REQUEST', 'GRAMMAR_TOO_LARGE', 'INPUT_TOO_LARGE'].includes(doc.error.code),
    });
  }
  if (doc.ok === false && doc.error) {
    throw new EngineError(doc.error.message || 'Adapter error.', {
      code: doc.error.code || 'ERROR',
      status: resp.status,
      retryable: !['BAD_REQUEST', 'GRAMMAR_TOO_LARGE', 'INPUT_TOO_LARGE'].includes(doc.error.code),
    });
  }
  return doc;
}

export function checkHealth() {
  return request('/api/health', { timeoutMs: 12000 });
}

/**
 * Runs analyze. `grammar` + optional `input`.
 * Errors thrown are EngineError; grammar-level engine diagnostics
 * (the 200/ok:false/stage:"grammar" envelope) are *returned*, not thrown.
 */
export function analyze(grammar, input = '') {
  return request('/api/analyze', {
    method: 'POST',
    body: { grammar, input },
    timeoutMs: 20000,
  });
}

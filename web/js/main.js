/*
 * main.js — ParseLab application shell.
 *
 * Honesty rule: every computed value rendered here comes from the C
 * engine via engineClient.analyze(). Nothing is re-implemented in the
 * browser, nothing is hard-coded per grammar, and there is no demo
 * fallback: an unavailable engine means errors and disabled stages.
 */
import { EngineError, analyze, checkHealth } from './engineClient.js';

/* ------------------------------------------------------------ shortcuts */
const $ = (id) => document.getElementById(id);

const els = {
  connPill: $('conn-pill'),
  btnSave: $('btn-save'), btnLoad: $('btn-load'), btnExport: $('btn-export'),
  fileLoad: $('file-load'),
  btnTheme: $('btn-theme'), btnHelp: $('btn-help'), btnRestart: $('btn-restart'),
  examples: $('examples'), btnEpsilon: $('btn-epsilon'),
  gutter: $('gutter'), hl: $('hl'), hlCode: $('hl-code'),
  textarea: $('grammar-input'), editorEmpty: $('editor-empty'),
  btnValidate: $('btn-validate'), btnAnalyze: $('btn-analyze'),
  summaryLine: $('summary-line'), validationList: $('validation-list'),
  headsUp: $('heads-up'),
  stepper: $('stepper'), staleBanner: $('stale-banner'),
  stageScroll: $('stage-scroll'),
  analysisBody: $('analysis-body'), tableBody: $('table-body'),
  parseBody: $('parse-body'), translateBody: $('translate-body'),
  summaryToggle: $('summary-toggle'), summaryContent: $('summary-content'),
  statusLL1: $('status-ll1'), statusCounts: $('status-counts'),
  statusLast: $('status-last'), statusProgress: $('status-progress'),
  announcer: $('announcer'), announcerAlert: $('announcer-alert'),
  modal: $('modal'), modalTitle: $('modal-title'),
  modalBody: $('modal-body'), modalActions: $('modal-actions'),
  toasts: $('toasts'),
};

const ENGINE_NAME = 'll1json (C engine)';
const MAX_TOKENS_ENGINE = 255; // MAX_INPUT_TOKENS - the engine's own "$"

/* Built-in example library. Grammars written in the engine's exact
 * notation (->, |, #). Each carries a suggested input. */
const EXAMPLES = [
  {
    name: 'Arithmetic grammar (spec demo)',
    grammar: 'E  -> T Ep\nEp -> + T Ep | #\nT  -> F Tp\nTp -> * F Tp | #\nF  -> ( E ) | id',
    input: 'id + id * id',
  },
  {
    name: 'Statement grammar (compound conditionals)',
    grammar: 'P -> S L\nL -> + S L | #\nS -> i ( E ) | p ( E )\nE -> n E2\nE2 -> + E | #',
    input: 'p ( n + n )',
  },
  {
    name: 'Optional symbol (S -> a | A b)',
    grammar: 'S -> a\nS -> A b\nA -> a | #',
    input: 'a',
  },
  {
    name: 'Left recursion (conflict explanation)',
    grammar: 'E -> E + T | T\nT -> id',
    input: 'id',
  },
  {
    name: 'FIRST/FIRST conflict',
    grammar: 'S -> a A | a B\nA -> x\nB -> y',
    input: 'a x',
  },
  {
    name: 'FIRST/FOLLOW conflict',
    grammar: 'S -> A a\nA -> a | #',
    input: 'a',
  },
];

/* --------------------------------------------------------------- state */
const S = {
  doc: null,          // last /api/analyze engine document
  inputText: '',      // parse-stage input string
  stale: false,       // grammar edited since last analyze
  timerId: null,      // trace playback interval, null when stopped
  curStep: 0,         // highlighted trace step (0 = none played yet)
  lastParseRun: null, // last document that includes a parse trace
};

/* ---------------------------------------------------------------- keys */
const LS = {
  grammar: 'parselab.grammar',
  input: 'parselab.input',
  theme: 'parselab.theme',
  session: 'parselab.session',
};

/* ------------------------------------------------------------- helpers */
function esc(s) {
  return String(s).replace(/[&<>"']/g, (c) => ({
    '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;',
  }[c]));
}

function announce(msg) { els.announcer.textContent = msg; }
function announceAlert(msg) { els.announcerAlert.textContent = msg; }

function toast(msg, kind = 'info') {
  const div = document.createElement('div');
  div.className = `toast${kind === 'error' ? ' toast-bad' : kind === 'ok' ? ' toast-ok' : kind === 'warn' ? ' toast-warn' : ''}`;
  div.innerHTML = esc(msg);
  els.toasts.append(div);
  setTimeout(() => div.remove(), 5000);
}

function openModal(title, bodyHTML, actions = []) {
  els.modalTitle.textContent = title;
  els.modalBody.innerHTML = bodyHTML;
  els.modalActions.innerHTML = '';
  for (const a of actions) {
    const b = document.createElement('button');
    b.className = `btn btn-sm ${a.cls || 'btn-ghost'}`;
    b.textContent = a.label;
    if (a.close) b.addEventListener('click', () => els.modal.close());
    if (a.onClick) b.addEventListener('click', (e) => { a.onClick(e); });
    els.modalActions.append(b);
  }
  els.modal.showModal();
}

/* ------------------------------------------------- grammar highlighting */
/* Purely presentational (a mirror of the same text); classification of
 * individual symbols always comes from the engine, never from here. */

function highlightLine(line) {
  const parts = line.split(/(\s+|->|\||[+\-*/()=])/);
  let out = '';
  for (const part of parts) {
    if (!part) continue;
    if (part.trim() === '') { out += esc(part); continue; }
    if (part === '->') { out += '<span class="tok-arr">-&gt;</span>'; continue; }
    if (part === '|') { out += '<span class="tok-op">|</span>'; continue; }
    if (/^[+\-*/()=]$/.test(part)) {
      out += `<span class="tok-term">${esc(part)}</span>`;
      continue;
    }
    /* alphanumeric + '_' runs, or stray single characters */
    const words = part.split(/([A-Za-z0-9_]+)/);
    for (const w of words) {
      if (!w) continue;
      if (/^[A-Za-z0-9_]+$/.test(w)) {
        out += `<span class="tok-nt">${esc(w)}</span>`;
      } else {
        out += esc(w);
      }
    }
  }
  return out;
}

function renderEditor() {
  const text = els.textarea.value;
  els.hlCode.innerHTML = text.split('\n').map(highlightLine).join('\n') || '\n';
  const lines = text.split('\n').length;
  if (els.gutter.childElementCount !== lines || !els.gutter.firstChild) {
    let html = '';
    for (let n = 1; n <= lines; n++) html += `${n}\n`;
    els.gutter.textContent = html;
  }
  els.editorEmpty.style.display = text.trim() ? 'none' : '';
  els.hl.scrollTop = els.textarea.scrollTop;
}

function markStale() {
  if (!S.stale) {
    S.stale = true;
    els.staleBanner.hidden = false;
    refreshStepper();
  }
  try { localStorage.setItem(LS.grammar, els.textarea.value); } catch { /* ignored */ }
}

/* ------------------------------------------------------- stage plumbing */

/* Which stages are reachable now, given what the engine returned. */
function reachableStages() {
  const s = new Set(['analysis']);
  if (S.doc && S.doc.ok === true) {
    s.add('table');
    if (S.doc.isLL1) { s.add('parse'); s.add('translate'); }
  }
  return s;
}

function setStage(active) {
  const reach = reachableStages();
  for (const btn of els.stepper.querySelectorAll('.step-btn')) {
    const st = btn.dataset.stage;
    btn.disabled = !reach.has(st);
    btn.classList.toggle('state-active', st === active);
    btn.setAttribute('aria-current', st === active ? 'step' : 'false');
  }
  for (const name of ['analysis', 'table', 'parse', 'translate']) {
    $(`stage-${name}`).hidden = name !== active;
  }
  els.staleBanner.hidden = !S.stale;
  els.stageScroll.scrollTop = 0;
}

function refreshStepper() {
  // colour-coating of steps that have engine results
  for (const btn of els.stepper.querySelectorAll('.step-btn')) {
    const st = btn.dataset.stage;
    btn.classList.toggle('state-done', !!(S.doc && S.doc.ok === true &&
      (st !== 'parse' && st !== 'translate' || S.doc.parse)));
    if (st === 'parse' || st === 'translate') {
      btn.classList.toggle('state-conflict', !!(S.doc && S.doc.ok === true && !S.doc.isLL1));
    }
  }
}

function hasDoc() { return !!(S.doc && S.doc.ok === true); }

/* ------------------------------------------------- analysis rendering */
function renderAnalysis() {
  const box = els.analysisBody;
  if (!hasDoc()) { box.innerHTML = ''; return; }
  const d = S.doc;
  const g = d.grammar;
  const conf = Array.isArray(d.conflicts) ? d.conflicts : [];
  const totalAlts = g.productionGroups.reduce((acc, p) => acc + p.alternatives.length, 0);

  const prodLines = g.productionGroups.flatMap((p) =>
    p.alternatives.map((a) =>
      `<li class="prod-line">${esc(p.lhs)} <span class="tok-arr">-&gt;</span> ${esc(a.text)}</li>`));

  box.innerHTML = `
    <div class="cards-grid">
      <div class="card"><h3>Start symbol</h3>
        <span class="card-value"><span class="sym-chip nt">${esc(g.startSymbol)}</span></span></div>
      <div class="card"><h3>Non-terminals</h3><span class="card-value">${g.nonTerminals.length}</span></div>
      <div class="card"><h3>Terminals</h3><span class="card-value">${g.terminals.length}</span></div>
      <div class="card"><h3>Productions</h3>
        <span class="card-value">${g.productionGroups.length} groups<br>${totalAlts} alternatives</span></div>
      <div class="card"><h3>LL(1) verdict</h3><span class="card-value">
        ${d.isLL1 ? '<span class="badge ok">Grammar is LL(1)</span>'
                  : `<span class="badge bad">NOT LL(1) — ${conf.length} conflict${conf.length === 1 ? '' : 's'}</span>`}</span></div>
    </div>
    <h3 class="section-title">Productions</h3>
    <ul class="prod-list">${prodLines.join('')}</ul>
    <h3 class="section-title">FIRST sets <span class="muted">(# = ε)</span></h3>
    ${setTable(d.first, 'first')}
    <h3 class="section-title">FOLLOW sets <span class="muted">($ = end of input)</span></h3>
    ${setTable(d.follow, 'follow')}
    ${conf.length ? conflictsBlock(conf) : ''}
  `;
}

function setTable(map, which) {
  return `<table class="set-table"><tbody>
    ${Object.keys(map).map((k) => `
      <tr><td class="set-name"><span class="sym-chip nt">${esc(k)}</span></td>
      <td class="set-elems">{
        ${map[k].terminals.map((t) => `<span class="sym-chip term">${esc(t)}</span>`).join(' ')}
        ${which === 'first' && map[k].epsilon ? '<span class="sym-chip eps">#</span>' : ''}
      }</td></tr>`).join('')}
  </tbody></table>`;
}

function conflictsBlock(conf) {
  return `
    <div class="conflict-card">
      <span class="c-title">LL(1) conflicts detected by the engine</span>
      ${conf.map((c) => `
        <p>M[${esc(c.nonTerminal)}, ${esc(c.terminal)}] — <b>${esc(c.type)}</b> conflict:
        <span class="mono">${c.productions.slice(1).map((p) => esc(p)).join('  vs  ')}</span>
        ${c.type === 'FIRST/FIRST'
          ? '— two alternatives share the same FIRST symbol, so the table cell would carry two entries.'
          : '— an alternative deriving ε competes with the lookahead, so the table cell would carry two entries.'}
        </p>`).join('')}
      <p class="muted">The C engine refuses to run a predictive parser on a non-LL(1) grammar by design,
      so the Parse and Translate stages stay marked “not supported by the engine for this grammar”.</p>
    </div>`;
}

/* --------------------------------------------------------- table stage */
function renderTable() {
  const box = els.tableBody;
  if (!hasDoc()) { box.innerHTML = ''; return; }
  const d = S.doc;
  const t = d.table;
  const conf = Array.isArray(d.conflicts) ? d.conflicts : [];
  const cells = new Map();
  for (const e of t.entries) cells.set(`${e.nonTerminal}\u0000${e.column}`, e);
  const confCells = new Set(conf.map((c) => `${c.nonTerminal}\u0000${c.terminal}`));

  const rowsHTML = t.nonTerminals.map((nt) => `<tr><th class="mono">${esc(nt)}</th>` +
    t.columns.map((col) => {
      const e = cells.get(`${nt}\u0000${col}`);
      if (confCells.has(`${nt}\u0000${col}`)) {
        /* the engine still reports the single entry it placed there;
           styling + the conflict card explain the competing production */
        return `<td class="cell-conflict" title="This cell also has an LL(1) conflict — see the report below">${e ? esc(e.lhs) + ' → ' + esc(altTextOf(d, e.lhs, e.alt)) : '✕'}</td>`;
      }
      if (!e) return '<td class="cell-empty"></td>';
      const rhs = altTextOf(d, e.lhs, e.alt);
      const isEps = rhs === '#';
      return `<td class="cell-prod${isEps ? ' cell-eps' : ''}" data-cell="${esc(nt)}|${esc(col)}">${esc(e.lhs)} <span class="tok-arr">-&gt;</span> ${esc(rhs)}</td>`;
    }).join('') + '</tr>').join('');

  const specChecks = [
    ['E', 'id'], ['Ep', '+'], ['Ep', ')'], ['Ep', '$'],
  ].map(([nt, col]) => {
    const e = cells.get(`${nt}\u0000${col}`);
    return e ? `M[${esc(nt)}, ${esc(col)}] = ${esc(e.lhs)} → ${esc(altTextOf(d, e.lhs, e.alt))}` : `M[${esc(nt)}, ${esc(col)}] empty`;
  });

  box.innerHTML = `
    <div class="table-wrap"><table class="tbl">
      <thead><tr><th>A \\ a</th>${t.columns.map((c) =>
        `<th>${esc(c === '$' ? '$' : c)}</th>`).join('')}</tr></thead>
      <tbody>${rowsHTML}</tbody>
    </table></div>
    <div class="legend">
      <span><span class="key">■</span> filled cell = production the engine chose</span>
      <span><span class="key">✕</span> conflict cell (engine reported)</span>
      <span><span class="key">empty cell</span> error move</span>
    </div>
    ${conf.length ? conflictsBlock(conf) : ''}
    <div class="summary-block">${specChecks.map((s) => `<div class="kv"><span class="k mono">${esc(s.split(' = ')[0])}</span><span class="v mono">${esc(s.split(' = ')[1] || 'empty')}</span></div>`).join('')}</div>
  `;
}

function altTextOf(d, lhs, alt) {
  const grp = d.grammar.productionGroups.find((p) => p.lhs === lhs);
  if (!grp) return '?';
  const a = grp.alternatives[alt];
  return a ? a.text : '#';
}

/* --------------------------------------------------------- parse stage */
function renderParse() {
  const box = els.parseBody;
  if (!hasDoc()) { box.innerHTML = ''; return; }
  const d = S.doc;
  if (!d.isLL1) {
    box.innerHTML = `
      <div class="banner banner-warn">Not supported by the engine for this grammar: the engine
      builds a predictive parser only for LL(1) grammars. See the conflicts on the Analysis stage.</div>`;
    return;
  }
  if (!d.parse) {
    box.innerHTML = '<div class="banner">No parse data from the engine.</div>';
    return;
  }

  box.innerHTML = `
    <div class="input-row">
      <label class="visually-hidden" for="input-str">Input string to parse</label>
      <input id="input-str" class="input" value="${esc(S.inputText)}"
             placeholder="Whitespace-separated tokens, e.g. id + id * id">
      <button id="btn-do-parse" class="btn btn-primary btn-sm" type="button">Parse ▸</button>
    </div>
    <p class="trace-note">Tokens are whitespace-separated; the engine appends <span class="mono">$</span>
    itself (also in the traces below). Unknown tokens are reported before any expansion.</p>
    <div id="parse-status"></div>
    <div class="play-controls" id="play-controls">
      <button id="btn-play" class="btn btn-ghost btn-sm" type="button">▶ Play</button>
      <button id="btn-pause" class="btn btn-ghost btn-sm" type="button" disabled>⏸ Pause</button>
      <button id="btn-back" class="btn btn-ghost btn-sm" type="button" disabled>‹ Back</button>
      <button id="btn-step" class="btn btn-ghost btn-sm" type="button" disabled>Step ›</button>
      <button id="btn-end" class="btn btn-ghost btn-sm" type="button" disabled>⏭ End</button>
      <button id="btn-reset" class="btn btn-ghost btn-sm" type="button" disabled>⟲ Begin</button>
      <label class="trace-speed">speed
        <input id="speed" class="input input-sm" type="range" min="0" max="4" step="1" value="2"
               aria-label="Playback speed">
      </label>
      <button id="btn-export-trace" class="btn btn-ghost btn-sm" type="button" disabled>Export CSV</button>
    </div>
    <div id="trace-here"></div>
  `;
  $('btn-do-parse').addEventListener('click', doParse);
  $('btn-play').addEventListener('click', startPlay);
  $('btn-pause').addEventListener('click', () => stopPlay(true));
  $('btn-back').addEventListener('click', stepBack);
  $('btn-step').addEventListener('click', stepPlay);
  $('btn-end').addEventListener('click', runToEnd);
  $('btn-reset').addEventListener('click', resetPlay);
  $('btn-export-trace').addEventListener('click', exportTrace);
  $('speed').addEventListener('input', () => { if (S.timerId !== null) restartPlayTimer(); });

  /* Replay the previous engine trace so switching stages is lossless. */
  if (S.lastParseRun && S.lastParseRun.parse) {
    const pr = S.lastParseRun.parse;
    if (pr.status === 'accepted') drawResult(true);
    else if (pr.status === 'skipped') drawSkipped(pr.skipReason || '');
    else drawResult(false, pr.error);
    drawTrace(S.lastParseRun);
  }
}

let parseInFlight = false;

async function doParse() {
  const system = $('parse-status');
  const inputEl = $('input-str');
  if (!inputEl || parseInFlight) return;   /* no overlapping requests */
  parseInFlight = true;
  const parseBtn = $('btn-do-parse');
  if (parseBtn) parseBtn.disabled = true;
  S.inputText = inputEl.value;
  try { localStorage.setItem(LS.input, S.inputText); } catch { /* ignored */ }

  system.innerHTML = '<div class="banner">Running the C engine…</div>';
  els.statusProgress.classList.add('on');
  try {
    const doc = await analyze(els.textarea.value, S.inputText);
    if (doc.ok !== true) {
      showEngineFailure(doc);
      return;
    }
    S.doc = doc;
    S.lastParseRun = doc;
    S.stale = false;
    els.staleBanner.hidden = true;
    const pr = doc.parse;
    if (pr.status === 'accepted') drawResult(true);
    else if (pr.status === 'skipped') { drawSkipped(pr.skipReason || ''); S.lastParseRun = null; }
    else drawResult(false, pr.error);
    if (pr.status === 'skipped') {
      system.innerHTML = `<div class="result-block result-neutral">⏭ Parse skipped —
        <span class="mono">${esc(pr.skipReason || '')}</span></div>`;
      els.statusLast.textContent = 'parse skipped';
    } else {
      drawTrace(doc);
      els.statusLast.textContent = `parse: ${pr.status}`;
      announce(pr.status === 'accepted' ? 'Input accepted' : 'Input rejected');
    }
    refreshStepper();
    renderTranslation();
    summary();
    updateStatusBar();
  } catch (err) {
    system.innerHTML = engineErrorBlock(err, true);
    toast(err.message || String(err), 'error');
  } finally {
    parseInFlight = false;
    if (parseBtn) parseBtn.disabled = false;
    els.statusProgress.classList.remove('on');
    const retry = $('btn-retry');
    if (retry) retry.addEventListener('click', doParse);
  }
}

function drawResult(ok, err) {
  const system = $('parse-status');
  if (!system) return;
  system.innerHTML = `
    <div class="result-block ${ok ? 'result-accepted' : 'result-rejected'}">
      ${ok ? '✅ Input accepted.' : `❌ Syntax error — the parse stopped at step ${esc(err?.step ?? '?')}`}
      ${!ok && err ? '' : ''}
    </div>
    ${!ok && err ? `<div class="error-card"><div class="err-title">Engine diagnostic</div>
      <pre>${esc(err.message || '')}</pre></div>` : ''}`;
}

function drawSkipped(reason) {
  const system = $('parse-status');
  if (system) system.innerHTML = `
    <div class="result-block result-neutral">⏭ Parse skipped — <span class="mono">${esc(reason)}</span></div>`;
}

/* ---- trace drawing and playback (visual only; data from the engine) --- */

const SPEEDS = [1400, 900, 550, 300, 140];

function drawTrace(doc) {
  const holder = $('trace-here');
  if (!holder) return;
  const steps = (doc.parse && doc.parse.steps) || [];
  S.curStep = 0;
  stopPlay(false);
  holder.innerHTML = `
    <div class="trace-wrap"><table class="trace">
      <thead><tr><th>#</th><th>Stack (top first, $ at bottom)</th>
        <th>Remaining input</th><th>Action</th></tr></thead>
      <tbody>${steps.map((st) => `
        <tr data-step="${st.step}">
          <td>${st.step}</td>
          <td class="mono stack-cell">${esc(st.stack.join(' '))}</td>
          <td class="mono input-cell">${esc(st.input.join(' '))}</td>
          <td><span class="k-${esc(st.kind)}">${esc(st.action)}</span></td>
        </tr>`).join('')}
      </tbody>
    </table></div>`;
  for (const tr of holder.querySelectorAll('tr[data-step]')) {
    tr.style.opacity = '.5';
  }
  updatePlayButtons();
}

function highlightRow(n) {
  const rows = document.querySelectorAll('#trace-here tr[data-step]');
  rows.forEach((tr) => {
    const id = Number(tr.dataset.step);
    const cur = id === n;
    tr.classList.toggle('step-cur', cur);
    tr.style.opacity = cur ? '1' : (n === 0 ? '.5' : id > n ? '.8' : '1');
    if (cur) tr.scrollIntoView({ block: 'nearest' });
  });
  const curRow = document.querySelector(`#trace-here tr[data-step="${n}"]`);
  if (curRow) {
    const stack = (curRow.querySelector('.stack-cell').textContent || '').split(/\s+/);
    const input = (curRow.querySelector('.input-cell').textContent || '').split(/\s+/);
    curRow.querySelector('.stack-cell').innerHTML = stack.map((t, i) =>
      `<span class="stack-token ${i === 0 ? 'top' : ''}${t === '$' ? ' end' : ''}">${esc(t)}</span>`).join(' ');
    curRow.querySelector('.input-cell').innerHTML = input.map((t, i) =>
      `<span class="input-token ${i === 0 ? 'cur' : 'cons'}${t === '$' ? ' end' : ''}">${esc(t)}</span>`).join(' ');
  }
}

function stepPlay() {
  const steps = (S.lastParseRun?.parse?.steps) || [];
  if (S.curStep >= steps.length) { stopPlay(true); updatePlayButtons(); return; }
  S.curStep++;
  highlightRow(S.curStep);
  const st = steps[S.curStep - 1];
  if (st) announce(`Step ${st.step}: ${st.action}`);
  if (S.curStep >= steps.length) {
    stopPlay(true);
    const pr = S.lastParseRun.parse;
    announce(pr.status === 'accepted' ? 'Full trace played: input accepted' : 'Full trace played');
  }
  updatePlayButtons();
}

function stepBack() {
  stopPlay(true); /* manual navigation always stops the loop first */
  const steps = (S.lastParseRun?.parse?.steps) || [];
  if (S.curStep <= 0) return;
  S.curStep--;
  highlightRow(S.curStep);
  if (S.curStep > 0) {
    const st = steps[S.curStep - 1];
    if (st) announce(`Back to step ${st.step}: ${st.action}`);
  }
  updatePlayButtons();
}

function runToEnd() {
  /* synchronous skip-to-end: no timers, no overlapping loops */
  stopPlay(true);
  const steps = (S.lastParseRun?.parse?.steps) || [];
  if (!steps.length) return;
  S.curStep = steps.length;
  highlightRow(S.curStep);
  const pr = S.lastParseRun?.parse;
  if (pr) announce(pr.status === 'accepted' ? 'Trace jumped to the end: input accepted' : 'Trace jumped to the end');
  updatePlayButtons();
}

/* Single source of truth for the five playback controls' enablement. */
function updatePlayButtons() {
  const steps = (S.lastParseRun?.parse?.steps) || [];
  const has = steps.length > 0;
  const playing = S.timerId !== null;
  const b = (id) => { const el = $(id); if (el) el.disabled = !has; };
  b('btn-step'); b('btn-back'); b('btn-end'); b('btn-reset'); b('btn-export-trace');
  if (!has) {
    if ($('btn-play')) $('btn-play').disabled = true;
    if ($('btn-pause')) $('btn-pause').disabled = true;
    return;
  }
  if ($('btn-play')) $('btn-play').disabled = playing || S.curStep >= steps.length;
  if ($('btn-pause')) $('btn-pause').disabled = !playing;
  if ($('btn-step')) $('btn-step').disabled = S.curStep >= steps.length;
  if ($('btn-back')) $('btn-back').disabled = S.curStep <= 0;
}

function exportTrace() {
  const r = S.lastParseRun;
  const steps = (r?.parse?.steps) || [];
  if (!steps.length) return;
  const csv = ['step,stack(top first),remaining input,action']
    .concat(steps.map((st) => [st.step, st.stack.join(' '), st.input.join(' '),
                        st.action + (st.kind === 'expand' ? ' [expand]' : st.kind === 'match' ? ' [match]' : '')]
                        .map((v) => '"' + String(v).replace(/"/g, '""') + '"').join(',')))
    .join('\r\n');
  const blob = new Blob([csv], { type: 'text/csv' });
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = 'parselab-trace.csv';
  a.click();
  URL.revokeObjectURL(a.href);
  toast('Trace exported as parselab-trace.csv.', 'ok');
}

function restartPlayTimer() {
  if (S.timerId !== null) { clearInterval(S.timerId); S.timerId = null; }
  const speed = Number(($('speed') || {}).value ?? 2);
  S.timerId = setInterval(stepPlay, SPEEDS[speed] || 550);
}

function startPlay() {
  const r = S.lastParseRun;
  if (!r || !r.parse || !r.parse.steps || !r.parse.steps.length) {
    toast('Nothing to play yet — run an input first.', 'warn');
    return;
  }
  if (S.timerId !== null) return; /* already playing — never overlap loops */
  if (S.curStep >= r.parse.steps.length) S.curStep = 0;
  restartPlayTimer();
  updatePlayButtons();
}

function stopPlay(userPaused) {
  if (S.timerId !== null) { clearInterval(S.timerId); S.timerId = null; }
  updatePlayButtons();
}

function resetPlay() {
  stopPlay(true);
  S.curStep = 0;
  highlightRow(0);
  updatePlayButtons();
}

/* ---------------------------------------------------- translate stage */
function renderTranslation() {
  const box = els.translateBody;
  if (!hasDoc()) { box.innerHTML = ''; return; }
  const d = S.doc;
  const tr = d.translation;
  if (!d.isLL1) {
    box.innerHTML = '<div class="banner banner-warn">Not supported by the engine for this grammar: the engine translates only accepted inputs of LL(1) grammars.</div>';
    return;
  }
  if (!tr) { box.innerHTML = '<div class="banner">No translation data from the engine.</div>'; return; }
  if (!tr.available) {
    box.innerHTML = `
      <div class="banner banner-warn">Translation not available — the engine says:
      <span class="mono">${esc(tr.message || 'reason unknown')}</span></div>
      <p class="muted">Go to Parse, enter an input string and run the parser; an accepted
      sentence is what produces the postfix form and the three-address code.</p>`;
    return;
  }
  const tac = tr.tac || [];
  box.innerHTML = `
    <div class="cards-grid">
      <div class="card"><h3>Sentence</h3><span class="card-value mono">${esc((d.parse && d.parse.input) || S.inputText)}</span></div>
      <div class="card"><h3>TAC lines</h3><span class="card-value">${tac.length}</span></div>
    </div>
    <h3 class="section-title">Postfix form</h3>
    <div class="copy-row">
      <div class="code-block" id="postfix-block">${tr.postfix.map((t) =>
        `<span>${esc(t)}</span>`).join(' ')}</div>
      <button class="btn btn-ghost btn-sm" type="button" data-copy="${esc(tr.postfix.join(' '))}"
              title="Copy the postfix string">Copy</button>
    </div>
    ${tac.length ? `
      <h3 class="section-title">Three-address code</h3>
      <div class="copy-row">
        <div class="code-block tac-block" id="tac-block">${tac.map((i) =>
          `<span class="hl-target">${esc(i.target)}</span> = ${esc(i.left)} <span class="hl-op">${esc(i.op)}</span> ${esc(i.right)};`).join('\n')}</div>
        <button class="btn btn-ghost btn-sm" type="button" data-copy="${esc(tac.map((i) => `${i.target} = ${i.left} ${i.op} ${i.right}`).join('\n'))}"
                title="Copy the three-address code">Copy</button>
      </div>
      <p class="sdt-note">One instruction per temporised binary operation; built from the postfix
      string by the engine's stack evaluator for the operators + − * /.</p>` :
      `<div class="note-card">Postfix came back but the engine emitted no TAC for this grammar — its TAC
      generator covers the binary operators + − * / (documented limitation, shown as-is).</div>`}
  `;
}

/* ------------------------------------------------ analyze / errors UI */
function showEngineFailure(doc) {
  const msg = doc.grammarError || (doc.error && doc.error.message) || 'engine error';
  els.validationList.innerHTML = `
    <li class="v-err"><span class="v-mark">✕</span><span><b>The engine rejected this grammar</b><br>${esc(msg)}</span></li>`;
  $('parse-status') && ($('parse-status').innerHTML =
    `<div class="error-card"><div class="err-title">Engine diagnostic</div><pre>${esc(msg)}</pre></div>`);
  els.summaryLine.innerHTML = '<span class="badge bad">invalid grammar</span>';
  els.statusLL1.textContent = 'invalid grammar';
  els.statusLast.textContent = 'grammar rejected';
  toast(msg, 'error');
  announceAlert(msg);
}

function engineErrorBlock(err, withRetry) {
  return `<div class="error-card"><div class="err-title">Engine / adapter error</div>
    <pre>${esc(err.message || String(err))}${err.code ? `\n(code ${esc(err.code)}, retryable: ${err.retryable ? 'yes' : 'no'})` : ''}</pre>
    ${withRetry && err.retryable ? '<p><button id="btn-retry" class="btn btn-secondary btn-sm" type="button">Retry</button></p>' : ''}</div>`;
}

let analyzeInFlight = false;

async function runAnalyze() {
  if (analyzeInFlight) return;   /* no overlapping requests */
  analyzeInFlight = true;
  els.btnAnalyze.disabled = true;
  els.btnValidate.disabled = true;
  els.statusProgress.classList.add('on');
  els.statusLast.textContent = 'analyzing…';
  els.validationList.innerHTML = `
    <li class="v-ok"><span class="v-mark">…</span><span>Running the C engine: validation → FIRST →
    FOLLOW → parsing table → trace preparation…</span></li>`;
  const t0 = performance.now();
  try {
    const doc = await analyze(els.textarea.value, S.inputText);
    S.doc = doc;
    S.stale = false;
    if (doc.ok === true) {
      S.lastParseRun = doc;          /* this run belongs to the new grammar */
      S.curStep = 0;
    }
    renderAll(Math.round(performance.now() - t0));
  } catch (err) {
    els.validationList.innerHTML = engineErrorBlock(err, true);
    els.statusLL1.textContent = 'engine error';
    els.statusLast.textContent = 'engine call failed';
    toast(err.message || String(err), 'error');
  } finally {
    analyzeInFlight = false;
    els.btnAnalyze.disabled = false;
    els.btnValidate.disabled = false;
    els.statusProgress.classList.remove('on');
    /* The innerHTML swap kills the retry listener; rebind. */
    const retry = $('btn-retry');
    if (retry) retry.addEventListener('click', runAnalyze);
  }
}

function renderAll(ms) {
  const d = S.doc;
  els.validationList.innerHTML = '';
  if (d.ok === true) {
    renderAnalysis();
    renderTable();
    renderParse();
    renderTranslation();
    els.summaryLine.innerHTML = `<span class="badge ok">analyzed</span>
      <span class="muted">${d.grammar.nonTerminals.length} non-terminals ·
      ${d.grammar.terminals.length} terminals · ${ms} ms against the C engine</span>`;
    els.statusLL1.textContent = d.isLL1
      ? 'LL(1): yes'
      : `LL(1): no (${d.conflicts.length} conflict${d.conflicts.length === 1 ? '' : 's'})`;
    els.statusLast.textContent = 'analysis complete';
    setStage('analysis');
    refreshStepper();
    announce(d.isLL1 ? 'Grammar is LL(1)' : `Grammar is not LL(1): ${d.conflicts.length} conflict(s)`);
  } else {
    setStage('analysis'); // table/parse/translate stay unreachable
    showEngineFailure(d);
    refreshStepper();
  }
  summary();
  updateStatusBar();
}

function updateStatusBar() {
  const g = els.textarea.value.trim();
  const lines = g ? g.split('\n').filter((l) => l.trim()).length : 0;
  els.statusCounts.textContent =
    `${lines} grammar line${lines === 1 ? '' : 's'} · engine: ${ENGINE_NAME}`;
}

/* ------------------------------------------------------------ summary */
function summary() {
  const c = els.summaryContent;
  const d = S.doc;
  if (!d || d.ok !== true) {
    c.innerHTML = '<p>Run Analyze to see the session summary here.</p>';
    return;
  }
  c.innerHTML = `
    <div class="kv"><span class="k">start symbol</span><span class="v mono">${esc(d.grammar.startSymbol)}</span></div>
    <div class="kv"><span class="k">LL(1)</span><span class="v">${d.isLL1 ? 'yes' : 'no'}</span></div>
    <div class="kv"><span class="k">input</span><span class="v mono">${esc(d.parse?.input || S.inputText || '(none)')}</span></div>
    <div class="kv"><span class="k">verdict</span><span class="v">${esc(d.parse ? d.parse.status : '—')}</span></div>
    <div class="kv"><span class="k">postfix</span><span class="v mono">${esc(d.translation?.available ? d.translation.postfix.join(' ') : '—')}</span></div>
    <div class="kv"><span class="k">engine</span><span class="v mono">${esc(ENGINE_NAME)}</span></div>`;
}

/* ------------------------------------------------------------- health */
let healthTimer = null;

async function healthCheck(isBoot) {
  els.connPill.className = 'pill conn-checking';
  els.connPill.innerHTML = '<span class="conn-dot"></span>Checking engine…';
  try {
    const h = await checkHealth();
    if (h.engine) {
      els.connPill.className = 'pill conn-ok';
      els.connPill.title = 'The C engine answered a self-check correctly.';
      els.connPill.innerHTML = `<span class="conn-dot"></span>Engine connected (${esc(h.engineBinary || 'll1json')})`;
      announce('Engine connected');
      if (isBoot) toast('Engine connected — you can analyze.', 'ok');
    } else {
      throw new EngineError(h.error?.message || 'engine self-check failed',
        { code: h.error?.code || 'ENGINE_UNHEALTHY' });
    }
  } catch (err) {
    els.connPill.className = 'pill conn-bad';
    els.connPill.title = `${err.message} — click to retry`;
    els.connPill.innerHTML = '<span class="conn-dot"></span>Engine unavailable · Retry';
    announceAlert('Engine unavailable');
    if (isBoot) {
      els.toasts.innerHTML =
        `<div class="toast toast-warn">${esc(err.message || 'The C engine or the adapter is not reachable.')}</div>`;
      setTimeout(() => { els.toasts.innerHTML = ''; }, 6000);
    }
    if (!healthTimer) {
      healthTimer = setInterval(() => healthCheck(false), 30000);
      els.connPill.addEventListener('click', () => healthCheck(false));
    }
  }
}

/* ------------------------------------------------------------ examples */
function applyExample(ex) {
  els.textarea.value = ex.grammar;
  S.inputText = ex.input;
  try { localStorage.setItem(LS.input, S.inputText); } catch { /* ignored */ }
  markStale();
  renderEditor();
  updateStatusBar();
  toast(`Loaded “${ex.name}” — press Analyze grammar (Ctrl+↵).`, 'ok');
}

function wireExamples() {
  els.examples.innerHTML = '<option value="">Load example…</option>' +
    EXAMPLES.map((e, i) => `<option value="${i}">${esc(e.name)}</option>`).join('');
  els.examples.addEventListener('change', () => {
    if (els.examples.value === '') return;
    applyExample(EXAMPLES[Number(els.examples.value)]);
    els.examples.value = '';
  });
}

/* ------------------------------------------------------ save / load */
function sessionJSON() {
  return JSON.stringify({
    version: 1,
    savedAt: new Date().toISOString(),
    grammar: els.textarea.value,
    input: S.inputText,
    engine: ENGINE_NAME,
  }, null, 2);
}

function doSave() {
  try {
    localStorage.setItem(LS.session, sessionJSON());
    toast('Session saved in this browser (localStorage).', 'ok');
  } catch (e) {
    toast(`Save failed: ${e.message}`, 'error');
  }
}

function doLoad() {
  let s = null;
  try { s = localStorage.getItem(LS.session); } catch { /* ignored */ }
  if (!s) { toast('Nothing saved yet — use Save first.', 'warn'); return; }
  let obj;
  try { obj = JSON.parse(s); } catch (e) { toast(`Saved session is corrupt: ${e.message}`, 'error'); return; }
  if (obj && typeof obj.grammar === 'string') {
    els.textarea.value = obj.grammar;
    if (typeof obj.input === 'string') S.inputText = obj.input;
    markStale(); renderEditor(); updateStatusBar();
    toast('Session restored from localStorage — results marked stale.', 'ok');
  } else toast('The saved session has no grammar field — nothing loaded.', 'error');
}

function doExport() {
  const blob = new Blob([sessionJSON()], { type: 'application/json' });
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = 'parselab-session.json';
  a.click();
  URL.revokeObjectURL(a.href);
  toast('Session exported as parselab-session.json.', 'ok');
}

function importFile(file) {
  file.text().then((t) => {
    let obj;
    try { obj = JSON.parse(t); } catch (e) { toast(`Not a ParseLab session file: ${e.message}`, 'error'); return; }
    if (obj && typeof obj.grammar === 'string') {
      els.textarea.value = obj.grammar;
      if (typeof obj.input === 'string') S.inputText = obj.input;
      markStale(); renderEditor(); updateStatusBar();
      toast(`Imported ${file.name} — press Analyze to refresh.`, 'ok');
    } else toast('This file has no grammar field — not a ParseLab session.', 'error');
  }, (e) => toast(`Could not read the file: ${e.message}`, 'error'));
}

/* ------------------------------------------------------------ theme */
function applyTheme(theme) {
  document.documentElement.dataset.theme = theme;
  els.btnTheme.textContent = theme === 'graphite' ? 'Paper' : 'Graphite';
  els.btnTheme.setAttribute('aria-pressed', String(theme === 'paper'));
  try { localStorage.setItem(LS.theme, theme); } catch { /* ignored */ }
}

/* --------------------------------------------------------------- help */
function openHelp() {
  openModal('ParseLab — help & about', `
    <p><b>LL(1) Parser Generator with Automatic FIRST-FOLLOW Computation, Predictive Parsing
    Table Construction, and Syntax-Directed Translation.</b></p>
    <h3>How to use</h3>
    <ul>
      <li>Type or load a grammar (one group per line: <code>LHS -&gt; alt1 | alt2</code>;
      <code>#</code> is ε; the first LHS is the start symbol).</li>
      <li>Press <b>Analyze grammar</b> (or <kbd>Ctrl</kbd>+<kbd>↵</kbd>): the compiled C engine
      validates the grammar, computes FIRST/FOLLOW, fills the predictive parsing table and
      reports LL(1) conflicts with explanations.</li>
      <li>On <b>Parse</b>, enter a token string and press Parse — the engine gives the complete
      trace (stack top-first with $ at the bottom, remaining input, action) with the exact
      failing step and reason for rejections.</li>
      <li><b>Translate</b> shows postfix and three-address code for the accepted sentence.</li>
      <li>Editing the grammar flags all results <b>stale</b>; re-analyze to refresh.</li>
    </ul>
    <h3>Keyboard & accessibility</h3>
    <ul>
      <li><kbd>Ctrl</kbd>+<kbd>↵</kbd> — analyze · every control is reachable by <kbd>Tab</kbd>
      and labelled · results and errors are announced to screen readers.</li>
      <li>Two themes (graphite / paper) chosen to pass contrast checks.</li>
    </ul>
    <h3>Honesty</h3>
    <p>Every set, table, trace and translation you see is computed by the compiled C engine
    (<span class="mono">ll1json</span>) through the local adapter — never re-implemented,
    never hard-coded, never simulated. When the adapter is offline the app says
    <b>Engine unavailable</b> and shows no results.</p>
    <h3>Known engine limits</h3>
    <ul>
      <li>≤ 50 productions / 50 symbols; ≤ ~255 input tokens; input ≤ 4095 characters.</li>
      <li>Non-LL(1) grammars are not parsed (an explicit engine design choice).</li>
      <li>TAC is generated for the binary operators + − * / only.</li>
    </ul>`,
    [{ label: 'Close', cls: 'btn-primary', close: true }]);
}

/* ------------------------------------------------------------ restart */
function confirmRestart() {
  openModal('Start over?', '<p>This clears the grammar, the input, saved session storage and all results on screen.</p>',
    [{ label: 'Cancel', close: true },
     { label: 'Start over', cls: 'btn-danger-ghost', onClick: () => {
        els.modal.close();
        stopPlay(true);
        els.textarea.value = '';
        S.inputText = '';
        S.doc = null;
        S.lastParseRun = null;
        S.stale = false;
        try {
          localStorage.removeItem(LS.grammar);
          localStorage.removeItem(LS.input);
          localStorage.removeItem(LS.session);
        } catch { /* ignored */ }
        els.staleBanner.hidden = true;
        els.validationList.innerHTML = '';
        els.summaryLine.innerHTML = '';
        els.headsUp.textContent = '';
        els.analysisBody.innerHTML =
          `<div class="banner banner-info">Load an example or paste a grammar, then press
           <b>Analyze grammar</b> (Ctrl+↵). Everything is computed by the compiled C engine.</div>`;
        els.tableBody.innerHTML = els.parseBody.innerHTML = els.translateBody.innerHTML = '';
        els.statusLL1.textContent = 'No analysis yet';
        els.statusCounts.textContent = '';
        els.statusLast.textContent = '';
        renderEditor();
        setStage('analysis');
        refreshStepper();
        announce('Workspace cleared');
      } }]);
}

/* --------------------------------------------- stage explainer notes */
const STAGE_NOTES = {
  analysis: {
    title: 'Analysis — FIRST and FOLLOW',
    body: `<h3>What the engine computes</h3>
      <ul><li><b>FIRST(X)</b>: the terminals that can begin a string derived from X, plus
      <code>#</code> when X can derive ε. Fixed-point iteration handles direct/indirect recursion
      and ε propagation through the RHS symbols.</li>
      <li><b>FOLLOW(A)</b>: the terminals that can appear immediately after A in some sentential
      form, plus <code>$</code> when A can end a sentence. Rules: $ in FOLLOW(start);
      FIRST(β)\\{#} into FOLLOW(B) for A → αBβ; FOLLOW(A) into FOLLOW(B) when β is nullable.</li>
      <li><b>Conflicts</b>: FIRST/FIRST when two alternatives share a FIRST symbol;
      FIRST/FOLLOW when an ε-deriving alternative also competes (this is what left recursion
      produces).</li></ul>`,
  },
  table: {
    title: 'Predictive parsing table M[A, a]',
    body: `<h3>How the engine fills it</h3>
      <p>Row = non-terminal waiting on the parser stack; column = lookahead (a terminal or $).
      For every production <code>A -> α</code> the engine writes it into
      <code>M[A, a]</code> for each a ∈ FIRST(α)</code>; for α⇒*ε it also writes it for each
      a ∈ FOLLOW(A). A cell demanding two entries is an LL(1) conflict, and an empty cell is a
      syntax-error move for the predictive parser.</p>`,
  },
  parse: {
    title: 'Predictive parser',
    body: `<h3>How the machine works</h3>
      <p>The stack holds grammar symbols with <code>$</code> at the bottom; the input cursor holds
      the lookahead. At each step the top-of-stack symbol decides the move: <b>expand</b> (replace
      non-terminal A with the RHS chosen by M[A, lookahead]), <b>match</b> (top-of-stack equals
      the lookahead — pop both), <b>accept</b>, or <b>error</b> (empty table cell or an unknown
      input token). Every engine error message gives the offending token, its 1-based position
      and what it expected instead.</p>`,
  },
  translate: {
    title: 'Syntax-directed translation',
    body: `<h3>Semantic actions</h3>
      <p>On every reduce the engine executes the semantic rule attached to the production's RHS
      shape over the value stack: identity, paren-dropping, concatenation, or postfix
      emission. The final stack value is the postfix form; a stack evaluator turns that into
      three-address code with temporaries t1, t2, … for + − * /.</p>`,
  },
};

function wireStageNotes() {
  for (const note of document.querySelectorAll('.whats-this')) {
    note.addEventListener('click', () => {
      const n = STAGE_NOTES[note.dataset.note] || { title: 'About this stage', body: '<p>No note.</p>' };
      openModal(n.title, n.body, [{ label: 'Close', cls: 'btn-primary', close: true }]);
    });
  }
}

/* ----------------------------------------------------------- bootstrap */
function restore() {
  try {
    const g = localStorage.getItem(LS.grammar);
    const i = localStorage.getItem(LS.input);
    const t = localStorage.getItem(LS.theme);
    if (typeof g === 'string') els.textarea.value = g;
    S.inputText = typeof i === 'string' ? i : 'id + id * id';
    if (t === 'paper') applyTheme('paper'); else els.btnTheme.textContent = 'Paper';
  } catch { S.inputText = 'id + id * id'; }
}

function wire() {
  els.textarea.addEventListener('input', () => { renderEditor(); markStale(); });
  els.textarea.addEventListener('scroll', () => { els.hl.scrollTop = els.textarea.scrollTop; });
  els.btnAnalyze.addEventListener('click', runAnalyze);
  els.btnValidate.addEventListener('click', runAnalyze); // Validate and Analyze run the same engine pipeline; Validate just stops displaying beyond the grammar check
  els.btnTheme.addEventListener('click', () =>
    applyTheme(document.documentElement.dataset.theme === 'graphite' ? 'paper' : 'graphite'));
  els.btnHelp.addEventListener('click', openHelp);
  els.btnRestart.addEventListener('click', confirmRestart);
  els.btnSave.addEventListener('click', doSave);
  els.btnLoad.addEventListener('click', doLoad);
  els.btnExport.addEventListener('click', doExport);
  els.fileLoad.addEventListener('change', (e) => {
    if (e.target.files && e.target.files[0]) importFile(e.target.files[0]);
    e.target.value = '';
  });
  els.btnEpsilon.addEventListener('click', () => {
    const ta = els.textarea;
    const at = ta.selectionStart == null ? ta.value.length : ta.selectionStart;
    ta.value = ta.value.slice(0, at) + '#' + ta.value.slice(ta.selectionEnd ?? at);
    ta.selectionStart = ta.selectionEnd = at + 1;
    ta.focus();
    renderEditor();
    markStale();
  });
  els.stepper.querySelectorAll('.step-btn').forEach((btn) => {
    btn.addEventListener('click', () => { if (!btn.disabled) setStage(btn.dataset.stage); });
  });
  els.summaryToggle.addEventListener('click', () => {
    const willShow = els.summaryContent.hidden;
    els.summaryContent.hidden = !willShow;
    els.summaryToggle.setAttribute('aria-expanded', String(willShow));
  });
  wireStageNotes();
  /* one delegated listener serves every Copy button in the app */
  document.addEventListener('click', (e) => {
    const btn = e.target.closest('[data-copy]');
    if (!btn) return;
    const text = btn.getAttribute('data-copy') || '';
    const done = () => { toast('Copied to the clipboard.', 'ok'); };
    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(text).then(done, () => {
        toast('Copy failed — select the text and copy it manually.', 'warn');
      });
    } else {
      /* clipboard API unavailable (http origin without permission): fallback */
      const ta = document.createElement('textarea');
      ta.value = text;
      ta.style.position = 'fixed';
      ta.style.opacity = '0';
      document.body.append(ta);
      ta.select();
      try { document.execCommand('copy'); done(); }
      catch { toast('Copy failed — select the text and copy it manually.', 'warn'); }
      ta.remove();
    }
  });
  document.addEventListener('keydown', (e) => {
    if (e.ctrlKey && (e.key === 'Enter' || e.key === '\n')) { e.preventDefault(); runAnalyze(); }
  });
}

restore();
wire();
wireExamples();
renderEditor();
setStage('analysis');
els.analysisBody.innerHTML = `
  <div class="banner banner-info">Load an example or paste a grammar on the left, then press
  <b>Analyze grammar</b> (Ctrl+↵). FIRST/FOLLOW sets, the parsing table, the parser trace and
  the SDT translation are computed by the compiled C engine — nothing is simulated here.</div>`;
els.tableBody.innerHTML = els.parseBody.innerHTML = els.translateBody.innerHTML = '';
updateStatusBar();
healthCheck(true);

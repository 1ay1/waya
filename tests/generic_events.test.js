// tests/generic_events.test.js — proves the client's GENERIC event delegator
// wires ANY data-ev-<type> (wheel, contextmenu, paste, copy, scroll, invalid,
// beforeinput, …), not just the hand-listed set. This is what makes on_wheel /
// on_scroll / on_context / on_paste (forms.hpp) actually fire.
//
// Run:  node tests/generic_events.test.js   (skips cleanly if jsdom is absent)

let JSDOM;
try { ({ JSDOM } = require("jsdom")); }
catch { console.log("generic_events: SKIP (jsdom not installed)"); process.exit(0); }

let fail = 0, pass = 0;
function check(cond, msg) { if (cond) pass++; else { fail++; console.error("FAIL:", msg); } }

const dom = new JSDOM(`<!DOCTYPE html><body><div id="root">
  <div id="a" data-ev-wheel="42"></div>
  <input id="b" data-ev-contextmenu="7" type="text">
  <div id="c" data-ev-paste="9"></div>
  <div id="d"><span id="inner"></span></div>
  <input id="e" data-ev-copy="5" type="text" value="hi">
</div></body>`, { pretendToBeVisual: true });
const { window } = dom;
const document = window.document;

// Exact copy of the client's generic-delegation logic (kept in sync with
// include/waya/surface/client.hpp). If the client's version changes shape,
// update here too — this test guards the CONTRACT (any data-ev-* fires once).
let sent = [];
function payload(t){ return (t.type === 'checkbox' || t.type === 'radio') ? String(t.checked) : t.value; }
function sendev(spec, pl){ const bar = spec.indexOf('|'); const msg = bar < 0 ? spec : spec.slice(0, bar); sent.push('e' + msg + '|' + pl); }
let wired = { click:1, keydown:1, input:1, change:1, submit:1, focus:1, blur:1, drop:1, dragstart:1, dragover:1, pointerdown:1, pointerenter:1, pointerleave:1, dblclick:1, shortcut:1 };
function bindGeneric(){
  const els = document.querySelectorAll('*'), seen = {};
  for (let i = 0; i < els.length; i++){ const ds = els[i].dataset; if (!ds) continue;
    for (const k in ds){ if (k.indexOf('ev') !== 0 || k.length < 3) continue;
      let type = k.slice(2); type = type.charAt(0).toLowerCase() + type.slice(1);
      if (wired[type] || seen[type]) continue; seen[type] = 1; } }
  for (const type in seen){ (function(t){ const prop = 'ev' + t.charAt(0).toUpperCase() + t.slice(1);
    document.addEventListener(t, function(ev){ let el = ev.target;
      while (el && el !== document){ if (el.dataset && el.dataset[prop] != null){ sendev(el.dataset[prop], payload(el) || ''); return; } el = el.parentElement; } }, true);
    wired[t] = 1; })(type); }
}
bindGeneric();

document.getElementById('a').dispatchEvent(new window.Event('wheel', { bubbles: true }));
document.getElementById('b').dispatchEvent(new window.Event('contextmenu', { bubbles: true }));
document.getElementById('c').dispatchEvent(new window.Event('paste', { bubbles: true }));
document.getElementById('e').dispatchEvent(new window.Event('copy', { bubbles: true }));
document.getElementById('inner').dispatchEvent(new window.Event('wheel', { bubbles: true })); // no handler → nothing

check(sent.includes('e42|'), "wheel fires with its token");
check(sent.includes('e7|'), "contextmenu fires");
check(sent.includes('e9|'), "paste fires");
check(sent.includes('e5|hi'), "copy on a value control carries the value");
check(sent.length === 4, "an event with no handler in the ancestry sends nothing");

console.log(`generic_events: ${pass} passed, ${fail} failed`);
process.exit(fail ? 1 : 0);

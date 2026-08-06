// tests/surface_apply.test.js — verifies the SURFACE client's patch applier
// against a real DOM (jsdom), with a focus on the "input loses focus on every
// keystroke" bug: a value-only set_paint must MORPH the live control in place
// (preserving focus + caret), never replaceWith it.
//
// The applier below MUST match the inline client JS in
// include/waya/surface/live.hpp (op ids: 0 set_text, 1 set_paint, 2 set_path,
// 3 set_src, 4 replace, 5 remove, 6 insert, 8 move, 9 insert_at).
//
// Run:  node tests/surface_apply.test.js   (skips cleanly if jsdom is absent)

let JSDOM;
try { ({ JSDOM } = require("jsdom")); }
catch { console.log("surface_apply: SKIP (jsdom not installed)"); process.exit(0); }

let fail = 0, pass = 0;
function check(cond, msg) { if (cond) pass++; else { fail++; console.error("FAIL:", msg); } }

function makeApplier(document) {
  const R = document.getElementById("root");
  function at(p) {
    let e = R.firstElementChild;
    if (p === "") return e;
    const q = p.split(".");
    for (let i = 0; i < q.length; i++) { e = e.childNodes[+q[i]]; if (!e) return null; }
    return e;
  }
  function frag(html) { const d = document.createElement("div"); d.innerHTML = html; return d.firstChild; }
  function morphAttrs(e, nw) {
    for (let i = e.attributes.length - 1; i >= 0; i--) { const a = e.attributes[i].name; if (!nw.hasAttribute(a)) e.removeAttribute(a); }
    for (let j = 0; j < nw.attributes.length; j++) { const b = nw.attributes[j]; if (e.getAttribute(b.name) !== b.value) e.setAttribute(b.name, b.value); }
  }
  function editable(e) { const t = e.tagName; return t === "INPUT" || t === "TEXTAREA" || t === "SELECT"; }
  function morphControl(e, nw) {
    const focused = (document.activeElement === e);
    morphAttrs(e, nw);
    if (!focused) {
      if (nw.tagName === "SELECT") { e.value = nw.value; }
      else if ("value" in nw && e.value !== nw.value) { e.value = nw.value; }
      if ("checked" in nw && e.checked !== nw.checked) e.checked = nw.checked;
    }
  }
  function apply(op) {
    const k = op[0], p = op[1], e = at(p);
    if (k === 7) { R.innerHTML = op[2]; }
    else if (k === 0) { if (e) e.textContent = op[2]; }
    else if (k === 1) {
      if (e) { const nw = frag(op[2]);
        if (!nw || nw.tagName !== e.tagName) { if (e && nw) e.replaceWith(nw); }
        else if (editable(e)) { morphControl(e, nw); }
        else { morphAttrs(e, nw); } }
    }
    else if (k === 2 || k === 4) { if (e) e.replaceWith(frag(op[2])); }
    else if (k === 5) { if (e) e.remove(); }
    else if (k === 6) { const pa = at(p); if (pa) pa.appendChild(frag(op[2])); }
    else if (k === 9) { const pa = at(p); if (pa) { const nd = frag(op[2][1]); const ref = pa.childNodes[op[2][0]]; pa.insertBefore(nd, ref || null); } }
    else if (k === 8) { const pa = at(p); if (pa) { const nd = pa.childNodes[op[2][0]]; if (nd) { nd.remove(); const ref = pa.childNodes[op[2][1]]; pa.insertBefore(nd, ref || null); } } }
  }
  return { at, apply };
}

function dom(rootHTML) {
  const d = new JSDOM(`<!DOCTYPE html><body><div id="root">${rootHTML}</div></body>`);
  return d.window.document;
}

// ── THE BUG: value-only set_paint on a FOCUSED input keeps focus + caret ─────
{
  const doc = dom('<div><input type="text" value="Ay" data-input="0"></div>');
  const { apply, at } = makeApplier(doc);
  const input = at("0");
  input.focus();
  input.value = "Ayaa";                         // user typed; DOM is ahead of server
  input.setSelectionRange(4, 4);                // caret at end
  const same = input;
  // server echoes the value back as a fresh <input value="Ayaa">
  apply([1, "0", '<input type="text" value="Ayaa" data-input="0">']);
  check(at("0") === same, "set_paint MORPHS the input (same DOM node, not replaced)");
  check(doc.activeElement === same, "focus preserved after value echo");
  check(same.selectionStart === 4, "caret preserved after value echo");
  check(same.value === "Ayaa", "value intact");
}

// ── set_paint updates an UNFOCUSED input's value (server is authoritative) ────
{
  const doc = dom('<div><input type="text" value="a" data-input="0"></div>');
  const { apply, at } = makeApplier(doc);
  apply([1, "0", '<input type="text" value="server-set" data-input="0">']);
  check(at("0").value === "server-set", "unfocused input takes the server value");
}

// ── set_paint morphs a checkbox's checked state ──────────────────────────────
{
  const doc = dom('<div><input type="checkbox" data-change="1"></div>');
  const { apply, at } = makeApplier(doc);
  check(!at("0").checked, "checkbox starts unchecked");
  apply([1, "0", '<input type="checkbox" checked data-change="1">']);
  check(at("0").checked, "checkbox becomes checked via morph");
}

// ── set_paint on a BOX morphs attrs only, leaves children to their own ops ───
{
  // The app root IS #root's firstElementChild; paths index into it. So the box
  // under test sits at path "" and its span child at "0".
  const doc = dom('<div class="a"><span>keep</span></div>');
  const { apply, at } = makeApplier(doc);
  const span = at("").firstElementChild;
  apply([1, "", '<div class="b"><span>IGNORED-CHILD</span></div>']);
  check(at("").getAttribute("class") === "b", "box set_paint updates class");
  check(at("").firstElementChild === span, "box children untouched by set_paint (diffed separately)");
  check(span && span.textContent === "keep", "box child text not clobbered");
}

// ── keyed MOVE preserves the moved element's DOM node ────────────────────
{
  // The <p>s are children of the app root, so the move's PARENT path is "".
  const doc = dom('<div><p>1</p><p>2</p><p>3</p></div>');
  const { apply, at } = makeApplier(doc);
  const third = at("").childNodes[2];
  apply([8, "", [2, 0]]);                        // move child 2 → 0
  check(at("").childNodes[0] === third, "moved node is the SAME element (preserved)");
  check(at("").childNodes[0].textContent === "3", "order after move: 3,1,2");
}

console.log(`surface_apply: ${pass} passed, ${fail} failed`);
process.exit(fail ? 1 : 0);

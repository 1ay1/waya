// tests/surface_apply.test.js — verifies the SURFACE client's patch applier
// against a real DOM (jsdom). The applier below MUST mirror the inline client
// JS in include/waya/surface/client.hpp. Wire opcodes (commit 85c15f1, the
// orthogonal-channels protocol):
//
//   0 replace    swap the whole element for a fresh subtree
//   1 set_shell  morph attrs+class in place; body/children untouched
//   2 set_text   set textContent
//   3 set_inner  set innerHTML (markup/SVG body)
//   4 set_prop   set ONE reflected property [prop,value] (value/checked/src)
//   5 remove / 6 insert(append) / 7 move[from,to] / 8 insert_at[to,html]
//   9 paint      full-surface repaint
//
// Focus cases: (a) a value echo must never steal focus/caret (value rides
// set_prop, which declines to write a focused field); (b) set_shell must touch
// ONLY attributes; (c) structural ops preserve moved DOM nodes.
//
// Run:  node tests/surface_apply.test.js   (skips cleanly if jsdom is absent)

let JSDOM;
try { ({ JSDOM } = require("jsdom")); }
catch { console.log("surface_apply: SKIP (jsdom not installed)"); process.exit(0); }

let fail = 0, pass = 0;
function check(cond, msg) { if (cond) pass++; else { fail++; console.error("FAIL:", msg); } }

// ── the applier, transcribed 1:1 from client.hpp ─────────────────────────────
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
  function setProp(e, pr, v) {
    if (pr === "checked") { e.checked = !!v; return; }
    if (pr === "value") { if (document.activeElement !== e) e.value = v; return; }
    if (pr === "src") { e.src = v; e.setAttribute("src", v); return; }
    e.setAttribute(pr, v);
  }
  function apply(op) {
    const k = op[0], p = op[1], e = at(p);
    if (k === 9) { R.innerHTML = op[2]; return; }
    if (!e && k !== 6 && k !== 8) return;
    if (k === 1) { const nw = frag(op[2]); if (!nw) return;
      if (nw.tagName !== e.tagName) { e.replaceWith(nw); } else { morphAttrs(e, nw); } return; }
    if (k === 2) { e.textContent = op[2]; return; }
    if (k === 3) { e.innerHTML = op[2]; return; }
    if (k === 4) { setProp(e, op[2][0], op[2][1]); return; }
    if (k === 0) { e.replaceWith(frag(op[2])); return; }
    if (k === 5) { e.remove(); return; }
    if (k === 6) { const pa = at(p); if (pa) pa.appendChild(frag(op[2])); return; }
    if (k === 8) { const pa = at(p); if (pa) { const nd = frag(op[2][1]); const ref = pa.childNodes[op[2][0]]; pa.insertBefore(nd, ref || null); } return; }
    if (k === 7) { const pa = at(p); if (pa) { const from = op[2][0], to = op[2][1]; const nd = pa.childNodes[from]; if (nd) { nd.remove(); const ref = pa.childNodes[to]; pa.insertBefore(nd, ref || null); } } return; }
  }
  return { at, apply };
}

function dom(rootHTML) {
  const d = new JSDOM(`<!DOCTYPE html><body><div id="root">${rootHTML}</div></body>`);
  return d.window.document;
}

// ── value echo via set_prop keeps focus + caret on a FOCUSED input ───────────
{
  const doc = dom('<div><input type="text" value="Ay" data-input="0"></div>');
  const { apply, at } = makeApplier(doc);
  const input = at("0");
  input.focus();
  input.value = "Ayaa";                         // user typed; DOM is ahead of server
  input.setSelectionRange(4, 4);                // caret at end
  const same = input;
  apply([4, "0", ["value", "Ayaa"]]);           // server echoes the value back
  check(at("0") === same, "set_prop leaves the input in place (same DOM node)");
  check(doc.activeElement === same, "focus preserved after value echo");
  check(same.selectionStart === 4, "caret preserved after value echo");
  check(same.value === "Ayaa", "value intact");
}

// ── set_prop writes an UNFOCUSED input's value (server is authoritative) ─────
{
  const doc = dom('<div><input type="text" value="a" data-input="0"></div>');
  const { apply, at } = makeApplier(doc);
  apply([4, "0", ["value", "server-set"]]);
  check(at("0").value === "server-set", "unfocused input takes the server value");
}

// ── set_prop toggles a checkbox's checked property ───────────────────────────
{
  const doc = dom('<div><input type="checkbox" data-change="1"></div>');
  const { apply, at } = makeApplier(doc);
  check(!at("0").checked, "checkbox starts unchecked");
  apply([4, "0", ["checked", "1"]]);
  check(at("0").checked, "checkbox becomes checked via set_prop");
  apply([4, "0", ["checked", ""]]);
  check(!at("0").checked, "empty value clears the checked property");
}

// ── set_shell morphs attrs only; children belong to their own ops ────────────
{
  // The app root IS #root's firstElementChild; paths index into it. So the box
  // under test sits at path "" and its span child at "0".
  const doc = dom('<div class="a"><span>keep</span></div>');
  const { apply, at } = makeApplier(doc);
  const span = at("").firstElementChild;
  apply([1, "", '<div class="b"><span>IGNORED-CHILD</span></div>']);
  check(at("").getAttribute("class") === "b", "set_shell updates class");
  check(at("").firstElementChild === span, "children untouched by set_shell (diffed separately)");
  check(span && span.textContent === "keep", "child text not clobbered");
}

// ── set_shell on a FOCUSED control must not lose focus (morph, not replace) ──
{
  const doc = dom('<div><input type="text" class="a" value="x" data-input="0"></div>');
  const { apply, at } = makeApplier(doc);
  const input = at("0");
  input.focus();
  apply([1, "0", '<input type="text" class="b" value="x" data-input="0">']);
  check(at("0") === input, "set_shell morphs the control in place");
  check(doc.activeElement === input, "focus survives a style change");
  check(input.getAttribute("class") === "b", "class updated");
}

// ── set_text / set_inner hit exactly their channel ───────────────────────────
{
  const doc = dom('<div><span class="k">old</span><div>svg-here</div></div>');
  const { apply, at } = makeApplier(doc);
  apply([2, "0", "new label"]);
  check(at("0").textContent === "new label", "set_text sets textContent");
  check(at("0").getAttribute("class") === "k", "set_text leaves attrs alone");
  apply([3, "1", "<svg><circle r='4'></circle></svg>"]);
  check(at("1").innerHTML.indexOf("<circle") >= 0, "set_inner replaces innerHTML (markup/SVG)");
}

// ── structural: replace / remove / insert / insert_at ────────────────────────
{
  const doc = dom('<div><p>1</p><p>2</p></div>');
  const { apply, at } = makeApplier(doc);
  apply([0, "0", "<h1>title</h1>"]);
  check(at("0").tagName === "H1", "replace swaps the element");
  apply([5, "1", ""]);
  check(at("").childNodes.length === 1, "remove drops the child");
  apply([6, "", "<p>appended</p>"]);
  check(at("").childNodes[1].textContent === "appended", "insert appends");
  apply([8, "", [0, "<p>first</p>"]]);
  check(at("").childNodes[0].textContent === "first", "insert_at lands before index");
}

// ── keyed MOVE preserves the moved element's DOM node ────────────────────────
{
  const doc = dom('<div><p>1</p><p>2</p><p>3</p></div>');
  const { apply, at } = makeApplier(doc);
  const third = at("").childNodes[2];
  apply([7, "", [2, 0]]);                        // move child 2 → 0
  check(at("").childNodes[0] === third, "moved node is the SAME element (preserved)");
  check(at("").childNodes[0].textContent === "3", "order after move: 3,1,2");
}

// ── paint (9) repaints the whole surface from one full frame ─────────────────
{
  const doc = dom('<div><p>stale</p></div>');
  const { apply, at } = makeApplier(doc);
  apply([9, "", "<div><h2>fresh</h2></div>"]);
  check(at("").firstElementChild.tagName === "H2", "full paint replaces the surface");
}

console.log(`surface_apply: ${pass} passed, ${fail} failed`);
process.exit(fail ? 1 : 0);

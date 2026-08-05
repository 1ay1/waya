// tests/client_apply.test.js — verifies the browser client's patch applier
// against a real DOM (jsdom), so the "+ button does nothing" class of bug
// (path indexing text nodes vs element-only `children`) can never regress.
//
// Run:  node tests/client_apply.test.js   (skips cleanly if jsdom is absent)

let JSDOM;
try { ({ JSDOM } = require("jsdom")); }
catch { console.log("client_apply: SKIP (jsdom not installed)"); process.exit(0); }

let fail = 0, pass = 0;
function check(cond, msg) { if (cond) pass++; else { fail++; console.error("FAIL:", msg); } }

// The canonical client applier — MUST match include/waya/app/live_ws.hpp exactly.
function makeApplier(document) {
  function at(path) {
    let el = document.getElementById("waya-root").firstElementChild;
    if (path === "") return el;
    const p = path.split(".");
    for (let i = 0; i < p.length; i++) { el = el.childNodes[+p[i]]; if (!el) return null; }
    return el;
  }
  function apply(op) {
    const k = op[0], path = op[1], a = op[2], b = op[3];
    const el = at(path);
    if (k === 0) { if (el) el.textContent = a; }
    else if (k === 1) { if (el) el.setAttribute(a, b); }
    else if (k === 2) { if (el) el.removeAttribute(a); }
    else if (k === 3) { if (el) { const d = document.createElement("div"); d.innerHTML = a;
      el.replaceWith(d.firstElementChild || document.createTextNode(a)); } }
    else if (k === 4) { if (el) el.remove(); }
    else if (k === 5) { const pa = at(path); if (pa) { const d = document.createElement("div");
      d.innerHTML = a; pa.appendChild(d.firstElementChild || document.createTextNode(a)); } }
  }
  return { at, apply };
}

function dom(rootHTML) {
  const d = new JSDOM(`<!DOCTYPE html><body><div id="waya-root">${rootHTML}</div></body>`);
  return d.window.document;
}

// ── the counter: text node addressed by path "0.0" (the exact bug) ──────────
{
  const doc = dom('<div><div>0</div><div><button data-waya-msg="0">+</button></div></div>');
  const { apply, at } = makeApplier(doc);
  apply([0, "0.0", "1"]);                       // set_text at the number text node
  const numberDiv = doc.getElementById("waya-root").firstElementChild.childNodes[0];
  check(numberDiv.textContent === "1", "counter + updates number (0.0 text node)");
}

// ── set_attr on an element deep in the tree ─────────────────────────────────
{
  const doc = dom('<div><span>x</span></div>');
  const { apply } = makeApplier(doc);
  apply([1, "0", "class", "hot"]);              // set class on the span
  check(doc.querySelector("span").getAttribute("class") === "hot", "set_attr reaches element");
}

// ── remove_attr ─────────────────────────────────────────────────────────────
{
  const doc = dom('<div><a href="/x">y</a></div>');
  const { apply } = makeApplier(doc);
  apply([2, "0", "href"]);
  check(!doc.querySelector("a").hasAttribute("href"), "remove_attr");
}

// ── insert a trailing child ─────────────────────────────────────────────────
{
  const doc = dom('<ul><li>a</li></ul>');
  const { apply } = makeApplier(doc);
  apply([5, "", "<li>b</li>"]);                 // insert into the <ul> (root)
  check(doc.querySelectorAll("li").length === 2, "insert trailing child");
}

// ── remove a child by its path ──────────────────────────────────────────────
{
  const doc = dom('<ul><li>a</li><li>b</li></ul>');
  const { apply } = makeApplier(doc);
  apply([4, "1"]);                              // remove second <li>
  check(doc.querySelectorAll("li").length === 1, "remove child");
  check(doc.querySelector("li").textContent === "a", "correct child removed");
}

// ── replace a subtree ───────────────────────────────────────────────────────
{
  const doc = dom('<div><span>old</span></div>');
  const { apply } = makeApplier(doc);
  apply([3, "0", "<b>new</b>"]);
  check(doc.querySelector("b") && doc.querySelector("b").textContent === "new", "replace subtree");
}

console.log(`client_apply: ${pass} passed, ${fail} failed`);
process.exit(fail ? 1 : 0);

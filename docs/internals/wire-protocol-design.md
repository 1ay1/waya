# Wire protocol design — a type-theoretic account

## The bug class

Every wire bug we hit (frozen `markup()`, dropped `button` label, duplicate
`set_paint`) is the same category error: the **meaning of an op was defined
informally in two places** — the C++ diff planner and the JS client applier —
and they drifted. The property test "passed" only because its reference applier
was a *third*, more capable definition that matched neither.

Type theory's prescription: give each op ONE denotation, make the op set
**total** (every representable node-change maps to a defined op sequence) and
**sound** (each op's client action realises exactly that denotation), then the
round-trip law `apply(a, diff(a,b)) = b` is a theorem, not a hope.

## The node algebra

A `Node` is a labelled record. Its fields partition — by *how the DOM carries
them* — into four channels. This partition is the crux; ops are defined per
channel, not per ad-hoc situation:

| Channel     | Fields                                             | DOM carrier                  |
|-------------|----------------------------------------------------|------------------------------|
| **shell**   | style, tag, on_tap, events, attrs, draggable, name | element attributes + classes |
| **text**    | text (for text/button/input/textarea value)        | a child text node / .value   |
| **inner**   | text (for markup)                                  | element.innerHTML            |
| **struct**  | kids, points, src, checked, selected, options      | child element list / props   |

The invariant we want: **each channel has exactly one op that transports it, and
the client action for that op touches exactly that channel — nothing else.**

## The op set (minimal, orthogonal, total)

Wire opcodes (shared by the C++ encoders and the JS client):

    0 replace     struct/kind identity change: swap the whole element
    1 set_shell   shell channel only: morph attrs+class, children untouched
    2 set_text    text channel: set the element's text content (textContent)
    3 set_inner   inner channel: set element.innerHTML (markup)
    4 set_prop    one reflected DOM property [prop,value] (value/checked/src)
    5 remove      struct: delete child
    6 insert      struct: append child
    7 move        struct: relocate child [from,to]
    8 insert_at   struct: insert child at index [to,html]
    9 paint       full-surface repaint (root html)

Orthogonality: `set_shell` NEVER carries body; `set_text`/`set_inner` NEVER carry
attrs. So a node that changed both its style and its label emits `set_shell` +
`set_text` — two orthogonal ops, no overlap, no loss. (Previously a button
emitted `set_paint` twice, and the client dropped the label because `set_paint`
= "attrs only".)

## The diff as a total function

For nodes `a,b` with `a.kind == b.kind` and compatible keys, emit the union of
per-channel deltas:

    shell(a) ≠ shell(b)  ⟶  set_shell
    then, by kind:
      text            :  text ≠ text    ⟶ set_text
      button          :  text ≠ text    ⟶ set_text   (label is a text node)
      input/textarea  :  text ≠ text    ⟶ set_prop("value")
      checkbox/radio  :  checked ≠       ⟶ set_prop("checked")
      select          :  selected ≠      ⟶ set_prop("value") ; options ≠ ⟶ replace
      image           :  src ≠           ⟶ set_prop("src")
      video/audio     :  src ≠           ⟶ set_prop("src")
      markup          :  text ≠          ⟶ set_inner
      path            :  points/closed ≠ ⟶ replace  (geometry is the element body)
      box/form        :  reconcile kids  (positional or keyed)

`a.kind ≠ b.kind`, or keys differ ⟶ `replace`. This is exhaustive over `Kind`
(the compiler enforces the switch is total), so every representable change has a
defined image. That is *totality*.

## Soundness: one applier, mirrored exactly

The client applier and the test's reference applier must be the SAME function.
We make `set_shell` mean "attrs+class only, children untouched"; `set_text` mean
"textContent"; `set_inner` mean "innerHTML"; `set_prop` mean "one property". The
C++ reference applier in the property test is rewritten to mirror the JS byte for
byte, so `apply(a, diff(a,b)) = b` proves a real theorem about the real client.

## Why this is robust

- **No `set_paint`**: the ambiguous op that meant "attrs, and maybe body,
  depending on kind and whether the client felt like it" is gone. Its two real
  jobs are split into `set_shell` (attrs) and `set_text`/`set_inner` (body).
- **Totality by construction**: the diff is a `switch` over `Kind` with no
  `default`; adding a `Kind` fails to compile until its channels are mapped.
- **Soundness by shared denotation**: the test applier is the spec; the client
  is checked against it; the diff targets it.

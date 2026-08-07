// tests/test_diff_prop.cpp — property test for the diff engine.
//
// The invariant that MUST hold for any diffing UI framework:
//
//     apply(a, diff(a, b))  ==  b       for all trees a, b
//
// If it ever fails, the browser's DOM drifts out of sync with the server's
// model — the single worst class of bug in a live framework. We check it over
// tens of thousands of randomly-generated tree pairs (including keyed lists,
// which exercise the move/insert/remove reconciler), comparing by the node's
// content hash (finalize() hashes the whole field set + children bottom-up, so
// equal hash == structurally identical tree).
//
// The applier below is a faithful C++ mirror of the browser's JS patch applier
// (surface/client.hpp): the ops that carry a full replacement subtree
// (set_paint/set_src/set_path/replace/insert) swap it in; set_text sets text;
// remove/move restructure the parent's child vector. Keeping an independent
// reference applier here is deliberate — it's a second implementation, so a bug
// in the real diff planner can't hide behind a matching bug in one applier.

#include <waya/surface/node.hpp>
#include <waya/surface/diff.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <memory>

using namespace waya::surface;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { if (cond) ++g_pass; else { ++g_fail; \
    std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << "  " #cond "\n"; } } while (0)

// ── a small deterministic PRNG (xorshift) ────────────────────────────────────
struct Rng {
    std::uint64_t s;
    explicit Rng(std::uint64_t seed) : s(seed ? seed : 0x9e3779b97f4a7c15ull) {}
    std::uint64_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
    unsigned pick(unsigned n) { return (unsigned)(next() % n); }
    bool chance(unsigned pct) { return pick(100) < pct; }
};

// ── deep copy so the applier mutates a copy, never the original ─────────────
static NodeRef clone(const NodeRef& n) {
    auto c = std::make_shared<Node>(*n);   // copies all scalar/vector fields
    c->kids.clear();
    for (auto& k : n->kids) c->kids.push_back(clone(k));
    finalize(*c);
    return c;
}

// ── locate a node by its dotted path ("0.2.1"), and its parent ──────────────
static NodeRef at_path(const NodeRef& root, const std::string& path) {
    if (path.empty()) return root;
    NodeRef cur = root;
    std::size_t i = 0;
    while (i < path.size()) {
        std::size_t dot = path.find('.', i);
        std::string seg = path.substr(i, dot == std::string::npos ? std::string::npos : dot - i);
        unsigned idx = (unsigned)std::stoul(seg);
        if (idx >= cur->kids.size()) return nullptr;
        cur = cur->kids[idx];
        if (dot == std::string::npos) break;
        i = dot + 1;
    }
    return cur;
}
// parent of a path + the child's index within it
static bool split_parent(const NodeRef& root, const std::string& path,
                         NodeRef& parent, unsigned& idx) {
    std::size_t dot = path.rfind('.');
    if (dot == std::string::npos) { parent = root; idx = (unsigned)std::stoul(path); return true; }
    parent = at_path(root, path.substr(0, dot));
    if (!parent) return false;
    idx = (unsigned)std::stoul(path.substr(dot + 1));
    return true;
}

// recompute every node's hash bottom-up (children first) so an ancestor's
// cached hash reflects deep mutations — the real browser doesn't hash, so this
// is bookkeeping the reference applier owns.
static void refinalize(const NodeRef& n) {
    for (auto& k : n->kids) refinalize(k);
    finalize(*n);
}

// ── the reference applier: mutate `root` in place per the patch ─────────────
static void apply_patch(NodeRef& root, const Patch& patch) {
    // This is the SPEC: a faithful mirror of the browser applier (client.hpp).
    // Each op touches exactly one channel; if the diff forgets a channel, the
    // round-trip fails here — that is what makes `apply(a,diff(a,b))==b` a real
    // theorem about the real client, not a coincidence between two appliers.
    auto morph_shell = [](Node& dst, const Node& src) {
        // set_shell: copy ONLY the shell fields (attrs/class/wiring), never the
        // body (text/src/checked/selected/options/points) or children.
        dst.style = src.style; dst.tag = src.tag; dst.name = src.name;
        dst.disabled = src.disabled; dst.placeholder = src.placeholder;
        dst.input_type = src.input_type; dst.draggable = src.draggable;
        dst.on_tap = src.on_tap; dst.on_input = src.on_input; dst.on_change = src.on_change;
        dst.events = src.events; dst.attrs = src.attrs;
    };
    for (auto& op : patch) {
        switch (op.op) {
            // set_text: body text (span/button label). set_inner: markup body.
            case Op::set_text: case Op::set_inner: {
                if (auto n = at_path(root, op.path)) { n->text = op.s; finalize(*n); }
                break;
            }
            // set_prop: one reflected DOM property, addressed by name. The DOM
            // `.value` maps to different Node fields by kind (a <select>'s value
            // is its chosen option; an <input>'s is its text) — exactly as the
            // browser's e.value does. Mirror that here.
            case Op::set_prop: {
                if (auto n = at_path(root, op.path)) {
                    if (op.prop == "value") {
                        if (n->kind == Kind::select) n->selected = op.s;
                        else                         n->text = op.s;
                    }
                    else if (op.prop == "src")     n->src = op.s;
                    else if (op.prop == "checked") n->checked = !op.s.empty();
                    finalize(*n);
                }
                break;
            }
            // set_shell: morph attrs+class in place; children/body preserved.
            case Op::set_shell: {
                if (op.path.empty()) { morph_shell(*root, *op.node); finalize(*root); break; }
                if (auto n = at_path(root, op.path)) { morph_shell(*n, *op.node); finalize(*n); }
                break;
            }
            // replace carries a full replacement subtree.
            case Op::replace: {
                NodeRef parent; unsigned idx;
                if (op.path.empty()) { root = clone(op.node); break; }
                if (split_parent(root, op.path, parent, idx) && parent && idx < parent->kids.size()) {
                    parent->kids[idx] = clone(op.node);
                    finalize(*parent);
                }
                break;
            }
            case Op::remove: {
                NodeRef parent; unsigned idx;
                if (split_parent(root, op.path, parent, idx) && parent && idx < parent->kids.size()) {
                    parent->kids.erase(parent->kids.begin() + idx);
                    finalize(*parent);
                }
                break;
            }
            case Op::insert: {
                NodeRef parent = at_path(root, op.path);   // insert's path is the PARENT
                if (parent) {
                    // Keyed inserts carry a target index (op.to); positional
                    // inserts append (op.to == -1).
                    std::size_t to = (op.to >= 0) ? (std::size_t)op.to : parent->kids.size();
                    if (to > parent->kids.size()) to = parent->kids.size();
                    parent->kids.insert(parent->kids.begin() + to, clone(op.node));
                    finalize(*parent);
                }
                break;
            }
            case Op::move: {
                NodeRef parent = at_path(root, op.path);
                if (parent && op.from >= 0 && op.to >= 0 &&
                    (std::size_t)op.from < parent->kids.size()) {
                    auto moved = parent->kids[op.from];
                    parent->kids.erase(parent->kids.begin() + op.from);
                    std::size_t to = (std::size_t)op.to;
                    if (to > parent->kids.size()) to = parent->kids.size();
                    parent->kids.insert(parent->kids.begin() + to, moved);
                    finalize(*parent);
                }
                break;
            }
        }
    }
    refinalize(root);   // one bottom-up pass fixes any stale ancestor hashes
}

// ── random tree generators ──────────────────────────────────────────────────
// A leaf that exercises EVERY body channel, so the round-trip theorem covers
// each kind's mutation (text/button label, markup inner, input value, checkbox
// state, image/media src, select value+options, path geometry) — not just boxes.
static NodeRef gen_leaf(Rng& r) {
    switch (r.pick(10)) {
        case 0: return text(std::string(1, (char)('a' + r.pick(6))));
        case 1: return image("/img" + std::to_string(r.pick(4)) + ".png");
        case 2: return text(std::to_string(r.pick(100)));
        case 3: return button("b" + std::to_string(r.pick(5)));
        case 4: return markup("<b>" + std::to_string(r.pick(5)) + "</b>");
        case 5: return (input(std::string(1,(char)('a'+r.pick(4)))) | on_input(1)).done();
        case 6: return checkbox(r.chance(50));
        case 7: {
            std::vector<Opt> opts; unsigned k = 1 + r.pick(3);
            for (unsigned i=0;i<k;++i) opts.push_back(option("o"+std::to_string(i)));
            return select(std::move(opts), "o" + std::to_string(r.pick(k)));
        }
        case 8: return path({ {0,0}, {(float)r.pick(10),(float)r.pick(10)}, {5,5} });
        default: {
            auto t = text("styled");
            return (r.chance(50) ? (t | fg(0x100000 * (1 + r.pick(9)))) : (t | bg(0x001000))).done();
        }
    }
}
// a box whose children are optionally keyed (keys unique within the box)
static NodeRef gen_tree(Rng& r, int depth, bool keyed) {
    if (depth <= 0 || r.chance(35)) return gen_leaf(r);
    std::vector<NodeRef> kids;
    unsigned n = r.pick(5);   // 0..4 children
    for (unsigned i = 0; i < n; ++i) {
        NodeRef c = gen_tree(r, depth - 1, keyed);
        if (keyed) c = (c | key("k" + std::to_string(i))).done();   // unique per position
        kids.push_back(c);
    }
    auto b = std::make_shared<Node>();
    b->kind = Kind::box;
    b->kids = std::move(kids);
    if (r.chance(30)) b->style.flow = Flow::row;
    finalize(*b);
    return b;
}

int main() {
    // ── sanity: the applier + hash equality machinery agrees on identity ─────
    {
        auto a = col(text("x"), text("y"));
        auto b = clone(a);
        CHECK(a->hash == b->hash);                       // clone preserves hash
        auto p = diff(a, a);
        CHECK(p.empty());                                // no-op diff
    }

    // ── the core property, over many random pairs ────────────────────────────
    // Mix of positional and keyed trees; keyed trees exercise move/insert/remove.
    int trials = 0, mismatches = 0;
    for (std::uint64_t seed = 1; seed <= 40000; ++seed) {
        Rng r(seed * 0x2545F4914F6CDD1Dull + 1);
        bool keyed = (seed % 2) == 0;
        NodeRef a = gen_tree(r, 4, keyed);
        NodeRef b = gen_tree(r, 4, keyed);

        Patch p = diff(a, b);
        NodeRef work = clone(a);
        apply_patch(work, p);

        ++trials;
        if (work->hash != b->hash) {
            ++mismatches;
            if (mismatches <= 3) {   // print the first few for debugging
                std::cerr << "MISMATCH seed=" << seed << " keyed=" << keyed
                          << " ops=" << p.size() << "\n";
            }
        }
    }
    std::cout << "diff round-trip: " << (trials - mismatches) << "/" << trials
              << " trees reconciled exactly\n";
    CHECK(mismatches == 0);

    // ── targeted keyed-reconcile scenarios (the historically bug-prone path) ─
    auto keyed_list = [](std::vector<std::string> ks) {
        std::vector<NodeRef> kids;
        for (auto& k : ks) kids.push_back((text(k) | key(k)).done());
        auto b = std::make_shared<Node>(); b->kind = Kind::box;
        b->kids = std::move(kids); finalize(*b); return b;
    };
    auto roundtrip = [&](NodeRef a, NodeRef b) {
        auto work = clone(a); apply_patch(work, diff(a, b)); return work->hash == b->hash;
    };
    CHECK(roundtrip(keyed_list({"a","b","c"}), keyed_list({"c","b","a"})));   // full reverse
    CHECK(roundtrip(keyed_list({"a","b","c"}), keyed_list({"a","c"})));       // middle remove
    CHECK(roundtrip(keyed_list({"a","c"}),     keyed_list({"a","b","c"})));   // middle insert
    CHECK(roundtrip(keyed_list({"a","b"}),     keyed_list({"b","a","c"})));   // move + append
    CHECK(roundtrip(keyed_list({"x"}),         keyed_list({}) ));             // clear
    CHECK(roundtrip(keyed_list({}),            keyed_list({"x","y"}) ));      // populate

    std::cout << "test_diff_prop: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}

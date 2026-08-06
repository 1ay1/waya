/// examples/blog.cpp — "hypertext" : a big, nerdy, data-rich blog engine.
///
///   cmake --build build -j && ./build/blog     # http://localhost:8080
///
/// A whole content site in one file: a real router (/, /post/:slug, /tag/:tag,
/// /archive, /about), a seed corpus of nerdy posts with full metadata, LIVE
/// search with a result count, a featured hero post, tag filtering, a tag cloud,
/// monogram author avatars, per-post SEO (Open Graph + Article JSON-LD), reading
/// time, a scroll-driven reading-progress bar, an auto-generated table of
/// contents, related posts, and prev/next navigation. Every route
/// server-renders its own screen and is crawlable.

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/surface/router.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <string>
#include <variant>
#include <vector>

using namespace waya::surface;
using namespace waya::surface::color;

// ── data ──────────────────────────────────────────────────────────────────────
struct Post {
    std::string slug, title, excerpt, author, date;
    int minutes, likes;
    std::vector<std::string> tags;
    std::vector<std::pair<std::string,std::string>> body;  // (kind, text) kind: p|h|code|quote
};

static std::vector<Post> corpus() {
    auto P = [](std::string s, std::string t, std::string e, std::string a, std::string d,
                int m, int l, std::vector<std::string> tg,
                std::vector<std::pair<std::string,std::string>> b){
        return Post{std::move(s),std::move(t),std::move(e),std::move(a),std::move(d),m,l,std::move(tg),std::move(b)};
    };
    return {
        P("zero-cost-abstractions","Zero-cost abstractions are a lie (a good one)",
          "The compiler pays the abstraction tax at build time so your CPU never sees the bill \u2014 usually.",
          "Ada L.","2024-11-02",7,342,{"c++","performance","compilers"},{
            {"p","The promise of a zero-cost abstraction is that using it costs no more than the hand-written equivalent. In practice the cost moves \u2014 from runtime to compile time, from cycles to instruction-cache pressure."},
            {"h","Where the cost hides"},
            {"p","Inlining is the magician's hand. A std::sort with a lambda comparator specializes to a monomorphic call; qsort with a function pointer cannot. The abstraction is free at runtime precisely because it was expensive at build time."},
            {"code","template <class It, class Cmp>\nvoid isort(It a, It b, Cmp less) {\n  for (auto i = a; i != b; ++i)\n    std::rotate(std::upper_bound(a, i, *i, less), i, i+1);\n}"},
            {"quote","The fastest code is the code the optimizer can prove things about."},
            {"p","So it isn't free. It's prepaid. And the receipt is your compile time."}
        }),
        P("cache-oblivious","Cache-oblivious algorithms without tuning knobs",
          "Recursion that self-tunes to every level of the memory hierarchy at once, with no cache-size parameter.",
          "Grace H.","2024-10-18",9,521,{"algorithms","performance","memory"},{
            {"p","A cache-aware algorithm asks: how big is the cache? A cache-oblivious one refuses to ask, and wins on every machine anyway."},
            {"h","Divide until it fits"},
            {"p","Recursive subdivision means that at SOME level of recursion the working set fits in L1, at another L2, at another RAM \u2014 all without naming a single constant. The recursion tree does the tuning."},
            {"code","void mm(M a, M b, M c) {\n  if (small(a)) base(a,b,c);\n  else split(a,b,c, mm);  // recurse into quadrants\n}"},
            {"p","Van Emde Boas layouts, funnelsort, recursive matrix multiply \u2014 all the same trick."}
        }),
        P("lock-free-queues","The lock-free queue that ate my afternoon",
          "A single missing memory fence, a Tuesday, and the ABA problem staring back from the debugger.",
          "Alan T.","2024-09-30",11,287,{"concurrency","c++","atomics"},{
            {"p","Lock-free does not mean wait-free, and neither means bug-free. The Michael-Scott queue is elegant on paper and merciless in practice."},
            {"h","ABA: the ghost in the CAS"},
            {"p","A compare-and-swap sees the same pointer value and assumes nothing changed \u2014 but the node was freed, reallocated, and refilled. Same address, different soul. Tagged pointers or hazard pointers exorcise it."},
            {"code","// tag the pointer so recycled nodes differ\nstruct Tagged { Node* p; uint64_t tag; };\nwhile (!cas(&head, old, {next, old.tag+1})) old = head;"},
            {"quote","In concurrency, 'it works on my machine' means 'the race hasn't lost yet'."}
        }),
        P("type-state","Type-state: making illegal states unrepresentable",
          "Push your invariants into the type system and the compiler becomes your unit-test suite.",
          "Barbara L.","2024-09-12",6,198,{"types","c++","design"},{
            {"p","A File that might be open or closed forces a runtime check on every read. A OpenFile that is open by construction moves the check to compile time \u2014 forever."},
            {"code","struct Closed{}; struct Open{};\ntemplate<class S> struct File;\ntemplate<> struct File<Open> { Bytes read(); };\nFile<Open> open(Path);  // only path to a readable File"},
            {"p","This is what waya does with its DSL: a <td> outside a <tr> is not a runtime error, it is a program that does not compile."}
        }),
        P("branchless","Branchless code and the death of the if",
          "How to compute a conditional without asking the branch predictor's permission.",
          "Katherine J.","2024-08-25",8,445,{"performance","bit-tricks","cpu"},{
            {"p","A mispredicted branch costs ~15 cycles. On hot, unpredictable paths, replacing the branch with arithmetic can be a real win \u2014 and a real readability loss."},
            {"code","// max(a,b) with no branch\nint m = b ^ ((a ^ b) & -(a < b));"},
            {"quote","Premature branchlessness is the root of some evil, but the profiler is the root of the rest."},
            {"p","Measure. The compiler already does cmov for you more often than you think."}
        }),
        P("garbage-collection","Tri-color marking, explained with actual colors",
          "White, grey, black, and the invariant that keeps a concurrent collector from eating live objects.",
          "Dennis R.","2024-08-03",10,376,{"gc","runtimes","memory"},{
            {"p","White = condemned. Black = safe. Grey = reachable but not yet scanned. The collector's one job: never let a black object point at a white one without noticing."},
            {"h","The write barrier"},
            {"p","Mutators run concurrently, so a write of a white pointer into a black object must re-grey something. That's the whole ballgame."},
            {"code","void write(Obj* o, Field f, Obj* v) {\n  if (black(o) && white(v)) shade(v);  // barrier\n  o->f = v;\n}"}
        }),
        P("simd-json","Parsing JSON at 3 GB/s with SIMD",
          "Twelve bytes at a time, no character-by-character loop in sight, and a lot of pshufb.",
          "Margaret H.","2024-07-19",12,612,{"simd","parsing","performance"},{
            {"p","The trick to fast parsing is to stop parsing one byte at a time. simdjson classifies 64 bytes of input with a handful of vector instructions, building a bitmask of structural characters."},
            {"code","__m256i quotes = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\"'));\nuint32_t mask = _mm256_movemask_epi8(quotes);"},
            {"quote","The fastest loop is the one you turned into a bitmask."}
        }),
        P("coroutines","C++ coroutines: the state machine you didn't write",
          "co_await looks like magic; it compiles to a heap-allocated struct and a jump table you'd have hated to write by hand.",
          "Ada L.","2024-06-28",9,254,{"c++","async","coroutines"},{
            {"p","A coroutine is a function that can suspend and resume. The compiler transforms it into a state machine: locals become members of a frame, and co_await is a labeled resume point."},
            {"code","task<int> fetch() {\n  auto r = co_await get(url);\n  co_return parse(r);\n}"},
            {"p","The frame is (usually) heap-allocated unless the optimizer can prove it doesn't escape \u2014 HALO, the coroutine's inlining."}
        }),
        P("bloom-filters","Bloom filters: yes, no, and probably",
          "A set that answers 'definitely not' with certainty and 'maybe' with a tunable lie rate.",
          "Grace H.","2024-06-10",5,189,{"data-structures","probabilistic","hashing"},{
            {"p","k hash functions, one bit array. To insert, set k bits. To query, check k bits: any zero means definitely-absent; all ones means probably-present."},
            {"code","bool maybe(K x){ for(auto h:hashes) if(!bits[h(x)]) return false; return true; }"},
            {"quote","A Bloom filter never says no when it means yes. It only ever says yes when it means no."}
        }),
        P("virtual-memory","Virtual memory is the biggest lie your OS tells",
          "Every pointer you hold is fiction, translated by silicon you never see, backed by pages that may not exist yet.",
          "Alan T.","2024-05-22",8,401,{"os","memory","kernels"},{
            {"p","Your process believes it owns a flat 2^48-byte address space. It owns nothing. The MMU translates every access through page tables the kernel populates lazily, on fault."},
            {"h","Demand paging"},
            {"p","malloc doesn't allocate memory. It allocates the promise of memory. The first write to a page triggers a fault, and only then does a physical frame appear."},
            {"code","// this 'allocates' 1 GB instantly \u2014 it's all promises\nchar* p = mmap(0, 1<<30, PROT_RW, MAP_ANON|MAP_PRIVATE, -1, 0);"}
        }),
        P("rope-data-structure","Ropes: strings that don't fear the insert",
          "When you edit gigabytes of text, concatenation must be O(1), and a balanced tree of chunks obliges.",
          "Barbara L.","2024-05-01",6,167,{"data-structures","text-editors","trees"},{
            {"p","A flat string makes an insert at the front O(n). A rope makes it O(log n) by storing text as leaves of a balanced binary tree and concatenating with a new root."},
            {"code","Rope concat(Rope a, Rope b){ return node(a, b, weight(a)); }"},
            {"p","Every serious text editor \u2014 and every serious CRDT \u2014 hides a rope or a piece-table underneath."}
        }),
        P("entropy-coding","Arithmetic coding: squeezing below the byte",
          "Why Huffman leaves bits on the table and how to represent a whole message as a single fraction.",
          "Katherine J.","2024-04-14",10,233,{"compression","information-theory","math"},{
            {"p","Huffman codes are integer-length; a symbol with probability 0.9 still costs a whole bit. Arithmetic coding represents the entire message as one number in [0,1), spending fractional bits per symbol."},
            {"quote","Entropy is the floor. Everything above it is engineering; everything below it is a bug."},
            {"code","// narrow the interval by each symbol's probability\nlo += range * cdf(sym); range *= prob(sym);"}
        }),
    };
}

// ── app ────────────────────────────────────────────────────────────────────────
enum Screen { Feed, PostView, TagView, Archive, About, NotFound };

static Router routes() {
    return router().at("/", Feed).at("/post/:slug", PostView)
                   .at("/tag/:tag", TagView).at("/archive", Archive).at("/about", About);
}

struct Blog {
    struct Model {
        Screen screen = Feed;
        std::string slug, tag, query;
        std::vector<Post> posts = corpus();
    };
    struct SetQuery { std::string v; };
    struct Route { std::string path; };
    struct Nav { std::string path; };
    using Msg = std::variant<SetQuery, Route, Nav>;

    static Model init() { return {}; }

    static const char* site_url() { return "https://hypertext.example"; }
    static std::vector<std::string> sitemap() {
        std::vector<std::string> s{"/", "/archive", "/about"};
        for (auto& p : corpus()) s.push_back("/post/" + p.slug);
        return s;
    }

    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg, std::string value) {
        return std::visit(overload{
            [&](const SetQuery& q) -> std::pair<Model,Cmd<Msg>> { m.query = q.v; return { m, Cmd<Msg>::none() }; },
            [&](const Nav& n) -> std::pair<Model,Cmd<Msg>> { return { m, Cmd<Msg>::navigate(n.path) }; },
            [&](const Route& r) -> std::pair<Model,Cmd<Msg>> {
                // route path arrives as r.path (and mirrored in `value`)
                std::string path = r.path.empty() ? value : r.path;
                auto match = routes().match(path);
                m.screen = match.matched ? (Screen)match.value : NotFound;
                m.slug = match.param("slug"); m.tag = match.param("tag");
                m.query.clear();
                return { m, Cmd<Msg>::none() };
            },
        }, msg);
    }

    static Sub<Msg> subscribe(const Model&) {
        return Sub<Msg>::on_route([](std::string p){ return Route{p}; });
    }

    static const Post* find(const Model& m, const std::string& slug) {
        for (auto& p : m.posts) if (p.slug == slug) return &p;
        return nullptr;
    }

    // ── SEO ───────────────────────────────────────────────────────────────────
    static Meta meta(const Model& m) {
        std::string base = site_url();
        if (m.screen == PostView) if (auto* p = find(m, m.slug))
            return { .title = p->title + " \u00b7 hypertext", .description = p->excerpt,
                     .canonical = base + "/post/" + p->slug, .type = "article",
                     .site_name = "hypertext", .author = p->author,
                     .json_ld = jsonld("Article", {{"headline",p->title},{"author",p->author},
                                                    {"datePublished",p->date},{"description",p->excerpt}}) };
        if (m.screen == TagView)
            return { .title = "#" + m.tag + " · hypertext", .description = "All posts tagged #" + m.tag + ".",
                     .canonical = base + "/tag/" + m.tag };
        if (m.screen == Archive)
            return { .title = "Archive · hypertext", .description = "Every hypertext post, newest first.",
                     .canonical = base + "/archive" };
        if (m.screen == About)
            return { .title = "About · hypertext", .description = "A nerdy systems blog, one C++ file.",
                     .canonical = base + "/about" };
        return { .title = "hypertext — a nerdy systems blog",
                 .description = "Deep dives on compilers, concurrency, memory, and the algorithms underneath.",
                 .canonical = base + "/", .site_name = "hypertext",
                 .json_ld = jsonld("Blog", {{"name","hypertext"},{"url",base}}) };
    }

    // ── shared UI ──────────────────────────────────────────────────────────────

    // A deterministic accent colour per author, so avatars/labels feel personal.
    static std::uint32_t author_hue(const std::string& name) {
        std::uint32_t h = 2166136261u;
        for (char c : name) { h ^= (std::uint8_t)c; h *= 16777619u; }
        static const std::uint32_t palette[] = {
            0x818cf8, 0x22d3ee, 0xf472b6, 0x34d399, 0xfbbf24, 0xa78bfa, 0xfb7185, 0x60a5fa
        };
        return palette[h % (sizeof(palette)/sizeof(palette[0]))];
    }

    // A round monogram avatar (initials on a tinted disc) — zero images needed.
    static NodeRef avatar(const std::string& name, int px_=34) {
        std::string initials;
        bool take = true;
        for (char c : name) { if (c==' ') { take=true; continue; } if (take && initials.size()<2) { initials += (char)std::toupper((unsigned char)c); take=false; } }
        std::uint32_t c = author_hue(name);
        return stack(text(initials) | fg(0x0b0e18) | weight(Weight::black) | font(px_*0.42f))
             | css("width", std::to_string(px_)+"px") | css("height", std::to_string(px_)+"px")
             | round(999) | css("background", "linear-gradient(135deg," + hex(c) + "," + hex(c) + "aa)")
             | css("flex","0 0 auto") | css("place-items","center");
    }

    static std::string hex(std::uint32_t c) {
        char b[8]; std::snprintf(b, sizeof(b), "#%06x", c & 0xffffff); return b;
    }

    // A real <a href> anchor link (in-page nav to a heading, external URLs).
    static NodeRef alink(const std::string& label, const std::string& href) {
        return text(label) | as("a") | attr("href", href) | pointer
             | on(Hover, css("text-decoration","underline"));
    }

    // A reading-progress bar pinned to the very top that fills as you scroll the
    // page — pure CSS (scroll-driven animation), no JS. Degrades to hidden on
    // browsers without animation-timeline.
    static NodeRef reading_progress() {
        return markup(
            "<div style=\"position:fixed;top:0;left:0;right:0;height:3px;z-index:50;"
            "transform-origin:0 50%;background:linear-gradient(90deg,#818cf8,#22d3ee,#f472b6);"
            "animation:wa-blogscroll linear;animation-timeline:scroll(root);\"></div>"
            "<style>@keyframes wa-blogscroll{from{transform:scaleX(0)}to{transform:scaleX(1)}}</style>");
    }

    static NodeRef tag_chip(std::string t, bool active=false) {
        return link("#" + t) | caption | mono | pad_x(10) | pad_y(4) | round(999)
             | when_(active, tint(brand, 0.18f)) | when_(!active, tint(0xffffff, 0.05f))
             | hairline(0xffffff, 0.08f)
             | tap(Nav{"/"});
    }
    // a real navigating tag (uses browser nav via the anchor click → route)
    static NodeRef nav_tag(std::string t, bool active=false) {
        return text("#" + t) | fg(active ? brand2 : muted) | caption | mono
             | pad_x(10) | pad_y(4) | round(999)
             | when_(active, tint(brand, 0.18f)) | when_(!active, tint(0xffffff, 0.05f))
             | hairline(0xffffff, 0.08f) | interactive()
             | tap(Nav{"/tag/" + t});
    }

    static NodeRef nav_link(std::string label, std::string href, bool active) {
        return text(std::move(label)) | fg(active ? ink : muted) | semibold | font(15)
             | pad_x(10) | pad_y(6) | round(8) | hover_bg() | pointer | tap(Nav{href});
    }

    static NodeRef topbar(const Model& m) {
        return row(
            text("hypertext") | display | font(24) | weight(Weight::black)
                | aurora_text(0x818cf8, 0x22d3ee, 0xf472b6, 8) | pointer | tap(Nav{"/"}),
            push(),
            row(
                nav_link("Feed", "/", m.screen==Feed),
                nav_link("Archive", "/archive", m.screen==Archive),
                nav_link("About", "/about", m.screen==About)
            ) | gap(4) | center
        ) | pad_y(18) | center;
    }

    static NodeRef post_card(const Post& p) {
        std::vector<NodeRef> chips;
        for (auto& t : p.tags) chips.push_back(nav_tag(t));
        auto chiprow = box_(std::move(chips)); chiprow->style.flow = Flow::row;
        chiprow->style.wrap = Wrap::wrap; finalize(*chiprow); chiprow = chiprow | gap(6);
        return col(
            row(avatar(p.author, 28),
                col(text(p.author) | fg(ink) | caption | semibold,
                    row(text(p.date) | fg(faint) | caption | mono, text("\u00b7") | fg(faint),
                        text(std::to_string(p.minutes) + " min") | fg(faint) | caption) | gap(6) | center
                ) | gap(1),
                push(),
                row(text("\u2661") | fg(0xf472b6), text(std::to_string(p.likes)) | fg(muted) | caption) | gap(4) | center
            ) | gap(9) | center,
            text(p.title) | fg(ink) | subtitle | font(22) | weight(Weight::bold) | leading(1.25f),
            text(p.excerpt) | fg(muted) | body | leading(1.6f),
            push(),
            chiprow
        ) | gap(12) | pad(22) | round(18)
          | tint(0xffffff, 0.025f) | hairline(0xffffff, 0.07f)
          | hover_lift(3) | pointer | tap(Nav{"/post/" + p.slug});
    }

    // A large, editorial "featured" card for the newest post at the top of feed.
    static NodeRef featured_card(const Post& p) {
        std::vector<NodeRef> chips;
        for (auto& t : p.tags) chips.push_back(nav_tag(t));
        auto chiprow = box_(std::move(chips)); chiprow->style.flow = Flow::row;
        chiprow->style.wrap = Wrap::wrap; finalize(*chiprow); chiprow = chiprow | gap(6);
        return col(
            row(text("FEATURED") | fg(brand2) | caption | mono | weight(Weight::black)
                    | css("letter-spacing",".2em"),
                push(),
                row(text("\u2661") | fg(0xf472b6), text(std::to_string(p.likes)) | fg(muted) | caption) | gap(4) | center
            ) | center,
            text(p.title) | fg(ink) | display | font_fluid(26, 40) | weight(Weight::black)
                | leading(1.1f) | css("letter-spacing","-.02em"),
            text(p.excerpt) | fg(0xcbd5e1) | body | font(18) | leading(1.6f) | css("max-width","46rem"),
            row(avatar(p.author, 32),
                col(text(p.author) | fg(ink) | caption | semibold,
                    row(text(p.date) | fg(faint) | caption | mono, text("\u00b7") | fg(faint),
                        text(std::to_string(p.minutes) + " min read") | fg(faint) | caption) | gap(6) | center
                ) | gap(1),
                push(),
                text("Read \u2192") | fg(brand2) | semibold | font(15)
            ) | gap(10) | center,
            (chiprow | gap(6))
        ) | gap(16) | pad_fluid(22, 34) | round(22)
          | css("background", "linear-gradient(135deg, rgba(99,102,241,.14), rgba(34,211,238,.06))")
          | hairline(0xffffff, 0.10f) | hover_lift(3) | pointer | tap(Nav{"/post/" + p.slug});
    }

    // ── screens ─────────────────────────────────────────────────────────
    static NodeRef feed(const Model& m) {
        // live search filter
        std::string q = m.query; for (auto& c : q) c = std::tolower(c);
        auto matches = [&](const Post& p){
            if (q.empty()) return true;
            auto low = [](std::string s){ for(auto&c:s)c=std::tolower(c); return s; };
            std::string hay = low(p.title) + " " + low(p.excerpt);
            for (auto& t : p.tags) hay += " " + t;
            return hay.find(q) != std::string::npos;
        };
        bool searching = !q.empty();
        std::vector<const Post*> hits;
        for (auto& p : m.posts) if (matches(p)) hits.push_back(&p);

        // When not searching, promote the newest post to a featured hero and grid
        // the rest. When searching, just grid the matches.
        const Post* featured = (!searching && !hits.empty()) ? hits.front() : nullptr;
        std::vector<NodeRef> cards;
        for (std::size_t i = 0; i < hits.size(); ++i) {
            if (featured && hits[i] == featured) continue;
            cards.push_back(post_card(*hits[i]));
        }
        auto grid = box_(std::move(cards));
        grid->style.extra.emplace_back("display","grid");
        grid->style.extra.emplace_back("grid-template-columns","repeat(auto-fill,minmax(min(22rem,100%),1fr))");
        grid->style.extra.emplace_back("grid-auto-rows","1fr");
        grid->style.gap = {20,Unit::px}; finalize(*grid);

        auto search = input(m.query)
            | placeholder("search " + std::to_string(m.posts.size()) + " posts\u2026")
            | on_input([](std::string v){ return SetQuery{v}; })
            | fg(ink) | tint(0xffffff, 0.04f) | hairline(0xffffff, 0.10f)
            | pad_x(16) | pad_y(12) | round(12) | font(15) | css("width","100%");

        std::string countline = searching
            ? (std::to_string(hits.size()) + (hits.size()==1 ? " result" : " results") + " for \u201c" + m.query + "\u201d")
            : (std::to_string(m.posts.size()) + " essays on the machine underneath the machine");

        return col(
            col(
                text("Notes from the machine") | display | font_fluid(30, 46) | weight(Weight::black)
                    | aurora_text(0x818cf8, 0x22d3ee, 0xf472b6, 9),
                text("Compilers, concurrency, memory, and the algorithms underneath.")
                    | fg(muted) | body
            ) | gap(8) | fade_up(400),
            search | fade_up(500) | delay(60),
            text(countline) | fg(faint) | caption | mono,
            (hits.empty()
                ? (col(
                      text("\U0001f50d") | font(40) | center,
                      text("no posts match \u201c" + m.query + "\u201d") | fg(muted) | body | center,
                      link("clear search") | caption | center | tap(Nav{"/"})
                   ) | gap(10) | pad_y(48) | center)
                : col(
                      (featured ? (featured_card(*featured) | fade_up(560) | delay(90)) : fragment({})),
                      grid | fade_up(620) | delay(140)
                  ) | gap(20))
        ) | gap(24);
    }

    static NodeRef code_block(std::string code) {
        // escape + render as a real <pre><code> via markup, styled dark
        std::string esc; for (char c : code){ if(c=='<')esc+="&lt;"; else if(c=='>')esc+="&gt;"; else if(c=='&')esc+="&amp;"; else esc+=c; }
        return markup("<pre style=\"margin:0;overflow-x:auto\"><code>" + esc + "</code></pre>")
             | tint(0x000000, 0.35f) | hairline(0xffffff, 0.08f) | round(12) | pad(18)
             | mono | fg(0xcbd5e1) | font(13) | css("line-height","1.6");
    }

    static NodeRef post_view(const Model& m) {
        auto* p = find(m, m.slug);
        if (!p) return not_found();

        // Body + collect headings for a table of contents.
        std::vector<NodeRef> blocks;
        std::vector<std::pair<std::string,std::string>> toc; // (anchor, label)
        int hi = 0;
        for (auto& [kind, txt] : p->body) {
            if (kind == "h") {
                std::string anchor = "h" + std::to_string(hi++);
                toc.emplace_back(anchor, txt);
                blocks.push_back(text(txt) | fg(ink) | heading | font(25) | as("h2")
                    | attr("id", anchor) | pad_y(4) | css("scroll-margin-top","84px"));
            }
            else if (kind=="code") blocks.push_back(code_block(txt));
            else if (kind=="quote") blocks.push_back(
                text(txt) | fg(0xc7d2fe) | subtitle | italic | as("blockquote")
                    | css("border-left","3px solid #6366f1") | pad_x(18) | pad_y(4) | leading(1.6f));
            else blocks.push_back(text(txt) | fg(0xcbd5e1) | body_txt() | font(18) | leading(1.8f) | as("p"));
        }
        std::vector<NodeRef> chips; for (auto& t : p->tags) chips.push_back(nav_tag(t));
        auto chiprow = box_(std::move(chips)); chiprow->style.flow=Flow::row; chiprow->style.wrap=Wrap::wrap; finalize(*chiprow);

        // Table of contents (only if there are 2+ headings).
        NodeRef toc_box = fragment({});
        if (toc.size() >= 2) {
            std::vector<NodeRef> links;
            links.push_back(text("ON THIS PAGE") | fg(faint) | caption | mono
                | weight(Weight::black) | css("letter-spacing",".16em"));
            for (auto& [anchor, label] : toc)
                links.push_back(alink(label, "#" + anchor) | fg(muted) | caption
                    | css("text-decoration","none"));
            toc_box = col_(std::move(links)) | gap(9) | pad(18) | round(14)
                | tint(0xffffff, 0.03f) | hairline(0xffffff, 0.07f);
        }

        auto article = col(
            link("\u2190 all posts") | caption | tap(Nav{"/"}),
            (chiprow | gap(6)),
            text(p->title) | fg(ink) | display | font_fluid(30, 50) | weight(Weight::black)
                | css("letter-spacing","-.02em") | leading(1.08f),
            text(p->excerpt) | fg(0x94a3b8) | subtitle | font(20) | leading(1.5f),
            row(avatar(p->author, 40),
                col(text(p->author) | fg(ink) | body | semibold,
                    row(text(p->date) | fg(muted) | caption | mono, text("\u00b7") | fg(faint),
                        text(std::to_string(p->minutes) + " min read") | fg(muted) | caption) | gap(6) | center
                ) | gap(2),
                push(),
                row(text("\u2661") | fg(0xf472b6) | font(18),
                    text(std::to_string(p->likes)) | fg(muted) | caption) | gap(5) | center
            ) | gap(12) | center | pad_y(4),
            divider(),
            toc_box,
            col_(std::move(blocks)) | gap(22),
            divider(),
            post_footer(m, *p)
        ) | gap(18) | as_article;
        return fragment({reading_progress(), centered(46, article)});
    }

    // End-of-post: share of related posts (share a tag) + prev/next nav.
    static NodeRef post_footer(const Model& m, const Post& p) {
        // sort chronologically to find neighbours
        auto sorted = m.posts;
        std::sort(sorted.begin(), sorted.end(), [](const Post&a,const Post&b){ return a.date>b.date; });
        const Post* prev=nullptr; const Post* next=nullptr;
        for (std::size_t i=0;i<sorted.size();++i) if (sorted[i].slug==p.slug){
            if (i>0) next=&sorted[i-1];              // newer
            if (i+1<sorted.size()) prev=&sorted[i+1]; // older
            break;
        }
        auto navcell = [&](const char* label, const Post* q, bool right)->NodeRef{
            if (!q) return box() | grow(1);
            auto inner = col(
                text(label) | fg(faint) | caption | mono,
                text(q->title) | fg(ink) | body | semibold | leading(1.3f)
            ) | gap(4);
            if (right) inner = inner | css("text-align","right") | css("align-items","flex-end");
            return inner | grow(1) | pad(16) | round(14)
                 | tint(0xffffff, 0.03f) | hairline(0xffffff, 0.07f)
                 | hover_lift(2) | pointer | tap(Nav{"/post/" + q->slug});
        };

        // related: other posts sharing at least one tag, up to 3
        std::vector<NodeRef> rel;
        for (auto& o : m.posts) {
            if (o.slug==p.slug) continue;
            bool shares=false; for (auto& t:o.tags) for (auto& u:p.tags) if (t==u) shares=true;
            if (shares) rel.push_back(post_card(o));
            if (rel.size()>=3) break;
        }

        std::vector<NodeRef> out;
        out.push_back(row(navcell("\u2190 Older", prev, false), navcell("Newer \u2192", next, true)) | gap(14) | wrap);
        if (!rel.empty()) {
            auto grid = box_(std::move(rel));
            grid->style.extra.emplace_back("display","grid");
            grid->style.extra.emplace_back("grid-template-columns","repeat(auto-fill,minmax(min(18rem,100%),1fr))");
            grid->style.gap = {16,Unit::px}; finalize(*grid);
            out.push_back(text("Related reading") | fg(ink) | heading | font(22) | pad_y(4));
            out.push_back(grid);
        }
        return col_(std::move(out)) | gap(20);
    }

    static NodeRef archive(const Model& m) {
        // group by year, plus a tag cloud
        std::vector<NodeRef> rows;
        auto sorted = m.posts;
        std::sort(sorted.begin(), sorted.end(), [](const Post&a,const Post&b){ return a.date>b.date; });
        for (auto& p : sorted)
            rows.push_back(row(
                text(p.date) | fg(faint) | caption | mono | css("min-width","6rem"),
                text(p.title) | fg(ink) | body | grow(1) | pointer | tap(Nav{"/post/"+p.slug}),
                text(std::to_string(p.minutes)+"m") | fg(muted) | caption
            ) | gap(14) | center | pad_y(10) | hairline_bottom());
        // tag cloud
        std::vector<std::string> tags;
        for (auto& p : m.posts) for (auto& t : p.tags) if (std::find(tags.begin(),tags.end(),t)==tags.end()) tags.push_back(t);
        std::vector<NodeRef> cloud; for (auto& t : tags) cloud.push_back(nav_tag(t));
        auto cloudrow = box_(std::move(cloud)); cloudrow->style.flow=Flow::row; cloudrow->style.wrap=Wrap::wrap; finalize(*cloudrow);
        return col(
            text("Archive") | fg(ink) | display | font_fluid(30,46) | weight(Weight::black),
            text("Every post, newest first.") | fg(muted) | body,
            (cloudrow | gap(8)),
            divider(),
            col_(std::move(rows))
        ) | gap(18);
    }

    static NodeRef tag_view(const Model& m) {
        std::vector<NodeRef> cards;
        for (auto& p : m.posts)
            if (std::find(p.tags.begin(),p.tags.end(),m.tag)!=p.tags.end())
                cards.push_back(post_card(p));
        auto grid = box_(std::move(cards));
        grid->style.extra.emplace_back("display","grid");
        grid->style.extra.emplace_back("grid-template-columns","repeat(auto-fill,minmax(min(22rem,100%),1fr))");
        grid->style.gap={20,Unit::px}; finalize(*grid);
        return col(
            row(link("\u2190 back")|caption|tap(Nav{"/"}), push()) ,
            text("#" + m.tag) | fg(ink) | display | font_fluid(30,46) | mono | weight(Weight::black)
                | glow_text(brand, 16),
            text(std::to_string((int)cards.size()) + " posts") | fg(muted) | body,
            grid
        ) | gap(18);
    }

    static NodeRef about() {
        return centered(42, col(
            text("About hypertext") | fg(ink) | display | font_fluid(30,46) | weight(Weight::black),
            text("A blog about the machine underneath the machine \u2014 written to be read, and "
                 "built to prove a point: this whole site is one C++ file, server-rendered, "
                 "typed end to end, with zero HTML or CSS in the source.")
                | fg(0xcbd5e1) | body_txt() | leading(1.8f),
            divider(),
            text("Built with waya \u00b7 SSR \u00b7 keyed diffing \u00b7 a real router \u00b7 per-post SEO")
                | fg(muted) | caption
        ) | gap(18) | as_article);
    }

    static NodeRef not_found() {
        return centered(40, col(
            text("404") | display | font(88) | weight(Weight::black) | aurora_text(0xf472b6,0x818cf8,0x22d3ee),
            text("that page drifted out of the cache.") | fg(muted) | body,
            link("\u2190 home") | tap(Nav{"/"})
        ) | gap(14) | center);
    }

    // small helpers
    static Mod body_txt(){ return font(17); }
    static Mod hairline_bottom(){ return css("border-bottom","1px solid rgba(255,255,255,.06)"); }

    static NodeRef view(const Model& m) {
        auto body = screens((int)m.screen, {
            {Feed,     [&]{ return feed(m); }},
            {PostView, [&]{ return post_view(m); }},
            {TagView,  [&]{ return tag_view(m); }},
            {Archive,  [&]{ return archive(m); }},
            {About,    [&]{ return about(); }},
            {NotFound, [&]{ return not_found(); }},
        });
        return page(0x080a12,
            centered(72, col(topbar(m), col(body) | as_main, footer()) | gap(24))
        ) | mesh(0x6366f1, 0x8b5cf6, 0x080a12) | theme(Theme::midnight())
          ;
    }

    static NodeRef footer() {
        return row(text("\u00a9 2024 hypertext") | fg(faint) | caption,
                   push(),
                   text("rendered in C++ \u00b7 no CSS was harmed") | fg(faint) | caption)
             | pad_y(28) | wrap | center | css("border-top","1px solid rgba(255,255,255,.06)");
    }
};

int main() {
    static_assert(SurfaceProgram<Blog>);
    return live<Blog>({ .port = 8080, .page_bg = 0x080a12, .title = "hypertext" });
}

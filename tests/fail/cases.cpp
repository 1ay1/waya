/// tests/fail/cases.cpp — one file, many "must not compile" cases.
///
/// CMake compiles this once per case with -DWAYA_CASE=<n> and asserts each
/// build FAILS (WILL_FAIL). This is the golden-error surface: every invariant
/// in DESIGN §3.3 / §5.5 gets a case here.

#include <waya/waya.hpp>

#include <vector>

using namespace waya::dsl;
using namespace waya::style;
using namespace waya::style::literals;

int main() {
#if   WAYA_CASE == 1
    auto x = p_(div_());                              // <div> inside <p>
#elif WAYA_CASE == 2
    auto x = div_(td_());                             // <td> outside <tr>
#elif WAYA_CASE == 3
    auto x = div_(li_());                             // <li> outside a list
#elif WAYA_CASE == 4
    auto x = ul_(div_());                             // <div> inside <ul>
#elif WAYA_CASE == 5
    auto x = head_(p_());                             // <p> inside <head>
#elif WAYA_CASE == 6
    auto x = title_(div_());                          // <div> inside <title>
#elif WAYA_CASE == 7
    auto x = br_(text("no"));                         // children on void <br>
#elif WAYA_CASE == 8
    auto x = img_(text("no"));                        // children on void <img>
#elif WAYA_CASE == 9
    auto x = span_(text("s")) | href<"/u">;           // href on <span>
#elif WAYA_CASE == 10
    auto x = div_() | href<"/u">;                     // href on <div>
#elif WAYA_CASE == 11
    auto x = p_(table_());                            // <table> inside <p>
#elif WAYA_CASE == 12
    auto x = p_(h1_());                               // heading inside <p>
#elif WAYA_CASE == 13
    auto x = div_(text("x")) | gap(8_px);             // gap without a container
#elif WAYA_CASE == 14
    auto x = div_(text("x")) | justify(Justify::center); // justify without a container
#elif WAYA_CASE == 15
    auto x = select_(div_());                         // <div> inside <select>
#elif WAYA_CASE == 16
    // each produces <tr>, but <div> permits flow content, not <tr>
    std::vector<int> v{1};
    auto x = div_(each(v, [](int){ return tr_(td_(text("c"))); }));
#elif WAYA_CASE == 17
    // when's two branches must share a content category
    auto x = div_(when(true, p_(text("a")), td_(text("b"))));
#else
#   error "no WAYA_CASE selected"
#endif
    (void)x;
    return 0;
}

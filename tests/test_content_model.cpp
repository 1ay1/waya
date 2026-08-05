/// tests/test_content_model.cpp — positive content-model + type-state cases.
/// The NEGATIVE cases (things that must NOT compile) live in tests/fail/ and are
/// driven by CMake's WILL_FAIL compile tests — see tests/CMakeLists.txt.

#include <waya/waya.hpp>

#include <iostream>

using namespace waya::dsl;
using namespace waya::html;
using namespace waya::style;
using namespace waya::style::literals;

int main() {
    // The content model accepts valid nesting.
    static_assert( PermittedChild<Tag::p,   TextNode>);
    static_assert(!PermittedChild<Tag::p,   decltype(div_())>);
    static_assert( PermittedChild<Tag::div, decltype(p_())>);
    static_assert( PermittedChild<Tag::tr,  decltype(td_())>);
    static_assert(!PermittedChild<Tag::div, decltype(td_())>);
    static_assert( PermittedChild<Tag::ul,  decltype(li_())>);
    static_assert(!PermittedChild<Tag::ul,  decltype(div_())>);
    static_assert(!PermittedChild<Tag::head,decltype(p_())>);
    static_assert( PermittedChild<Tag::select, decltype(option_())>);
    static_assert( PermittedChild<Tag::dl,  decltype(dt_())>);

    // Valid documents build.
    auto ok1 = table_(thead_(tr_(th_(text("H")))), tbody_(tr_(td_(text("c")))));
    auto ok2 = ul_(li_(text("a")), li_(text("b")));
    auto ok3 = form_(label_(text("Name")), input_());
    (void)ok1; (void)ok2; (void)ok3;

    // Styling type-state: gap AFTER a container token is fine.
    auto styled = div_(text("x")) | row | gap(8_px) | justify(Justify::center);
    (void)styled;

    std::cout << "test_content_model: all static_asserts passed\n";
    return 0;
}

/// examples/media.cpp — every CAPABILITY element, one line each. Video with
/// options, a YouTube embed, a live map, inline SVG art, and a canvas — the
/// pieces you "can't build from a box" — all first-class and dead-simple. No
/// HTML, no CSS, no JavaScript.
///
///   waya run media

#include <waya/surface/live.hpp>
#include <waya/surface/media.hpp>

#include <string>
#include <variant>

using namespace waya::surface;
using namespace waya::surface::color;

struct Media {
    struct Model {};
    using Msg = std::variant<std::monostate>;
    static Model init(){ return {}; }
    static Model update(Model m, Msg){ return m; }

    // Every tile is the same fixed-height frame. `mode` decides how the body sits:
    //  "fill"   — the body stretches edge to edge (video, embed, image, canvas)
    //  "center" — the body is centered art at its own size (an icon)
    static NodeRef tile(std::string label, NodeRef body, std::string mode = "fill"){
        auto frame = box(std::move(body))
            | css("height","220px") | css("width","100%")
            | round(14) | clip
            | css("border","1px solid rgba(255,255,255,.08)")
            | css("background","rgba(255,255,255,.02)");
        if(mode == "center") frame = frame | dead_center;
        else                 frame = frame | css("display","flex");   // child flex-fills
        return col(
            text(label) | fg(muted) | font(13) | weight(Weight::semibold)
                | css("text-transform","uppercase") | css("letter-spacing",".08em"),
            frame
        ) | gap(10);
    }

    // A media element that covers its (flex) tile edge to edge.
    static NodeRef cover_el(NodeRef n){
        return std::move(n)
            | css("width","100%") | css("height","100%")
            | css("object-fit","cover") | css("display","block") | css("flex","1");
    }

    static NodeRef view(const Model&){
        // A heart, drawn as inline SVG, tinted by fg() — an icon at its own size.
        auto heart = svg("<path fill='currentColor' d='M12 21s-7-4.6-9.5-8.3C.6 9.6 2 6 5.2 6 7 6 8.4 7 12 10.5 15.6 7 17 6 18.8 6 22 6 23.4 9.6 21.5 12.7 19 16.4 12 21 12 21z'/>")
                   | fg(0xff5c8a) | size(px(84));

        return centered(58, col(
            col(
                text("Media & embeds") | fg(ink) | font_fluid(30, 46) | weight(Weight::black),
                text("Video, YouTube, maps, SVG, canvas \u2014 first-class, one line each.")
                    | fg(muted) | body
            ) | gap(8) | pad_y(8),

            grid(rem(20),
                tile("Video \u00b7 autoplay loop muted",
                     cover_el(
                     video("https://test-videos.co.uk/vids/bigbuckbunny/mp4/h264/360/Big_Buck_Bunny_360_10s_1MB.mp4")
                        | autoplay() | loop_media() | silent() | plays_inline() | no_controls()), "fill"),

                tile("YouTube \u00b7 just the id", youtube("aqz-KE-bpKQ"), "fill"),

                tile("Live map \u00b7 a query", google_map("Golden Gate Bridge"), "fill"),

                tile("Inline SVG \u00b7 tinted by fg()",
                     box(heart) | dead_center
                        | css("width","100%") | css("height","100%")
                        | css("background","radial-gradient(circle at 50% 45%, rgba(255,92,138,.16), transparent 62%)"),
                     "center"),

                tile("Canvas \u00b7 a real drawable element",
                     box(canvas(600, 340) | css("width","100%") | css("height","100%") | css("display","block"))
                        | css("flex","1") | css("width","100%") | css("height","100%")
                        | css("background","repeating-linear-gradient(45deg,#141b28,#141b28 10px,#171f2e 10px,#171f2e 20px)"),
                     "fill"),

                tile("Picture \u00b7 responsive sources",
                     cover_el(image("https://picsum.photos/600/340") | alt("A random photo")), "fill")
            ) | gap(20)
        ) | gap(28)) | pad_fluid(16, 40)
          | mesh(0x7c8cff, 0x22d3ee, 0x0a0c14)
          | css("min-height","100vh") | as_main;
    }

    static Meta meta(const Model&){
        Meta mt;
        mt.title = "Media & embeds \u00b7 waya";
        mt.description = "Video, YouTube, live maps, inline SVG, and canvas \u2014 every capability "
                         "element as a one-line builder. Pure C++, no HTML/CSS/JS.";
        return mt;
    }
};

int main(){ return live<Media>({ .port = 8080, .page_bg = 0x0a0c14, .title = "Media" }); }

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

    static NodeRef tile(std::string label, NodeRef body){
        return col(
            text(label) | fg(muted) | font(13) | weight(Weight::semibold)
                | css("text-transform","uppercase") | css("letter-spacing",".08em"),
            video_box(std::move(body)) | round(14) | clip
                | css("border","1px solid rgba(255,255,255,.08)")
        ) | gap(10);
    }

    static NodeRef view(const Model&){
        // A heart, drawn as inline SVG, tinted by fg().
        auto heart = svg("<path fill='currentColor' d='M12 21s-7-4.6-9.5-8.3C.6 9.6 2 6 5.2 6 7 6 8.4 7 12 10.5 15.6 7 17 6 18.8 6 22 6 23.4 9.6 21.5 12.7 19 16.4 12 21 12 21z'/>")
                   | fg(0xff5c8a) | size(px(72));

        return centered(58, col(
            col(
                text("Media & embeds") | fg(ink) | font_fluid(30, 46) | weight(Weight::black),
                text("Video, YouTube, maps, SVG, canvas \u2014 first-class, one line each.")
                    | fg(muted) | body
            ) | gap(8) | pad_y(8),

            grid(rem(20),
                tile("Video \u00b7 autoplay loop muted",
                     video("https://test-videos.co.uk/vids/bigbuckbunny/mp4/h264/360/Big_Buck_Bunny_360_10s_1MB.mp4")
                        | autoplay() | loop_media() | silent() | plays_inline() | no_controls()
                        | css("width","100%") | css("height","100%") | css("object-fit","cover")),

                tile("YouTube \u00b7 just the id", youtube("aqz-KE-bpKQ")),

                tile("Live map \u00b7 a query", google_map("Golden Gate Bridge")),

                tile("Inline SVG \u00b7 tinted by fg()",
                     box(heart) | dead_center
                        | css("background","radial-gradient(circle at 50% 40%, rgba(255,92,138,.14), transparent 60%)")),

                tile("Canvas \u00b7 a real drawable element",
                     box(canvas(600, 340) | css("width","100%") | css("height","100%"))
                        | css("background","repeating-linear-gradient(45deg,#141b28,#141b28 10px,#171f2e 10px,#171f2e 20px)")),

                tile("Picture \u00b7 responsive sources",
                     picture("https://picsum.photos/600/340",
                             { {"(max-width:600px)","https://picsum.photos/400/300"} },
                             "A random photo")
                        | css("width","100%") | css("height","100%"))
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

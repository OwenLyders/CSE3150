#include "Renderer.h"

#include "core/Format.h"

#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_ttf.h>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Colors
// ---------------------------------------------------------------------------

ALLEGRO_COLOR rgb(unsigned hex) {
    return al_map_rgb(static_cast<unsigned char>(hex >> 16), static_cast<unsigned char>(hex >> 8),
                      static_cast<unsigned char>(hex));
}

// Allegro blends with premultiplied alpha, so a see-through color scales r, g and b too.
ALLEGRO_COLOR translucent(ALLEGRO_COLOR color, float alpha) {
    float r = 0, g = 0, b = 0, a = 0;
    al_unmap_rgba_f(color, &r, &g, &b, &a);
    return al_map_rgba_f(r * alpha, g * alpha, b * alpha, alpha);
}

struct Palette {
    ALLEGRO_COLOR background = rgb(0x0F1117);
    ALLEGRO_COLOR panel = rgb(0x171A21);
    ALLEGRO_COLOR border = rgb(0x2A2F3A);
    ALLEGRO_COLOR text = rgb(0xE6E9EF);
    ALLEGRO_COLOR dim = rgb(0x8B93A5);
    ALLEGRO_COLOR faint = rgb(0x4B5263);
    ALLEGRO_COLOR emptyLine = rgb(0x1B1F27);
    ALLEGRO_COLOR validLine = rgb(0x242A38);
    ALLEGRO_COLOR highlight = rgb(0x282C42);
    ALLEGRO_COLOR hit = rgb(0x3FB950);
    ALLEGRO_COLOR miss = rgb(0xF85149);
    ALLEGRO_COLOR evict = rgb(0xE3B341);   // amber: the block just evicted, and the next victim
    ALLEGRO_COLOR tag = rgb(0x58A6FF);     // Each address field has one color, used both for its
    ALLEGRO_COLOR index = rgb(0xC297FF);   // bits in the top bar and for the matching part of the
    ALLEGRO_COLOR offset = rgb(0x39D3C3);  // grid: tags, the lit set, the byte at the offset.
    ALLEGRO_COLOR dirty = rgb(0xFF7EB6);
};

const Palette& palette() {
    static const Palette colors;  // made on first use, which is after al_init()
    return colors;
}

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

float toFloat(std::size_t n) {
    return static_cast<float>(n);
}

void drawText(const ALLEGRO_FONT* font, ALLEGRO_COLOR color, float x, float y, const std::string& text,
              int align = ALLEGRO_ALIGN_LEFT) {
    al_draw_text(font, color, x, y, align, text.c_str());
}

float textWidth(const ALLEGRO_FONT* font, const std::string& text) {
    return static_cast<float>(al_get_text_width(font, text.c_str()));
}

float lineHeight(const ALLEGRO_FONT* font) {
    return static_cast<float>(al_get_font_line_height(font));
}

// A run of text in one color, for lines that mix colors.
struct Piece {
    std::string text;
    ALLEGRO_COLOR color;
};

float drawPieces(const ALLEGRO_FONT* font, float x, float y, const std::vector<Piece>& pieces) {
    for (const Piece& piece : pieces) {
        drawText(font, piece.color, x, y, piece.text);
        x += textWidth(font, piece.text);
    }
    return x;
}

void drawPanel(float x, float y, float w, float h) {
    al_draw_filled_rounded_rectangle(x, y, x + w, y + h, 6, 6, palette().panel);
    al_draw_rounded_rectangle(x + 0.5f, y + 0.5f, x + w - 0.5f, y + h - 0.5f, 6, 6, palette().border, 1);
}

void drawDashedLine(float x1, float y1, float x2, float y2, ALLEGRO_COLOR color, float thickness) {
    const float dash = 5;
    const float gap = 4;
    const float length = std::hypot(x2 - x1, y2 - y1);
    if (length <= 0) return;
    const float dx = (x2 - x1) / length;
    const float dy = (y2 - y1) / length;
    for (float start = 0; start < length; start += dash + gap) {
        const float end = std::min(start + dash, length);
        al_draw_line(x1 + dx * start, y1 + dy * start, x1 + dx * end, y1 + dy * end, color, thickness);
    }
}

void drawDashedRect(float x1, float y1, float x2, float y2, ALLEGRO_COLOR color, float thickness) {
    drawDashedLine(x1, y1, x2, y1, color, thickness);
    drawDashedLine(x2, y1, x2, y2, color, thickness);
    drawDashedLine(x2, y2, x1, y2, color, thickness);
    drawDashedLine(x1, y2, x1, y1, color, thickness);
}

// Splits text into lines no wider than maxWidth, breaking between words.
std::vector<std::string> wrapText(const ALLEGRO_FONT* font, const std::string& text, float maxWidth) {
    std::vector<std::string> lines;
    std::istringstream words(text);
    std::string word;
    std::string line;
    while (words >> word) {
        const std::string longer = line.empty() ? word : line + " " + word;
        if (!line.empty() && textWidth(font, longer) > maxWidth) {
            lines.push_back(line);
            line = word;
        } else {
            line = longer;
        }
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

// The text, cut short with "..." if it is wider than maxWidth.
std::string fitText(const ALLEGRO_FONT* font, std::string text, float maxWidth) {
    if (textWidth(font, text) <= maxWidth) return text;
    while (!text.empty() && textWidth(font, text + "...") > maxWidth) text.pop_back();
    return text + "...";
}

std::string speedText(double stepsPerSecond) {
    std::ostringstream out;
    out << stepsPerSecond << "/s";
    return out.str();
}

// ---------------------------------------------------------------------------
// What to highlight
// ---------------------------------------------------------------------------

// The request that the top bar describes and the grids light up: the one just processed or,
// before the first step, the one about to be processed.
struct Focus {
    std::optional<std::size_t> request;
    bool processed = false;
};

Focus focusOf(const Simulator& sim) {
    Focus focus;
    if (sim.position() > 0) {
        focus.request = sim.position() - 1;
        focus.processed = true;
    } else if (!sim.trace().empty()) {
        focus.request = 0;
    }
    return focus;
}

// How strongly the touched line still flashes: 0.55 right after a step, fading to 0.
float flashStrength(const App& app) {
    const double duration = app.playing() ? std::min(0.6, 0.9 / app.stepsPerSecond()) : 0.6;
    const double t = app.secondsSinceStep() / duration;
    return t >= 1.0 ? 0.0f : static_cast<float>(0.55 * (1.0 - t));
}

}  // namespace

// ---------------------------------------------------------------------------
// Renderer
// ---------------------------------------------------------------------------

Renderer::Renderer(const std::string& fontPath) {
    small_ = al_load_ttf_font(fontPath.c_str(), 12, 0);
    body_ = al_load_ttf_font(fontPath.c_str(), 14, 0);
    large_ = al_load_ttf_font(fontPath.c_str(), 22, 0);
    if (!small_ || !body_ || !large_) {
        for (ALLEGRO_FONT* font : {small_, body_, large_}) {
            if (font) al_destroy_font(font);
        }
        small_ = al_create_builtin_font();
        body_ = al_create_builtin_font();
        large_ = al_create_builtin_font();
        fallback_ = true;
    }
}

Renderer::~Renderer() {
    for (ALLEGRO_FONT* font : {small_, body_, large_}) {
        if (font) al_destroy_font(font);
    }
}

void Renderer::draw(const App& app) const {
    al_clear_to_color(palette().background);

    const float margin = 16;
    const float gap = 12;
    const float barHeight = 96;
    const float statsHeight = 84;
    const float traceWidth = 268;
    const float middleY = margin + barHeight + gap;
    const float middleHeight = kHeight - margin - statsHeight - gap - middleY;

    drawRequestBar(app, {margin, margin, kWidth - 2 * margin, barHeight});
    drawTrace(app, {margin, middleY, traceWidth, middleHeight});

    const std::size_t caches = app.simulator().cacheCount();
    const float gridsX = margin + traceWidth + gap;
    const float gridWidth = (kWidth - margin - gridsX - gap * toFloat(caches - 1)) / toFloat(caches);
    for (std::size_t i = 0; i < caches; ++i) {
        drawCache(app, i, {gridsX + toFloat(i) * (gridWidth + gap), middleY, gridWidth, middleHeight});
    }

    drawStats(app, {margin, kHeight - margin - statsHeight, kWidth - 2 * margin, statsHeight});
}

// Top bar: the current request, its address split into tag, index and offset, and what
// each policy did with it.
void Renderer::drawRequestBar(const App& app, Box box) const {
    const Palette& c = palette();
    const Simulator& sim = app.simulator();
    const CacheConfig& config = sim.config();
    drawPanel(box.x, box.y, box.w, box.h);

    const float left = box.x + 18;
    const float top = box.y + 12;
    const Focus focus = focusOf(sim);

    std::string title;
    if (!focus.request) {
        title = "EMPTY TRACE";
    } else if (!focus.processed) {
        title = "NEXT REQUEST  (Space steps, P plays)";
    } else {
        title = "REQUEST " + std::to_string(*focus.request + 1) + " OF " + std::to_string(sim.trace().size());
        if (sim.finished()) title += "  -  end of trace: R restarts, T loads the next one";
    }
    drawText(small_, c.dim, left, top, title);
    if (focus.processed && !sim.finished()) {
        drawText(small_, c.dim, box.x + box.w - 18, top, "next: " + toString(sim.trace()[sim.position()]),
                 ALLEGRO_ALIGN_RIGHT);
    }
    if (!focus.request) return;

    const MemRequest& request = sim.trace()[*focus.request];
    const AddressParts parts = config.split(request.address);
    const int addressDigits = hexWidth(config.addressBits);
    const float row = top + 22;

    // The operation and the address in hex, with the byte underneath.
    const bool isWrite = request.op == Op::Write;
    drawText(large_, c.text, left, row, std::string(isWrite ? "WRITE " : "READ  ") + hex(request.address, addressDigits));
    std::string byteNote;
    if (isWrite) {
        byteNote = "stores " + hex(request.value, 2);
    } else if (focus.processed) {
        byteNote = "returns " + hex(sim.history()[*focus.request][0].value, 2);
    }
    drawText(small_, c.dim, left, row + 34, byteNote);

    // The address in binary, one color per field, each labeled underneath.
    const std::string bits = binary(request.address, config.addressBits);
    const unsigned tagBits = config.tagBits();
    const unsigned indexBits = config.indexBits();
    struct Field {
        std::string bits;
        std::string label;
        ALLEGRO_COLOR color;
    };
    const Field fields[] = {
        {bits.substr(0, tagBits), "tag " + hex(parts.tag, hexWidth(tagBits)), c.tag},
        {bits.substr(tagBits, indexBits), "set " + std::to_string(parts.index), c.index},
        {bits.substr(tagBits + indexBits), "offset " + std::to_string(parts.offset), c.offset},
    };
    float x = left + textWidth(large_, "WRITE " + hex(0, addressDigits)) + 44;
    for (const Field& field : fields) {
        const float slot = std::max(textWidth(large_, field.bits), textWidth(small_, field.label));
        drawText(large_, field.color, x + slot / 2, row, field.bits, ALLEGRO_ALIGN_CENTRE);
        drawText(small_, field.color, x + slot / 2, row + 34, field.label, ALLEGRO_ALIGN_CENTRE);
        x += slot + 18;
    }

    // What each policy did.
    if (!focus.processed) return;
    const float outcomeX = box.x + box.w - 470;
    const std::vector<AccessResult>& results = sim.history()[*focus.request];
    for (std::size_t i = 0; i < results.size(); ++i) {
        const AccessResult& result = results[i];
        const float y = top + 22 + toFloat(i) * 26;
        drawText(body_, c.text, outcomeX, y, sim.cache(i).policyName());
        drawText(body_, result.hit ? c.hit : c.miss, outcomeX + 56, y, result.hit ? "HIT" : "MISS");

        std::vector<Piece> detail{{"set " + std::to_string(result.setIndex) + ", way " + std::to_string(result.way), c.dim}};
        if (result.evictedTag) {
            detail.push_back({", evicted tag " + hex(*result.evictedTag, hexWidth(tagBits)), c.evict});
            if (result.wroteBack) detail.push_back({" (written back)", c.dirty});
        } else if (!result.hit) {
            detail.push_back({", filled a free line", c.dim});
        }
        drawPieces(small_, outcomeX + 108, y + 2, detail);
    }
}

// Left column: the request list, the latest request highlighted, hit or miss per policy.
void Renderer::drawTrace(const App& app, Box box) const {
    const Palette& c = palette();
    const Simulator& sim = app.simulator();
    drawPanel(box.x, box.y, box.w, box.h);

    const float left = box.x + 12;
    float y = box.y + 10;
    drawText(small_, c.dim, left, y,
             "TRACE " + std::to_string(app.traceIndex() + 1) + " OF " + std::to_string(app.traceCount()) +
                 "  (T: next)");
    y += 18;
    drawText(body_, c.text, left, y, fitText(body_, app.trace().name, box.w - 24));
    y += lineHeight(body_) + 2;
    std::vector<std::string> description = wrapText(small_, app.trace().description, box.w - 24);
    description.resize(std::min<std::size_t>(description.size(), 3));
    for (const std::string& line : description) {
        drawText(small_, c.dim, left, y, line);
        y += lineHeight(small_);
    }
    y += 8;

    // Column headings: request number, the request, then one column per policy.
    const std::size_t policies = sim.cacheCount();
    const float policyColumn = 42;
    auto policyX = [&](std::size_t i) { return box.x + box.w - 12 - toFloat(policies - i) * policyColumn + policyColumn / 2; };
    const float numberRight = left + 30;
    const float requestX = left + 40;
    drawText(small_, c.dim, numberRight, y, "#", ALLEGRO_ALIGN_RIGHT);
    drawText(small_, c.dim, requestX, y, "request");
    for (std::size_t p = 0; p < policies; ++p) {
        drawText(small_, c.dim, policyX(p), y, sim.cache(p).policyName(), ALLEGRO_ALIGN_CENTRE);
    }
    y += lineHeight(small_) + 4;
    al_draw_line(box.x + 8, y - 2, box.x + box.w - 8, y - 2, c.border, 1);

    // The rows, scrolled so the latest request stays near the middle.
    const float rowHeight = lineHeight(body_) + 2;
    const std::size_t total = sim.trace().size();
    const std::size_t position = sim.position();
    const std::size_t visible = static_cast<std::size_t>(std::max(0.0f, (box.y + box.h - 8 - y) / rowHeight));
    std::size_t first = 0;
    if (total > visible) first = std::min(total - visible, position > visible / 2 ? position - visible / 2 : 0);

    for (std::size_t i = first; i < std::min(total, first + visible); ++i) {
        const float rowY = y + toFloat(i - first) * rowHeight;
        const bool done = i < position;
        if (position > 0 && i == position - 1) {
            al_draw_filled_rectangle(box.x + 4, rowY - 1, box.x + box.w - 4, rowY + rowHeight - 1, c.highlight);
        }
        if (i == position) drawText(body_, c.index, box.x + 6, rowY, ">");
        drawText(small_, c.faint, numberRight, rowY + 2, std::to_string(i + 1), ALLEGRO_ALIGN_RIGHT);
        drawText(body_, done ? c.text : c.dim, requestX, rowY, toString(sim.trace()[i]));
        if (!done) continue;
        for (std::size_t p = 0; p < policies; ++p) {
            const bool hit = sim.history()[i][p].hit;
            drawText(body_, hit ? c.hit : c.miss, policyX(p), rowY, hit ? "H" : "M", ALLEGRO_ALIGN_CENTRE);
        }
    }
    if (total == 0) drawText(body_, c.dim, left, y, "(no requests)");
}

// One cache: rows are sets and columns are ways. The set of the current request is lit up,
// the line it touched flashes, and each full set outlines the line its policy evicts next.
void Renderer::drawCache(const App& app, std::size_t cacheIndex, Box box) const {
    const Palette& c = palette();
    const Simulator& sim = app.simulator();
    const Cache& cache = sim.cache(cacheIndex);
    const CacheConfig& config = sim.config();
    drawPanel(box.x, box.y, box.w, box.h);

    const Focus focus = focusOf(sim);
    const AccessResult* touched = focus.processed ? &sim.history()[*focus.request][cacheIndex] : nullptr;
    std::optional<std::size_t> litSet;
    if (focus.request) litSet = config.split(sim.trace()[*focus.request].address).index;

    // Heading: the policy, and its hit rate so far.
    const float left = box.x + 14;
    const float right = box.x + box.w - 14;
    float y = box.y + 10;
    const std::string hitRate = percent(cache.stats().hitRate());
    drawText(large_, c.text, left, y, cache.policyName() + " cache");
    drawText(large_, c.text, right, y, hitRate, ALLEGRO_ALIGN_RIGHT);
    drawText(small_, c.dim, right - textWidth(large_, hitRate) - 8, y + 8, "hit rate", ALLEGRO_ALIGN_RIGHT);
    y += 34;

    // Legend.
    float legendX = drawPieces(small_, left, y,
                               {{"V", c.text}, {" valid  ", c.dim}, {"tag", c.tag}, {"  ", c.dim},
                                {"D", c.dirty}, {" dirty  ", c.dim}, {"00", c.offset}, {" byte at offset  ", c.dim}});
    drawDashedRect(legendX, y + 2, legendX + 16, y + 12, c.evict, 1.5f);
    drawText(small_, c.dim, legendX + 22, y, "next victim");
    y += lineHeight(small_) + 6;

    // Grid geometry. When the ways don't fit across, each set wraps onto several rows of
    // cells, and the label column widens to say which ways each row holds.
    const std::size_t ways = config.associativity;
    const std::size_t sets = config.numSets();
    const float minCellWidth = 100;
    const std::size_t columns =
        std::clamp<std::size_t>(static_cast<std::size_t>((right - left - 36) / minCellWidth), 1, ways);
    const std::size_t subRows = (ways + columns - 1) / columns;
    const float labelWidth = subRows > 1 ? 50 : 36;
    const float gridX = left + labelWidth;
    const float cellWidth = (right - gridX) / toFloat(columns);

    if (subRows == 1) {
        for (std::size_t w = 0; w < ways; ++w) {
            drawText(small_, c.faint, gridX + (toFloat(w) + 0.5f) * cellWidth, y, "way " + std::to_string(w),
                     ALLEGRO_ALIGN_CENTRE);
        }
        y += lineHeight(small_) + 2;
    }

    // Cells stretch to fill the panel, but never shrink below the top row plus one row of
    // bytes. If the sets don't all fit at that size, show a window of them that follows the
    // current request, with a caption underneath.
    const float rowGap = 4;
    const float minCellHeight = 4 + lineHeight(body_) + lineHeight(small_) + 3 + 2;  // padding included
    const float maxCellHeight = 84;
    const float captionHeight = 20;
    const float gridTop = y;
    float available = box.y + box.h - 8 - gridTop;
    const bool scrolls = toFloat(sets * subRows) * minCellHeight + rowGap * toFloat(sets - 1) > available;
    if (scrolls) available -= captionHeight;
    const float cellHeight = std::clamp((available - rowGap * toFloat(sets - 1)) / toFloat(sets * subRows),
                                        minCellHeight, maxCellHeight);
    const float rowHeight = toFloat(subRows) * cellHeight + rowGap;
    const std::size_t visibleSets =
        scrolls ? std::clamp<std::size_t>(static_cast<std::size_t>((available + rowGap) / rowHeight), 1, sets) : sets;
    std::size_t firstSet = 0;
    if (visibleSets < sets && litSet) {
        firstSet = std::min(sets - visibleSets, *litSet > visibleSets / 2 ? *litSet - visibleSets / 2 : 0);
    }

    const float flash = flashStrength(app);
    for (std::size_t s = firstSet; s < firstSet + visibleSets; ++s) {
        const float rowY = gridTop + toFloat(s - firstSet) * rowHeight;
        const float rowBottom = rowY + toFloat(subRows) * cellHeight;
        const bool lit = litSet && *litSet == s;
        if (lit) {
            al_draw_filled_rounded_rectangle(box.x + 6, rowY - 2, box.x + box.w - 6, rowBottom + 2, 5, 5, c.highlight);
            al_draw_filled_rectangle(box.x + 6, rowY - 2, box.x + 9, rowBottom + 2, c.index);
        }
        const ALLEGRO_COLOR labelColor = lit ? c.index : c.dim;
        if (subRows == 1) {
            drawText(body_, labelColor, left + 2, rowY + cellHeight / 2 - lineHeight(body_) / 2, "S" + std::to_string(s));
        } else {
            drawText(body_, labelColor, left + 2, rowY + 4, "S" + std::to_string(s));
            for (std::size_t sub = 0; sub < subRows; ++sub) {
                const std::size_t firstWay = sub * columns;
                const std::size_t lastWay = std::min(ways, firstWay + columns) - 1;
                drawText(small_, c.faint, left + 2, rowY + toFloat(sub + 1) * cellHeight - lineHeight(small_) - 4,
                         "w" + std::to_string(firstWay) + "-" + std::to_string(lastWay));
            }
        }

        // With one way there is no choice to show: the only line is always the victim.
        const std::optional<std::size_t> victim = ways > 1 ? cache.nextVictim(s) : std::nullopt;
        for (std::size_t w = 0; w < ways; ++w) {
            const Box cell{gridX + toFloat(w % columns) * cellWidth + 3, rowY + toFloat(w / columns) * cellHeight + 1,
                           cellWidth - 6, cellHeight - 2};
            const bool isTouched = touched && touched->setIndex == s && touched->way == w;
            drawLine(cache.set(s)[w], config, cell, isTouched ? touched : nullptr, victim && *victim == w, flash);
        }
    }

    if (scrolls) {
        drawText(small_, c.dim, box.x + box.w / 2, box.y + box.h - 22,
                 "sets " + std::to_string(firstSet) + "-" + std::to_string(firstSet + visibleSets - 1) + " of " +
                     std::to_string(sets) + " (follows the current request)",
                 ALLEGRO_ALIGN_CENTRE);
    }
}

// One cache line: valid bit, tag and dirty flag on top, then the data bytes, then (for one
// step) the tag of the block it just evicted.
void Renderer::drawLine(const CacheLine& line, const CacheConfig& config, Box cell, const AccessResult* touched,
                        bool nextVictim, float flash) const {
    const Palette& c = palette();
    const float x2 = cell.x + cell.w;
    const float y2 = cell.y + cell.h;
    al_draw_filled_rounded_rectangle(cell.x, cell.y, x2, y2, 4, 4, line.valid ? c.validLine : c.emptyLine);

    if (touched) {
        const ALLEGRO_COLOR outcome = touched->hit ? c.hit : c.miss;
        if (flash > 0) al_draw_filled_rounded_rectangle(cell.x, cell.y, x2, y2, 4, 4, translucent(outcome, flash));
        al_draw_rounded_rectangle(cell.x + 1, cell.y + 1, x2 - 1, y2 - 1, 4, 4, outcome, 2);
    }
    if (nextVictim) drawDashedRect(cell.x + 4, cell.y + 4, x2 - 4, y2 - 4, c.evict, 1.5f);

    const int tagDigits = hexWidth(config.tagBits());
    const std::string tagPlaceholder(static_cast<std::size_t>(tagDigits), '-');
    const float textX = cell.x + 9;
    const float top = cell.y + 4;
    const float smallHeight = lineHeight(small_);

    // Top row: valid bit and tag on the left, dirty flag on the right.
    float rightEdge = x2 - 9;  // the free space on the top row ends here
    if (line.valid) {
        drawPieces(body_, textX, top, {{"V1 ", c.text}, {hexDigits(line.tag, tagDigits), c.tag}});
        if (line.dirty) {
            drawText(body_, c.dirty, rightEdge, top, "D", ALLEGRO_ALIGN_RIGHT);
            rightEdge -= textWidth(body_, "D") + 6;
        }
    } else {
        drawText(body_, c.faint, textX, top, "V0 " + tagPlaceholder);
    }
    const float tagEnd = textX + textWidth(body_, "V0 " + tagPlaceholder);
    const float y = top + lineHeight(body_);  // where the byte rows start

    // The block this line just pushed out, shown for one step: "out" and its old tag struck
    // through in amber, then "written back" if it was dirty. That takes the bottom rows when
    // the cell also has room for a row of bytes. Otherwise "out <tag>" goes at the right of
    // the top row if it fits there, or is left out (the top bar always says what was evicted).
    std::size_t noteRows = 0;
    if (touched && touched->evictedTag) {
        const std::string oldTag = hexDigits(*touched->evictedTag, tagDigits);
        const std::string note = "out " + oldTag;
        const std::size_t rowsWanted = touched->wroteBack ? 2 : 1;
        bool showNote = true;
        float noteX = textX;
        float noteY = top + 2;
        if (toFloat(rowsWanted + 1) * smallHeight <= y2 - 3 - y) {
            noteRows = rowsWanted;
            noteY = y2 - 3 - toFloat(noteRows) * smallHeight;
            if (touched->wroteBack) drawText(small_, c.dirty, textX, y2 - 3 - smallHeight, "written back");
        } else if (rightEdge - textWidth(small_, note) > tagEnd + 10) {
            noteX = rightEdge - textWidth(small_, note);
        } else {
            showNote = false;
        }
        if (showNote) {
            drawText(small_, c.evict, noteX, noteY, note);
            const float tagX = noteX + textWidth(small_, "out ");
            const float strikeY = noteY + smallHeight * 0.55f;
            al_draw_line(tagX - 1, strikeY, tagX + textWidth(small_, oldTag) + 1, strikeY, c.evict, 1);
        }
    }

    // Data bytes, as many rows as fit above the note; "+N" counts the ones that don't fit.
    const float byteWidth = textWidth(small_, "00 ");
    const float rowWidth = cell.w - 9 - 4;  // left and right padding
    const std::size_t perRow =
        std::max<std::size_t>(1, static_cast<std::size_t>((rowWidth + textWidth(small_, " ")) / byteWidth));
    const float bottom = y2 - 3 - toFloat(noteRows) * smallHeight;
    const std::size_t rows = static_cast<std::size_t>(std::max(0.0f, (bottom - y) / smallHeight));
    const std::size_t bytes = line.data.size();
    std::size_t shown = std::min(bytes, rows * perRow);
    if (shown < bytes && shown > 0) --shown;  // make room for "+N"
    for (std::size_t b = 0; b < shown; ++b) {
        const bool atOffset = touched && b == touched->offset;
        const ALLEGRO_COLOR color = !line.valid ? c.faint : (atOffset ? c.offset : c.text);
        drawText(small_, color, textX + toFloat(b % perRow) * byteWidth, y + toFloat(b / perRow) * smallHeight,
                 line.valid ? hexDigits(line.data[b], 2) : "--");
    }
    if (shown < bytes && rows > 0) {
        drawText(small_, c.dim, textX + toFloat(shown % perRow) * byteWidth, y + toFloat(shown / perRow) * smallHeight,
                 "+" + std::to_string(bytes - shown));
    }
}

// Bottom bar: the counters for each policy, a hit-rate bar, and the keys.
void Renderer::drawStats(const App& app, Box box) const {
    const Palette& c = palette();
    const Simulator& sim = app.simulator();
    drawPanel(box.x, box.y, box.w, box.h);

    const float left = box.x + 18;
    const float top = box.y + 10;
    const char* const headings[] = {"Accesses", "Hits", "Misses", "Evictions", "Write-backs", "Hit rate"};
    const float columnRight[] = {170, 240, 320, 420, 530, 620};  // right edge of each column, from `left`
    const float barX = left + 650;
    const float barWidth = 190;

    drawText(small_, c.dim, left, top + 2, "POLICY");
    for (std::size_t col = 0; col < 6; ++col) {
        drawText(small_, c.dim, left + columnRight[col], top + 2, headings[col], ALLEGRO_ALIGN_RIGHT);
    }
    for (std::size_t i = 0; i < sim.cacheCount(); ++i) {
        const Cache& cache = sim.cache(i);
        const CacheStats& stats = cache.stats();
        const float y = top + 22 + toFloat(i) * 22;
        const std::string values[] = {std::to_string(stats.accesses), std::to_string(stats.hits),
                                      std::to_string(stats.misses), std::to_string(stats.evictions),
                                      std::to_string(stats.writeBacks), percent(stats.hitRate())};
        drawText(body_, c.text, left, y, cache.policyName());
        for (std::size_t col = 0; col < 6; ++col) {
            drawText(body_, c.text, left + columnRight[col], y, values[col], ALLEGRO_ALIGN_RIGHT);
        }
        const float barY = y + 5;
        al_draw_filled_rounded_rectangle(barX, barY, barX + barWidth, barY + 9, 3, 3, c.emptyLine);
        if (stats.hitRate() > 0) {
            al_draw_filled_rounded_rectangle(barX, barY, barX + barWidth * static_cast<float>(stats.hitRate()),
                                             barY + 9, 3, 3, c.hit);
        }
    }

    // Keys, with the playback state.
    std::string state = app.playing() ? "playing" : "paused";
    if (sim.finished() && sim.position() > 0) state = "finished";
    const float keysX = box.x + box.w - 350;
    const float keyColumn2 = keysX + 200;
    const std::vector<std::vector<Piece>> lines = {
        {{"Space ", c.text}, {"step", c.dim}},
        {{"Up/Dn ", c.text}, {"speed " + speedText(app.stepsPerSecond()) + ", " + state, c.dim}},
        {{"T     ", c.text}, {"next trace", c.dim}},
    };
    const std::vector<std::vector<Piece>> column2 = {
        {{"P   ", c.text}, {"play / pause", c.dim}},
        {{"R   ", c.text}, {"reset", c.dim}},
        {{"Esc ", c.text}, {"quit", c.dim}},
    };
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const float y = top + 2 + toFloat(i) * 21;
        drawPieces(small_, keysX, y, lines[i]);
        drawPieces(small_, keyColumn2, y, column2[i]);
    }
}

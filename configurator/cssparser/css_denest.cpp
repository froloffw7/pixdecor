/************************************************************************
 **
 **  css_denest.cpp
 **  Used for the Sigil C++ CSS Parser
 **
 **  Copyright (C) 2026  Kevin B. Hendricks, Stratford, Ontario, Canada
 **
 **  License: MIT
 **  Permission is hereby granted, free of charge, to any person obtaining a
 **  copy of this software and associated documentation files (the “Software”),
 **  to deal in the Software without restriction, including without limitation
 **  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 **  and/or sell copies of the Software, and to permit persons to whom the
 **  Software is furnished to do so, subject to the following conditions:
 **
 **  The above copyright notice and this permission notice shall be included
 **  in all copies or substantial portions of the Software.
 **
 **  THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS
 **  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 **  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 **  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 **  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 **  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 **  DEALINGS IN THE SOFTWARE.
 ** 
 ***********************************************************************/
#include "css_denest.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

// ------------------------------------------------------------
// String utilities
// ------------------------------------------------------------

std::string css_denest::trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace((unsigned char)s[b])) ++b;
    while (e > b && std::isspace((unsigned char)s[e - 1])) --e;
    return s.substr(b, e - b);
}

std::string css_denest::replace_all(std::string s, const std::string& from,
                                    const std::string& to) {
    if (from.empty()) return s;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

// ------------------------------------------------------------
// Selector helpers
// ------------------------------------------------------------

// Split a comma-separated selector list, respecting parentheses.
std::vector<std::string> css_denest::split_selectors(const std::string& s) {
    std::vector<std::string> parts;
    std::string cur;
    int depth = 0;
    for (char c : s) {
        if (c == '(') ++depth;
        else if (c == ')' && depth > 0) --depth;
        if (c == ',' && depth == 0) {
            std::string t = trim(cur);
            if (!t.empty()) parts.push_back(t);
            cur.clear();
        } else {
            cur += c;
        }
    }
    std::string t = trim(cur);
    if (!t.empty()) parts.push_back(t);
    return parts;
}

// Combine one parent selector with one child selector.
// If child contains '&', every '&' is replaced by parent.
// Otherwise the child is treated as a descendant of parent.
std::string css_denest::combine_one(const std::string& parent,
                                    const std::string& child) {
    if (child.find('&') != std::string::npos)
        return replace_all(child, "&", parent);
    return parent + " " + child;
}

// Cartesian product of two selector lists.
// ".a, .b"  x  ".c, &:hover"  →  ".a .c, .a:hover, .b .c, .b:hover"
std::string css_denest::combine_selector_lists(const std::string& parent,
                                               const std::string& child) {
    auto ps = split_selectors(parent);
    auto cs = split_selectors(child);
    std::string result;
    bool first = true;
    for (const auto& p : ps) {
        for (const auto& c : cs) {
            if (!first) result += ",\n";
            result += combine_one(p, c);
            first = false;
        }
    }
    return result;
}

// ------------------------------------------------------------
// Low-level character-level readers
// (all advance `pos` and append consumed text to `out`)
// ------------------------------------------------------------

void css_denest::read_comment(const std::string& in, size_t& pos, std::string& out) {
    out += in[pos++]; // '/'
    out += in[pos++]; // '*'
    while (pos + 1 < in.size() &&
           !(in[pos] == '*' && in[pos + 1] == '/')) {
        out += in[pos++];
    }
    if (pos + 1 < in.size()) {
        out += in[pos++]; // '*'
        out += in[pos++]; // '/'
    }
}

void css_denest::read_string(const std::string& in, size_t& pos, std::string& out) {
    char q = in[pos];
    out += in[pos++]; // opening quote
    while (pos < in.size() && in[pos] != q) {
        if (in[pos] == '\\' && pos + 1 < in.size())
            out += in[pos++]; // backslash
        out += in[pos++];
    }
    if (pos < in.size()) out += in[pos++]; // closing quote
}

void css_denest::read_parenthesis(const std::string& in, size_t& pos, std::string& out) {
    out += in[pos++]; // '('
    int depth = 1;
    while (pos < in.size() && depth > 0) {
        char c = in[pos];
        if (c == '(') { ++depth; out += in[pos++]; }
        else if (c == ')') { --depth; out += in[pos++]; }
        else if ((c == '"' || c == '\'') && depth > 0) {
            read_string(in, pos, out);
        } else if (c == '/' && pos + 1 < in.size() && in[pos + 1] == '*') {
            read_comment(in, pos, out);
        } else {
            out += in[pos++];
        }
    }
}

// ------------------------------------------------------------
// Parser
// ------------------------------------------------------------

// Read the raw content between '{' (already consumed) and the matching '}'.
// Advances pos past the closing '}'.
std::string css_denest::read_raw_body(const std::string& in, size_t& pos) {
    std::string body;
    int depth = 1;
    while (pos < in.size() && depth > 0) {
        char c = in[pos];
        if (c == '/' && pos + 1 < in.size() && in[pos + 1] == '*') {
            read_comment(in, pos, body); continue;
        }
        if (c == '"' || c == '\'') { read_string(in, pos, body); continue; }
        if (c == '(')              { read_parenthesis(in, pos, body); continue; }
        if (c == '{') ++depth;
        else if (c == '}') {
            --depth;
            if (depth == 0) { ++pos; break; } // consume the closing '}'
        }
        body += in[pos++];
    }
    return body;
}

// Parse CSS text at the current nesting level into a list of Nodes.
// Stops at EOF or an unmatched '}' (without consuming it).
std::vector<css_denest::node_t> css_denest::parse(const std::string& in, size_t& pos) {
    std::vector<node_t> nodes;

    while (pos < in.size() && in[pos] != '}') {

        // Accumulate text until '{', '}', or ';'.
        std::string chunk;
        while (pos < in.size()) {
            char c = in[pos];
            if (c == '{' || c == '}' || c == ';') break;
            if (c == '/' && pos + 1 < in.size() && in[pos + 1] == '*') {
                read_comment(in, pos, chunk); continue;
            }
            if (c == '"' || c == '\'') { read_string(in, pos, chunk); continue; }
            if (c == '(')              { read_parenthesis(in, pos, chunk); continue; }
            chunk += in[pos++];
        }

        if (pos >= in.size() || in[pos] == '}') {
            if (!chunk.empty())
                nodes.push_back({node_t::TEXT, chunk, "", "", {}, false});
            break;
        }

        char stop = in[pos];

        if (stop == ';') {
            chunk += in[pos++]; // include ';'
            nodes.push_back({node_t::TEXT, chunk, "", "", {}, false});
        } else { // stop == '{'
            // Separate any leading whitespace and CSS block comments from the
            // actual selector/at-rule header.
            size_t ci = 0;
            while (ci < chunk.size()) {
                size_t prev = ci;
                // skip whitespace
                while (ci < chunk.size() &&
                       std::isspace((unsigned char)chunk[ci])) ++ci;
                // skip /* … */ comment
                if (ci + 1 < chunk.size() &&
                    chunk[ci] == '/' && chunk[ci + 1] == '*') {
                    ci += 2;
                    while (ci + 1 < chunk.size() &&
                           !(chunk[ci] == '*' && chunk[ci + 1] == '/')) ++ci;
                    if (ci + 1 < chunk.size()) ci += 2;
                } else if (ci == prev) {
                    break; // no progress → start of actual header
                }
            }

            // Anything before `ci` is pre-header whitespace/comments.
            if (ci > 0)
                nodes.push_back({node_t::TEXT, chunk.substr(0, ci),
                                 "", "", {}, false});

            std::string header = trim(chunk.substr(ci));

            ++pos; // consume '{'

            std::string raw_body = read_raw_body(in, pos);

            size_t innerPos = 0;
            std::vector<node_t> children = parse(raw_body, innerPos);

            css_denest::node_t n;
            n.type        = node_t::BLOCK;
            n.header      = header;
            n.raw_body    = raw_body;
            n.children    = std::move(children);
            n.is_at_rule  = (!header.empty() && header[0] == '@');
            nodes.push_back(std::move(n));
        }
    }

    return nodes;
}

// ------------------------------------------------------------
// At-rule classification
// ------------------------------------------------------------

// Extract the @-keyword (lowercased), e.g. "@media", "@keyframes".
std::string css_denest::at_rule_keyword(const std::string& header) {
    std::string kw;
    for (char c : header) {
        if (std::isspace((unsigned char)c) || c == '(') break;
        kw += (char)std::tolower((unsigned char)c);
    }
    return kw;
}

// At-rules whose block content must NOT be rewritten for nesting.
// Their body is reproduced verbatim.
bool css_denest::is_pass_through_at_rule(const std::string& header) {
    std::string kw = at_rule_keyword(header);
    return kw == "@keyframes"         ||
           kw == "@-webkit-keyframes" ||
           kw == "@-moz-keyframes"    ||
           kw == "@-o-keyframes"      ||
           kw == "@font-face"         ||
           kw == "@counter-style"     ||
           kw == "@viewport"          ||
           kw == "@-ms-viewport";
}

// ------------------------------------------------------------
// Flattener (mutually recursive)
// ------------------------------------------------------------

std::string css_denest::flatten_block(const node_t& block, const std::string& parentSel) {
    std::ostringstream out;

    if (block.is_at_rule) {
        if (is_pass_through_at_rule(block.header)) {
            // @keyframes, @font-face, etc. – emit verbatim
            out << block.header << " {\n" << block.raw_body << "}\n";
            return out.str();
        }

        // @media, @supports, @layer, @document, etc.
        // Flatten the children under the same parentSel, then wrap.
        std::string inner = flatten_nodes(block.children, parentSel);
        out << block.header << " {\n" << inner << "}\n";
    } else {
        bool has_hested_blocks = std::any_of(
            block.children.begin(), block.children.end(),
            [](const node_t& child) { return child.type == node_t::BLOCK; });

        if (parentSel.empty() && !has_hested_blocks) {
            out << block.header << " {" << block.raw_body << "}\n";
            return out.str();
        }

        // Regular selector block.
        std::string selector = parentSel.empty()
            ? block.header
            : combine_selector_lists(parentSel, block.header);

        out << flatten_nodes(block.children, selector);
    }

    return out.str();
}

std::string css_denest::flatten_nodes(const std::vector<node_t>& nodes,
                                       const std::string& parentSel) {
    std::ostringstream out;

    if (parentSel.empty()) {
        // Top-level (or inside an at-rule with no owning selector):
        // TEXT nodes pass through directly; BLOCK nodes are flattened.
        for (const auto& n : nodes) {
            if (n.type == node_t::TEXT)
                out << n.text;
            else
                out << flatten_block(n, "");
        }
    } else {
        // Inside a selector context:
        // 1. Collect all direct declarations (TEXT nodes) and emit as
        //    "selector { decls }" (preserving the original whitespace).
        // 2. Flatten nested BLOCK nodes afterwards.
        std::string decls;
        for (const auto& n : nodes)
            if (n.type == node_t::TEXT) decls += n.text;

        if (!trim(decls).empty())
            out << parentSel << " {" << decls << "}\n";

        for (const auto& n : nodes)
            if (n.type == node_t::BLOCK)
                out << flatten_block(n, parentSel);
    }

    return out.str();
}

// ============================================================
// Public
// ============================================================

std::string css_denest::denest_css(const std::string& input) {
    size_t pos = 0;
    std::vector<node_t> nodes = parse(input, pos);
    return flatten_nodes(nodes, "");
}
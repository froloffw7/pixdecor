/************************************************************************
 **
 **  css_denest.h
 **  Used for the Sigil C++ CSS Parser
 **
 **  Copyright (C) 2026  Kevin B. Hendricks, Stratford, Ontario, Canada
 **  Co-authored with CoPilot: Claude 4.6 engine
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
#ifndef CSS_DENEST_H
#define CSS_DENEST_H 

#include <string>
#include <vector>

// Flattens native CSS nesting so that older CSS parsers can consume the output.
//
// Any CSS that is not nested is passed through unchanged.
// Nested selectors are combined with their parent using the "&" nesting
// selector rules (if "&" appears in the child it is replaced by the parent;
// otherwise the child becomes a descendant of the parent).
// Nested at-rules (@media, @supports, …) are hoisted above the enclosing
// selector so that the selector is wrapped inside the at-rule.
// At-rules whose content must not be rewritten (@keyframes, @font-face, …)
// are emitted verbatim.
//
// Parameters:
//   input  – contents of a CSS file
// Returns:
//   flattened CSS as a std::string

class css_denest {

  public:

    struct node_t {
        enum type_t { TEXT, BLOCK } type = TEXT;

        // TEXT node: raw text (may include a trailing ';')
        std::string text;

        // BLOCK node fields
        std::string header;             // selector or at-rule, trimmed, without '{'
        std::string raw_body;           // raw text between '{' and '}' (before parsing)
        std::vector<node_t> children;   // parsed children of this block
        bool is_at_rule = false;        // header starts with '@'
    };


    static std::string denest_css(const std::string& input);
    
  private:

    // ------------------------------------------------------------
    // String utilities
    // ------------------------------------------------------------

    static std::string trim(const std::string& s);
    static std::string replace_all(std::string s, const std::string& from, const std::string& to);

    // ------------------------------------------------------------
    // Selector helpers
    // ------------------------------------------------------------

    // Split a comma-separated selector list, respecting parentheses.
    static std::vector<std::string> split_selectors(const std::string& s);

    // Combine one parent selector with one child selector.
    // If child contains '&', every '&' is replaced by parent.
    // Otherwise the child is treated as a descendant of parent.
    static std::string combine_one(const std::string& parent, const std::string& child);

    // Cartesian product of two selector lists.
    // ".a, .b"  x  ".c, &:hover"  →  ".a .c, .a:hover, .b .c, .b:hover"
    static std::string combine_selector_lists(const std::string& parent, const std::string& child);

    // ------------------------------------------------------------
    // Low-level character-level readers
    // (all advance `pos` and append consumed text to `out`)
    // ------------------------------------------------------------

    static void read_comment(const std::string& in, size_t& pos, std::string& out);
    static void read_string(const std::string& in, size_t& pos, std::string& out);
    static void read_parenthesis(const std::string& in, size_t& pos, std::string& out);

    // ------------------------------------------------------------
    // Parser
    // ------------------------------------------------------------

    // Read the raw content between '{' (already consumed) and the matching '}'.
    // Advances pos past the closing '}'.
    static std::string read_raw_body(const std::string& in, size_t& pos);

    // Parse CSS text at the current nesting level into a list of Nodes.
    // Stops at EOF or an unmatched '}' (without consuming it).
    static std::vector<css_denest::node_t> parse(const std::string& in, size_t& pos);

    // ------------------------------------------------------------
    // At-rule classification
    // ------------------------------------------------------------

    // Extract the @-keyword (lowercased), e.g. "@media", "@keyframes".
    static std::string at_rule_keyword(const std::string& header);

    // At-rules whose block content must NOT be rewritten for nesting.
    // Their body is reproduced verbatim.
    static bool is_pass_through_at_rule(const std::string& header);

    // ------------------------------------------------------------
    // Flattener (mutually recursive)
    // ------------------------------------------------------------

    static std::string flatten_nodes(const std::vector<css_denest::node_t>& nodes,
                                    const std::string& parentSel);
    
    static std::string flatten_block(const css_denest::node_t& block,
                                    const std::string& parent_sel);

};
#endif // CSS_DENEST_H

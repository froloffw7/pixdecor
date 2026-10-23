/************************************************************************
 **
 **  css_parser
 **  A Simple C++ CSS Parser
 **
 **  Copyright (C) 2021  Kevin B. Hendricks, Stratford, Ontario, Canada
 **
 **  License: LGPL v2.1 or later
 **
 **  Extracted and modified from:
 **  CSSTidy (https://github.com/csstidy-c/csstidy)
 **
 **  CSSTidy Portions Copyright:
 **     Florian Schmitz <floele@gmail.com>
 **     Thierry Charbonnel
 **     Will Mitchell <aethon@gmail.com>
 **     Brett Zamir <brettz9@yahoo.com>
 **     sined_ <sined_@users.sourceforge.net>
 **     Dmitry Leskov <git@dmitryleskov.com>
 **     Kevin Coyner <kcoyner@debian.org>
 **     Tuukka Pasanen <pasanen.tuukka@gmail.com>
 **     Frank W. Bergmann <csstidy-c@tuxad.com>
 **     Frank Dana <ferdnyc@gmail.com>
 **
 **     CSSTidy itself is Available under the LGPL 2.1
 **
 ** You should have received a copy of the GNU Lesser General Public License 2.1
 ** along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **
 *************************************************************************/
 
#ifndef CSS_PARSER_H
#define CSS_PARSER_H

#include <cstdlib>
#include <utility>
#include <string> 
#include <iterator>
#include <vector>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <map>
#include <algorithm>
#include "css_properties.h"
#include "css_utils.h"

class css_parser 
{ 

public:

    enum parse_status_t
    {
        PIS, PIP, PIV, PINSTR, PIC, PAT
    };

    enum message_type_t
    {
        Information,Warning,Error
    };

    enum token_type_t
    {
        CHARSET, IMPORT, NAMESP, AT_START, AT_END, SEL_START, SEL_END, PROPERTY, VALUE, COMMENT, CSS_END
    };

    struct token_t
    {
        token_type_t type;
        int pos;
        int line;
        std::string data;
    };

    struct message_t
    {
        std::string m;
        message_type_t t;
    };

    css_parser();

    // valid levels are "CSS1.0", "CSS2.0", "CSS2.1", "CSS3.0"
    void set_level(std::string level = "CSS3.0");

    void parse_css(std::string css_input);

    void reset_parser();        

    // serialize the current list of csstokens back to css
    std::string serialize_css(std::string filename = "", bool tostdout = true);
        
    // access charset, namespace and imports without having to walk csstokens
    std::string get_charset();
    std::vector<std::string> get_import();
    std::string get_namespace();

    // walk the csstokens list, token by token
    // set start_ptr to the position you woud like to start at in the list
    // leaving it as -1 will simply start at 0 and increment
    // last token is a dummy token with type set to CSS_END 
    token_t get_next_token(int start_ptr = -1);

    // covert token type enum value to a descriptive string
    std::string get_type_name(token_type_t t);

    // this routine allows external modifications to the csstokens
    // to be brought back into css_parser in order to serialize them
    // with serialize_css
    void set_css_tokens(const std::vector<token_t> &ntokens);

    // get errors, warnings, and information
    std::vector<std::string> get_parse_errors();
    std::vector<std::string> get_parse_warnings();
    std::vector<std::string> get_parse_info();


private:

    void parse_in_at_block(std::string& css_input, int& i, parse_status_t& astatus, parse_status_t& afrom);

    void parse_in_selector(std::string& css_input, int& i, parse_status_t& astatus, parse_status_t& afrom,
                         bool& invalid_at, char& str_char, int str_size);

    void parse_in_property(std::string& css_input, int& i, parse_status_t& astatus, parse_status_t& afrom,
                         bool& invalid_at);

    void parse_in_value(std::string& css_input, int& i, parse_status_t& astatus, parse_status_t& afrom,
                      bool& invalid_at, char& str_char, bool& pn, int str_size );

    void parse_in_comment(std::string& css_input, int& i, parse_status_t& astatus, parse_status_t& afrom,
                        std::string& cur_comment);

    void parse_in_string(std::string& css_input, int& i, parse_status_t& astatus, parse_status_t& afrom,
                       char&  str_char, bool& str_in_str);


    // filter the logs by message type
    std::vector<std::string> get_logs(message_type_t t);

    // Pparses unicode notations
    std::string unicode(std::string& istring, int& i);
        
    // checks if the chat in istring at i is a token
    bool is_token(std::string& istring, const int i);
                        
    void add_token(const token_type_t ttype, const std::string data, const bool force = false);
        
    // Add a message to the message log
    void log(const std::string msg, const message_type_t type, int iline = 0);

    // records token position information
    void record_position(parse_status_t old_status, parse_status_t new_status,
                         std::string &css_input, int i, bool force=false);
        
    int _seeknocomment(const int key, const int move);

    void explode_selectors();

    static bool property_is_next(std::string istring, int pos);

    // private member variables
    std::vector<std::string> token_type_names;
    std::map<std::string, parse_status_t>  at_rules;
    std::vector<std::string> css_template;
    std::string css_level;
    std::string tokens;
    int token_ptr;

    std::map<int, std::vector<message_t> > logs;
    int  line;
    int  spos;
    int  sline;
    int selector_nest_level;

    std::string charset;
    std::string namesp;
    std::vector<std::string> import;
    std::vector<token_t> css_tokens;
    std::string cur_selector;
    std::string cur_at;
    std::string cur_property;
    std::string cur_function;
    std::string cur_sub_value;
    std::string cur_value; 
    std::string cur_string;
    std::vector<int>   sel_separate;
    std::vector<std::string> cur_sub_value_arr;
    std::vector<std::string> cur_function_arr;

};
        
#endif // CSS_PARSER_H

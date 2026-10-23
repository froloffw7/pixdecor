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
 
#include "css_properties.h"
#include "css_parser.h"

/* PIS = in selector
 * PIP = in property
 * PIV = in value
 * PINSTR = in string (-> ",',( => ignore } and ; etc.)
 * PIC = in comment (ignore everything)
 * PAT = in @-block
 */

css_parser::css_parser()
{ 
    tokens = "{};:()@='\"/,\\!$%&*+.<>?[]^`|~";

    // Used for serializing parsed css
    css_template.push_back("    ");    //  0 - standard indentation
    css_template.push_back(" {\n");    //  1 - bracket after @-rule
    css_template.push_back("");        //  2 - unused
    css_template.push_back(" {\n");    //  3 - bracket after selector was "\n{\n"
    css_template.push_back("");        //  4 - unused
    css_template.push_back("");        //  5 - string after property before value
    css_template.push_back(";\n");     //  6 - string after value
    css_template.push_back("}");       //  7 - closing bracket - selector
    css_template.push_back("\n\n");    //  8 - space between blocks {...}
    css_template.push_back("}\n\n");   //  9 - closing bracket @-rule
    css_template.push_back("");        // 10 - unused
    css_template.push_back("");        // 11 - before comment
    css_template.push_back("\n");      // 12 - after comment
    css_template.push_back("\n");      // 13 - after last line @-rule

    // at_rule to parser state map
    at_rules["page"] = PIS;
    at_rules["font-face"] = PIS;
    at_rules["charset"] = PIV;
    at_rules["import"] = PIV;
    at_rules["namespace"] = PIV;
    at_rules["media"] = PAT;
    at_rules["keyframes"] = PAT;
    at_rules["supports"] = PAT;
    at_rules["-moz-keyframes"] = PAT;
    at_rules["-ms-keyframes"] = PAT;
    at_rules["-o-keyframes"] = PAT;
    at_rules["-webkit-keyframes"] = PAT;

    // descriptive names for each token type
    token_type_names.push_back("CHARSET");
    token_type_names.push_back("IMPORT");
    token_type_names.push_back("NAMESP");
    token_type_names.push_back("AT_START");
    token_type_names.push_back("AT_END");
    token_type_names.push_back("SEL_START");
    token_type_names.push_back("SEL_END");
    token_type_names.push_back("PROPERTY");
    token_type_names.push_back("VALUE");
    token_type_names.push_back("COMMENT");
    token_type_names.push_back("CSS_END");

    css_level = "CSS3.0";
} 


void css_parser::set_level(std::string level)
{
    if ((level == "CSS1.0") || (level == "CSS2.0") ||
        (level == "CSS2.1") || (level == "CSS3.0"))
    {
        css_level = level;
    }
}


void css_parser::reset_parser()
{
    token_ptr = 0;
    charset = "";
    namesp = "";
    selector_nest_level = 0;
    line = 1;
    import.clear();
    css_tokens.clear();
    cur_selector.clear();
    cur_at.clear();
    cur_property.clear();
    cur_function.clear();
    cur_sub_value.clear();
    cur_value.clear();
    cur_string.clear();
    cur_selector.clear();
    sel_separate.clear();
}


std::string css_parser::get_charset()
{
    return charset;
}

std::string css_parser::get_namespace()
{
    return namesp;
}

std::vector<std::string> css_parser::get_import()
{
    return import;
}


css_parser::token_t css_parser::get_next_token(int start_ptr)
{
    if ((start_ptr >= 0) && (start_ptr < css_tokens.size()))
    {
        token_ptr = start_ptr;
    }

    token_t atoken;
    atoken.type = CSS_END;
    atoken.data = "";

    if (token_ptr < css_tokens.size())
    {
        atoken = css_tokens[token_ptr];
        token_ptr++;
    }
    return atoken;
}


std::string css_parser::get_type_name(css_parser::token_type_t t)
{
    return token_type_names[t];
}


void css_parser::add_token(const token_type_t ttype, const std::string data, const bool force)
{
    token_t temp;
    temp.type = ttype;
    temp.pos = spos;
    temp.line = sline;
    temp.data = (ttype == COMMENT) ? data : css_utils::trim(data);
    css_tokens.push_back(temp);
    if (ttype == SEL_START) selector_nest_level++;
    if (ttype == SEL_END) selector_nest_level--;
}


void css_parser::log(const std::string msg, const message_type_t type, int iline)
{
    message_t new_msg;
    new_msg.m = msg;
    new_msg.t = type;
    if(iline == 0)
    {
        iline = line;
    }
    if(logs.count(line) > 0)
    {
        for(int i = 0; i < logs[line].size(); ++i)
        {
            if(logs[line][i].m == new_msg.m && logs[line][i].t == new_msg.t)
            {
                return;
            }
        }
    }
    logs[line].push_back(new_msg);
}

std::string css_parser::unicode(std::string& istring, int& i)
{
    ++i;
    std::string add = "";
    bool replaced = false;
    
    while(i < istring.length() && (css_utils::ctype_xdigit(istring[i]) ||
                                   css_utils::ctype_space(istring[i])) && add.length()< 6)
    {
        add += istring[i];

        if(css_utils::ctype_space(istring[i])) {
            break;
        }
        i++;
    }

    if ((css_utils::hexdec(add) > 47 && css_utils::hexdec(add) < 58) ||
        (css_utils::hexdec(add) > 64 && css_utils::hexdec(add) < 91) ||
        (css_utils::hexdec(add) > 96 && css_utils::hexdec(add) < 123))
    {
        std::string msg = "Replaced unicode notation: Changed \\" + css_utils::rtrim(add) + " to ";
        add = static_cast<int>(css_utils::hexdec(add));
        msg += add;
        log(msg, Information);
        replaced = true;
    } else {
        add = css_utils::trim("\\" + add);
    }

    if ((css_utils::ctype_xdigit(istring[i+1]) && css_utils::ctype_space(istring[i])
         && !replaced) || !css_utils::ctype_space(istring[i]))
    {
        i--;
    }
    
    if (1) {
        return add;
    }
    return "";
}


bool css_parser::is_token(std::string& istring, const int i)
{
    return (css_utils::in_str_array(tokens, istring[i]) && !css_utils::escaped(istring,i));
}


void css_parser::explode_selectors()
{
    sel_separate = std::vector<int>();
}


int css_parser::_seeknocomment(const int key, int move)
{
    int go = (move > 0) ? 1 : -1;
    for (int i = key + 1; abs(key-i)-1 < abs(move); i += go)
    {
        if (i < 0 || i >= css_tokens.size()) {
            return -1;
        }
        if (css_tokens[i].type == COMMENT) {
            move += 1;
            continue;
        }
        return css_tokens[i].type;
    }
    return -1;
}


std::string css_parser::serialize_css(std::string filename, bool tostdout)
{
    if(charset == "" && namesp == "" && import.empty() && css_tokens.empty())
    {
        std::cout << "Warning: empty CSS output!" << std::endl;
    }

    std::ofstream file_output;
    if(filename != "")
    {
         file_output.open(filename.c_str(),std::ios::binary);
         if(file_output.bad())
         {
             std::cout << "Error when trying to save the output file!" << std::endl;
             return "";
         }
    }

    std::stringstream output;
    int lvl = 0;
    std::string indent = "";

    for (size_t i = 0; i < css_tokens.size(); ++i)
    {
        switch (css_tokens[i].type)
        {
            case CHARSET:
                output << "@charset " << css_template[5] << css_tokens[i].data << css_template[6];
                break;

            case IMPORT:
                indent = css_utils::indent(lvl, css_template[0]);
                output << indent << "@import " << css_template[5] << css_tokens[i].data << css_template[6];
                break;

            case NAMESP:
                output << "@namespace " << css_template[5] << css_tokens[i].data << css_template[6];
                break;

            case AT_START:
                indent = css_utils::indent(lvl, css_template[0]);
                output << indent << css_tokens[i].data << css_template[1];
                lvl++;
                break;

            case SEL_START:
                indent = css_utils::indent(lvl, css_template[0]);
                output << indent << css_tokens[i].data << css_template[3];
                lvl++;
                break;

            case PROPERTY:
                indent = css_utils::indent(lvl, css_template[0]);
                output << indent << css_tokens[i].data << ":" << css_template[5];
                break;

            case VALUE:
                output << css_tokens[i].data << css_template[6];
                break;

            case SEL_END:
                lvl--; if (lvl < 0) lvl = 0;
                indent = css_utils::indent(lvl, css_template[0]);
                output << indent + css_template[7];
                if(_seeknocomment(i, 1) != AT_END) output << css_template[8];
                break;

            case AT_END:
                lvl--; if (lvl < 0) lvl = 0;
                indent = css_utils::indent(lvl, css_template[0]);
                output << css_template[13] << indent << css_template[9];
                break;

            case COMMENT:
                output << css_template[11] <<  "/*" << css_tokens[i].data << "*/" << css_template[12];
                break;

            case CSS_END:
                break;
        }
    }

    std::string output_string = css_utils::trim(output.str());

    if(tostdout)
    {
        std::cout << output_string << "\n";
    }
    if(filename != "")
    {
        file_output << output_string;
        file_output.close();
    }
    return output_string;
}


void css_parser::parse_in_at_block(std::string& css_input, int& i, parse_status_t& astatus, parse_status_t& afrom)
{
    if(is_token(css_input,i))
    {
        if(css_input[i] == '/' && css_utils::s_at(css_input,i+1) == '*')
        {
            astatus = PIC; i += 2;
            afrom = PAT;
        }
        else if(css_input[i] == '{')
        {
            astatus = PIS;
            add_token(AT_START, cur_at);
        }
        else if(css_input[i] == ',')
        {
            cur_at = css_utils::trim(cur_at) + ",";
        }
        else if(css_input[i] == '\\')
        {
            cur_at += unicode(css_input,i);
        }
        else
        {
            if(!css_utils::in_char_arr("():/.", css_input[i]))
            {
                // Strictly speaking, these are only permitted in @media rules 
                log("Unexpected symbol '" + std::string(css_input, i, 1) + "' in @-rule", Warning);
            }
            cur_at += css_input[i];  /* append tokens after media selector */
        }
    }
    else
    {
        // Skip excess whitespace
        int lastpos = cur_at.length()-1;
        if(lastpos == -1 || !( (css_utils::ctype_space(cur_at[lastpos]) ||
                               (is_token(cur_at,lastpos) && cur_at[lastpos] == ',')) && 
                               css_utils::ctype_space(css_input[i])))
        {
            cur_at += css_input[i];
        }
    }
}


void css_parser::parse_in_selector(std::string& css_input, int& i, parse_status_t& astatus, parse_status_t& afrom,
                                 bool& invalid_at, char& str_char, int str_size)
{
    if(is_token(css_input,i))
    {
        if(css_input[i] == '/' && css_utils::s_at(css_input,i+1) == '*')
        {
            astatus = PIC; ++i;
            afrom = PIS;
        }
        else if(css_input[i] == '@' && css_utils::trim(cur_selector) == "")
        {
            // Check for at-rule
            invalid_at = true;
            for(std::map<std::string,parse_status_t>::iterator j = at_rules.begin(); j != at_rules.end(); ++j )
            {
                if(css_utils::strtolower(css_input.substr(i+1,j->first.length())) == j->first)
                {
                    (j->second == PAT) ? cur_at = "@" + j->first : cur_selector = "@" + j->first;
                    astatus = j->second;
                    i += j->first.length();
                    invalid_at = false;
                }
            }
            if (invalid_at)
            {
                cur_selector = "@";
                std::string invalid_at_name = "";
                for(int j = i+1; j < str_size; ++j)
                {
                    if(!css_utils::ctype_alpha(css_input[j]))
                    {
                        return;
                    }
                    invalid_at_name += css_input[j];
                }
                log("Invalid @-rule: " + invalid_at_name + " (removed)", Warning);
            }
        }
        else if(css_input[i] == '"' || css_input[i] == '\'')
        {
            cur_string = css_input[i];
            astatus = PINSTR;
            str_char = css_input[i];
            afrom = PIS;
        }
        else if(invalid_at && css_input[i] == ';')
        {
            invalid_at = false;
            astatus = PIS;
        }
        else if (css_input[i] == '{')
        {
            astatus = PIP;
            add_token(SEL_START, cur_selector);
        }
        else if (css_input[i] == '}')
        {
            add_token(AT_END, cur_at);
            cur_at = "";
            cur_selector = "";
            sel_separate = std::vector<int>();
        }
        else if(css_input[i] == ',')
        {
            cur_selector = css_utils::trim(cur_selector) + ",";
            sel_separate.push_back(cur_selector.length());
        }
        else if (css_input[i] == '\\')
        {
            cur_selector += unicode(css_input,i);
        }
        // remove unnecessary universal selector, FS#147
        else if(!(css_input[i] == '*' && (css_utils::s_at(css_input,i+1) == '.' || css_utils::s_at(css_input,i+1) == '[' || css_utils::s_at(css_input,i+1) == ':' || css_utils::s_at(css_input,i+1) == '#')))
        {
            cur_selector += css_input[i];
        }
    }
    else
    {
        int lastpos = cur_selector.length()-1;
        if((lastpos == -1) || !( (css_utils::ctype_space(cur_selector[lastpos]) ||
                                  (is_token(cur_selector,lastpos) && cur_selector[lastpos] == ',')) &&
                                 css_utils::ctype_space(css_input[i])))
        {
            cur_selector += css_input[i];
        }
    }
}


void css_parser::parse_in_property(std::string& css_input, int& i, parse_status_t& astatus, parse_status_t& afrom,
                                 bool& invalid_at)
{

    if (is_token(css_input,i))
    {
        if (css_input[i] == ':' || (css_input[i] == '=' && cur_property != ""))
        {
            astatus = PIV;
            bool valid = true || (css_properties::instance()->contains(cur_property) && 
                                  css_properties::instance()->levels(cur_property).find(css_level,0) != std::string::npos);
            if(valid) {
                add_token(PROPERTY, cur_property);
            }
        }
        else if(css_input[i] == '/' && css_utils::s_at(css_input,i+1) == '*' && cur_property == "")
        {
            astatus = PIC; ++i;
            afrom = PIP;
        }
        else if(css_input[i] == '}')
        {
            explode_selectors();
            astatus = PIS;
            invalid_at = false;
            add_token(SEL_END, cur_selector);
            cur_selector = "";
            cur_property = "";
        }
        else if(css_input[i] == ';')
        {
            cur_property = "";
        }
        else if(css_input[i] == '\\')
        {
            cur_property += unicode(css_input,i);
        }
        else if(css_input[i] == '*') 
        {
            // IE7 and below recognize properties that begin with '*'
            if (cur_property == "")
            {
                cur_property += css_input[i];
                log("IE7- hack detected: property name begins with '*'", Warning);
            } 
        }
        else
        {
            log("Unexpected character '" + std::string(1, css_input[i]) + "'in property name", Error);
        }
    }
    else if(!css_utils::ctype_space(css_input[i]))
    {
        if(css_input[i] == '_' && cur_property == "")
        {
            // IE6 and below recognize properties that begin with '_'
            log("IE6 hack detected: property name begins with '_'", Warning);
        }
        cur_property += css_input[i];
    }
}


void css_parser::parse_in_value(std::string& css_input, int& i, parse_status_t& astatus, parse_status_t& afrom,
                              bool& invalid_at, char& str_char, bool& pn, int str_size)
{
    pn = (((css_input[i] == '\n' || css_input[i] == '\r') && property_is_next(css_input,i+1)) || i == str_size-1);
    if(pn)
    {
        log("Added semicolon to the end of declaration", Warning);
    }
    if(is_token(css_input,i) || pn)
    {
        if((css_input[i] == '{') && (cur_selector == "@import" ||
                                     cur_selector == "@charset" ||
                                     cur_selector == "@namespace"))
        {
            log("Unexpected character '" + std::string(1, css_input[i]) + "' in " + cur_selector, Error);
        }
        if(css_input[i] == '/' && css_utils::s_at(css_input,i+1) == '*')
        {
            astatus = PIC; ++i;
            afrom = PIV;
        }
        else if(css_input[i] == '"' || css_input[i] == '\'' ||
                (css_input[i] == '(' && cur_sub_value == "url") )
        {
            str_char = (css_input[i] == '(') ? ')' : css_input[i];
            cur_string = css_input[i];
            astatus = PINSTR;
            afrom = PIV;
        }
        else if(css_input[i] == '(')
        {
            assert(cur_sub_value != "url");
                    
            cur_sub_value = css_utils::trim(cur_sub_value + "(");
            cur_function = cur_sub_value;
            cur_function_arr.push_back(cur_sub_value);
            cur_sub_value_arr.push_back(cur_sub_value);
            cur_sub_value = "";
        }
        else if(css_input[i] == '\\')
        {
            cur_sub_value += unicode(css_input,i);
        }
        else if(css_input[i] == ';' || pn)
        {
            if(cur_selector.substr(0,1) == "@" && 
               at_rules.count(cur_selector.substr(1)) > 0 &&
               at_rules[cur_selector.substr(1)] == PIV)
            {
                cur_sub_value_arr.push_back(css_utils::trim(cur_sub_value));
                astatus = PIS;

                if(cur_selector == "@charset")
                {
                     charset = cur_sub_value_arr[0];
                     add_token(CHARSET, charset);
                }
                else if(cur_selector == "@import")
                {
                     std::string aimport = css_utils::build_value(cur_sub_value_arr);
                     add_token(IMPORT, aimport);
                     import.push_back(aimport);
                }
                else if(cur_selector == "@namespace")
                {
                    namesp = css_utils::implode(" ",cur_sub_value_arr);
                    add_token(NAMESP, namesp);
                }
                cur_sub_value_arr.clear();
                cur_sub_value = "";
                cur_selector = "";
                sel_separate = std::vector<int>();
            }
            else
            {
                astatus = PIP;
            }
        }
        else if (css_input[i] == '!') 
        {
            cur_sub_value_arr.push_back(css_utils::trim(cur_sub_value));
            cur_sub_value = "!";
        }
        else if (css_input[i] == ',' || css_input[i] == ')') 
        {
            cur_sub_value = css_utils::trim(cur_sub_value);
            if(cur_sub_value != "")
            {
                cur_sub_value_arr.push_back(cur_sub_value);
                cur_sub_value = "";
            }
            bool drop = false;
            if (css_input[i] == ')')
            {
                if (cur_function_arr.empty())
                {
                    log("Unexpected closing parenthesis, dropping", Warning);
                    drop = true;
                }
                else
                {
                    cur_function_arr.pop_back();
                    cur_function = cur_function_arr.empty() ? "" : cur_function_arr.back();
                }
            }
            if (!drop) {
                cur_sub_value_arr.push_back(std::string(1,css_input[i]));
            }
        }
        else if(css_input[i] != '}')
        {
            cur_sub_value += css_input[i];
        }
        if( (css_input[i] == '}' || css_input[i] == ';' || pn) && !cur_selector.empty())
        {
            if(cur_at == "")
            {
                cur_at = "standard";
            }

            cur_at = css_utils::trim(cur_at);
            cur_selector = css_utils::trim(cur_selector);
            cur_value = css_utils::trim(cur_value);
            cur_property = css_utils::trim(cur_property);
            cur_sub_value = css_utils::trim(cur_sub_value);

            cur_property = css_utils::strtolower(cur_property);

            if(cur_sub_value != "")
            {
                cur_sub_value_arr.push_back(cur_sub_value);
                cur_sub_value = "";
            }

            if (!cur_function_arr.empty())
            {
                std::vector<std::string>::reverse_iterator rit;
                for (rit = cur_function_arr.rbegin(); rit != cur_function_arr.rend(); ++rit)
                {
                    log("Closing parenthesis missing for '" + *rit + "', inserting", Warning);
                    cur_sub_value_arr.push_back(")");
                }
            }

            cur_value = css_utils::build_value(cur_sub_value_arr);

            bool valid = (css_properties::instance()->contains(cur_property) &&
                          css_properties::instance()->levels(cur_property).find(css_level,0) != std::string::npos);
            if(1)
            {
                add_token(VALUE, cur_value);
            }
            if(!valid)
            {
                log("Invalid property in " + css_utils::strtoupper(css_level) + ": " + cur_property, Warning);
            }

            cur_property = "";
            cur_sub_value_arr.clear();
            cur_value = "";
        }
        if(css_input[i] == '}')
        {
            explode_selectors();
            add_token(SEL_END, cur_selector);
            astatus = PIS;
            invalid_at = false;
            cur_selector = "";
        }
    }
    else if(!pn)
    {
        cur_sub_value += css_input[i];

        if(css_utils::ctype_space(css_input[i]))
        {
            if(css_utils::trim(cur_sub_value) != "")
            {
                cur_sub_value_arr.push_back(css_utils::trim(cur_sub_value));
            }
            cur_sub_value = "";
        }
    }
}


void css_parser::parse_in_comment(std::string& css_input, int& i, parse_status_t& astatus, parse_status_t& afrom,
                                std::string& cur_comment)
{

    if(css_input[i] == '*' && css_utils::s_at(css_input,i+1) == '/')
    {
        astatus = afrom;
        ++i;
        add_token(COMMENT, cur_comment);
        cur_comment = "";
    }
    else
    {
        cur_comment += css_input[i];
    }
}


void css_parser::parse_in_string(std::string& css_input, int& i, parse_status_t& astatus, parse_status_t& afrom,
                               char& str_char, bool& str_in_str)
{

    if(str_char == ')' && (css_input[i] == '"' || css_input[i] == '\'') &&
       str_in_str == false && !css_utils::escaped(css_input,i))
    {
        str_in_str = true;
    }
    else if(str_char == ')' && (css_input[i] == '"' || css_input[i] == '\'') &&
            str_in_str == true && !css_utils::escaped(css_input,i))
    {
        str_in_str = false;
    }
    std::string temp_add = ""; temp_add += css_input[i];
    if( (css_input[i] == '\n' || css_input[i] == '\r') &&
        !(css_input[i-1] == '\\' && !css_utils::escaped(css_input,i-1)) )
    {
        temp_add = "\\A ";
        log("Fixed incorrect newline in string", Warning);
    }
    if (!(str_char == ')' && 
          css_utils::char2str(css_input[i]).find_first_of(" \n\t\r\0xb") != std::string::npos && !str_in_str))
    {
        cur_string += temp_add;
    }
    if(css_input[i] == str_char && !css_utils::escaped(css_input,i) && str_in_str == false)
    {
        astatus = afrom;
        if (cur_function == "" && 
            cur_string.find_first_of(" \n\t\r\0xb") == std::string::npos &&
            cur_property != "content" && cur_sub_value != "format")
        {
            if (str_char == '"' || str_char == '\'')
            {
                cur_string = cur_string.substr(1, cur_string.length() - 2);
            } else if (cur_string.length() > 3 && (cur_string[1] == '"' || cur_string[1] == '\''))
            {
                cur_string = cur_string[0] + cur_string.substr(2, cur_string.length() - 4) + cur_string[cur_string.length()-1];
            }
        }
        if(afrom == PIV)
        {
            cur_sub_value += cur_string;
        }
        else if(afrom == PIS)
        {
            cur_selector += cur_string;
        }
    }
}


void css_parser::parse_css(std::string css_input)
{
    reset_parser();
    css_input = css_utils::str_replace("\r\n","\n",css_input);
    css_input += "\n";
    parse_status_t astatus = PIS, afrom;
    parse_status_t old_status = PIS;
    record_position(PIS, PIS, css_input, 0, true);
    std::string cur_comment;

    cur_sub_value_arr.clear();
    cur_function_arr.clear();
    char str_char;
    bool str_in_str = false;
    bool invalid_at = false;
    bool pn = false;

    int str_size = css_input.length();
    for(int i = 0; i < str_size; ++i)
    {
        if(css_input[i] == '\n' || css_input[i] == '\r')
        {
            ++line;
        }

        if (old_status != astatus)
        {
            record_position(old_status, astatus, css_input, i);
        }
        old_status = astatus;

        switch(astatus)
        {
            case PAT:
                parse_in_at_block(css_input, i, astatus, afrom);
                break;

            case PIS:
                parse_in_selector(css_input, i, astatus, afrom, invalid_at, str_char, str_size);
                break;

            case PIP:
                parse_in_property(css_input, i, astatus, afrom, invalid_at);
                break;

            case PIV:
                parse_in_value(css_input, i, astatus, afrom, invalid_at, str_char, pn, str_size);
                break;

            case PINSTR:
                parse_in_string(css_input, i, astatus, afrom, str_char, str_in_str);
                break;

            case PIC:
                parse_in_comment(css_input, i, astatus, afrom, cur_comment);
                break;
        }
    }

    if (selector_nest_level != 0)
    {
        log("Unbalanced selector braces in style sheet", Error, line);
    }
}


bool css_parser::property_is_next(std::string istring, int pos)
{
    istring = istring.substr(pos, istring.length() - pos);
    size_t i = istring.find_first_of(':',0);
    if(i == std::string::npos)
    {
        return false;
    }
    istring = css_utils::strtolower(css_utils::trim(istring.substr(0, i)));
    return css_properties::instance()->contains(istring);
}


std::vector<std::string> css_parser::get_parse_errors()
{ 
    return get_logs(css_parser::Error);
}


std::vector<std::string> css_parser::get_parse_warnings()
{
    return get_logs(css_parser::Warning);
}


std::vector<std::string> css_parser::get_parse_info()
{
    return get_logs(css_parser::Information);
}


std::vector<std::string> css_parser::get_logs(css_parser::message_type_t mt)
{
    std::vector<std::string> res;
    if(logs.size() > 0)
    {
        for(std::map<int, std::vector<message_t> >::iterator j = logs.begin(); j != logs.end(); j++ )
        {
            for(size_t i = 0; i < j->second.size(); ++i)
            {
                if (j->second[i].t == mt) {
                    res.push_back(std::to_string(j->first) + ": " + j->second[i].m);
                }
            }
        }
    }
    return res;
}


void css_parser::set_css_tokens(const std::vector<css_parser::token_t> &ntokens)
{
    css_tokens.clear();
    for(size_t i = 0; i < ntokens.size(); i++)
    {
        token_t atemp = ntokens[i];
        css_tokens.push_back(atemp);
    }
}


void css_parser::record_position(parse_status_t old_status, parse_status_t new_status,
                                 std::string &css_input, int i, bool force)
{
    bool record = false;

    if (new_status == PIC) record = true;

    if ((old_status == PIS) && (new_status == PIP)) record = true;
    if ((old_status == PIV) && (new_status == PIP)) record = true;

    if ((old_status == PAT) && (new_status == PIV)) record = true;
    if ((old_status == PIP) && (new_status == PIV)) record = true;

    if ((old_status == PAT) && (new_status == PIS)) record = true;
    if ((old_status == PIV) && (new_status == PIS)) record = true;
    if ((old_status == PIP) && (new_status == PIS)) record = true;

    if (record || force) {
        spos = css_input.find_first_not_of(" \n\t\r\0xb", i);
        sline = line;
        for(int j = i+i; j <= spos; j++)
        {
            if (css_input[j] == '\n') sline++;
        }
    }
}
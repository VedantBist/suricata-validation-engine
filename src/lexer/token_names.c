#include "lexer/token_names.h"

#include <stddef.h>

#include "parser.tab.h"

const char *token_kind_name(int kind)
{
    switch (kind) {
    case YYEOF:          return "EOF";
    case TOK_ALERT:      return "ALERT";
    case TOK_DROP:       return "DROP";
    case TOK_PASS:       return "PASS";
    case TOK_TCP:        return "TCP";
    case TOK_UDP:        return "UDP";
    case TOK_ICMP:       return "ICMP";
    case TOK_ANY:        return "ANY";
    case TOK_ARROW:      return "ARROW";
    case TOK_BIDIR:      return "BIDIR";
    case TOK_LPAREN:     return "LPAREN";
    case TOK_RPAREN:     return "RPAREN";
    case TOK_COLON:      return "COLON";
    case TOK_SEMICOLON:  return "SEMICOLON";
    case TOK_COMMA:      return "COMMA";
    case TOK_IP:         return "IP";
    case TOK_CIDR:       return "CIDR";
    case TOK_PORT:       return "PORT";
    case TOK_NUMBER:     return "NUMBER";
    case TOK_VARIABLE:   return "VARIABLE";
    case TOK_STRING:     return "STRING";
    case TOK_OPTION_KEY: return "OPTION_KEY";
    case TOK_IDENT:      return "IDENT";
    case TOK_EOL:        return "EOL";
    case TOK_INVALID:    return "INVALID_TOKEN";
    default:             return "UNKNOWN";
    }
}

const char *token_static_text(int kind)
{
    switch (kind) {
    case TOK_ALERT:     return "alert";
    case TOK_DROP:      return "drop";
    case TOK_PASS:      return "pass";
    case TOK_TCP:       return "tcp";
    case TOK_UDP:       return "udp";
    case TOK_ICMP:      return "icmp";
    case TOK_ANY:       return "any";
    case TOK_ARROW:     return "->";
    case TOK_BIDIR:     return "<>";
    case TOK_LPAREN:    return "(";
    case TOK_RPAREN:    return ")";
    case TOK_COLON:     return ":";
    case TOK_SEMICOLON: return ";";
    case TOK_COMMA:     return ",";
    default:            return NULL;
    }
}

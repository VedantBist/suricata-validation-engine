#include "models/rule.h"

#include <stdlib.h>

#include "utils/xalloc.h"

Rule *rule_create(void)
{
    return xcalloc(1, sizeof(Rule));
}

void rule_free(Rule *rule)
{
    if (rule == NULL) {
        return;
    }
    free(rule->src_ip.text);
    free(rule->src_port.text);
    free(rule->dst_ip.text);
    free(rule->dst_port.text);
    free(rule);
}

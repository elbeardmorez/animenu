#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "tokenize.h"

struct easyoption *parseoption(struct easyoption *optlist, const char *cmd) {
  struct easyoption *opt = NULL;
  char cmdpart[32];
  int i;

  if (strlen(cmd) > 31) {
    printf("parse error: command line too long\n");
    return(NULL);
  }

  if (1 == sscanf(cmd, "%s", cmdpart)) {
    i = 0;
    while (optlist[i].name) {
      if (0 == strcasecmp(optlist[i].name, cmdpart)) {
        opt = &optlist[i];
        opt->value1 = opt->value2 = opt->value3 = 0;
        opt->argsread = sscanf(cmd, "%*s %d %d %d", &opt->value1, &opt->value2, &opt->value3);
        return(opt);
      }
      ++i;
    }
  } else
    printf("parse error: %s\n", cmd);

  return(NULL);
}

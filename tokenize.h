#ifndef TOKENIZE_H
#define TOKENIZE_H

struct easyoption
{
  int id;
  const char *name;
  int args,argsread;
  int value1,value2,value3;
};

struct easyoption *parseoption(struct easyoption *optlist,const char *cmd);

#endif

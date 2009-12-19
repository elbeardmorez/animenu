/****************************************************************************
 * options.c                                                                *
 * By Massimo Maricchiolo <mmaricch@yahoo.com>                              *
 *                                                                          *
 * Simplified by Alastair M. Robinson                                       *
 *                                                                          *
 ****************************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "animenu.h"

#include "options.h"

char rc_fontspec[256];
char rc_color[256];
char rc_selcolor[256];

char *makefontspec(char *fontname,char *fontsize)
{
  rc_fontspec[0]=0;
  strcat(rc_fontspec,"-*-");
  strcat(rc_fontspec,fontname);
  strcat(rc_fontspec,"-*-*-*--");
  if(fontsize)
    strcat(rc_fontspec,fontsize);
  else
    strcat(rc_fontspec,"36");
  strcat(rc_fontspec,"-*-*-*-*-*-*-*");
  return(rc_fontspec);
}


int read_config()
{
  char buff[1024];
  char *tmp, *key, *val, *val1, *rr, *c, *c1;
  int len, pos, i;
  FILE *f;

  buff[0]=0;
  strcat(buff,getenv("HOME"));
  strcat(buff,"/.animenu/animenurc");

  if(!(f=fopen(buff,"r")))
    return(0);

  while(fgets(buff,sizeof(buff),f)!=NULL)
  {
    len=strlen(buff);
    for (i=0;i<len;i++) buff[i]=tolower(buff[i]);

    if(buff[0]=='#') continue;
    if(len<5) continue;

    len--;
    if(buff[len]=='\n') buff[len]='\0';
    len--;
    if(buff[len]=='\r') buff[len]='\0';

    key=strtok(buff,"= \t");
    if (strcmp(key,"fontname")==0) {
      pos=strlen(key)+1;
      while (buff[pos]=='='||buff[pos]=='\t') pos++;
      tmp=&buff[pos];

      val=strtok(tmp,",\t");
      if (val[0]!='\0')
      {
        pos=strlen(val)+1;
        while (tmp[pos]==','||tmp[pos]=='\t'||tmp[pos]==' ') pos++;
        tmp=&tmp[pos];
        val1=strtok(tmp,"\t #");
        if (val[0]=='\0') val1=NULL;
        makefontspec(val,val1);
      }
    }

    if (strcmp(key,"font")==0) {
      pos=strlen(key)+1;
      while (buff[pos]=='='||buff[pos]=='\t'||buff[pos]==' ') pos++;
      tmp=&buff[pos];

      val=strtok(tmp,"\t #");
      strcat(rc_fontspec,val);
    }

    if (strcmp(key,"color")==0) {
      pos=strlen(key)+1;
      while (buff[pos]=='='||buff[pos]=='\t'||buff[pos]==' ') pos++;
      tmp=&buff[pos];

      val=strtok(tmp,",\t");
      if (val[0]!='\0') {
        pos=strlen(val)+1;
        while (tmp[pos]==','||tmp[pos]=='\t'||tmp[pos]==' ') pos++;
        tmp=&tmp[pos];

        val1=strtok(tmp,"\t #");
        c=strdup(val);
        c1=strdup(val1);

        strcpy(rc_color,c);
        strcpy(rc_selcolor,c1);
      }
    }
  }

  if(rc_fontspec[0]);
    fontname=rc_fontspec;
  if(rc_color[0]);
    fgcolor=rc_color;
  if(rc_selcolor[0]);
    selectfgcolor=rc_selcolor;

  fclose(f);

  return;
}

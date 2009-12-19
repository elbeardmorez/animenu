/****************************************************************************
 ** animenu.c ***************************************************************
 ****************************************************************************
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <lirc/lirc_client.h>

#include "tokenize.h"
#include "osd.h"
#include "menu.h"
#include "options.h"

char *progname=PACKAGE "-" VERSION;

enum myids {id_null,id_show,id_next,id_prev,id_select,id_back};

struct easyoption myoptions[]=
{
  {id_show,"show",0,0,0,0,0},
  {id_next,"next",0,0,0,0,0},
  {id_prev,"prev",0,0,0,0,0},
  {id_select,"select",0,0,0,0,0},
  {id_back,"back",0,0,0,0,0},
  {0,NULL,0,0,0,0,0}
};

enum optresults {opt_null,opt_daemonize,opt_exit_failure,opt_exit_success};

/* Globals */

static long osd_utime;
static struct animenucontext *rootmenu;
static int alarm_pending;
char *fontname="-misc-fixed-medium-r-normal--36-*-75-75-c-*-iso8859-*";
char *fgcolor="rgb:00/ff/00", *bgcolor="black", *selectfgcolor="rgb:bf/ff/bf";
int menutimeout=5, menuanimation=10000;
int dump=0;

void sigalarm_handler(int junk)
{
  if(alarm_pending)
  {
    alarm_pending=0;
    rootmenu->hide(rootmenu);
  }
}


int process_options(int argc,char *argv[])
{
  int daemonize=0;
  read_config();

  while(1)
  {
    int c;
    static struct option long_options[] =
    {
      {"help",no_argument,NULL,'h'},
      {"version",no_argument,NULL,'v'},
      {"daemon",no_argument,NULL,'d'},
      {"font",required_argument,NULL,'f'},
      {"fontname",required_argument,NULL,'n'},
      {"fgcolor",required_argument,NULL,'c'},
/*      {"bgcolor",required_argument,NULL,'b'}, */
      {"selectfgcolor",required_argument,NULL,'s'},
      {"menutimeout",required_argument,NULL,'t'},
      {"menuanimation",required_argument,NULL,'a'},
      {"dump",no_argument,NULL,'D'},
      {0, 0, 0, 0}
    };
    c = getopt_long(argc,argv,"hvdf:n:c:s:t:a:D",long_options,NULL);
    if(c==-1)
      break;
    switch (c)
    {
    case 'h':
      printf("Usage: %s [options]\n",argv[0]);
      printf("\t -h --help\t\tdisplay this message\n");
      printf("\t -v --version\t\tdisplay version\n");
      printf("\t -d --daemon\t\trun in background\n");
      printf("\t -f --font\t\tuse specified font (xfontsel format)\n");
      printf("\t -n --fontname\t\tspecify font by family name only\n");
      printf("\t -c --fgcolor\t\tuse specified foreground color\n");
/*      printf("\t -b --bgcolor\t\tuse specified background color\n"); */
      printf("\t -s --selectfgcolor\t\tcolor of selected item\n");
      printf("\t -t --menutimeout\t\thow long before menu dissapears (0 for no timeout)\n");
      printf("\t -a --menuanimation\t\tmenu animation speed (microseconds)\n");
      printf("\t -D --dump\t\tdump menu structure to screen\n");
      return(opt_exit_success);
    case 'v':
      printf("%s\n",progname);
      return(opt_exit_success);
    case 'd':
      daemonize=1;
      break;
    case 'f':
      fontname = strdup(optarg);
      break;
    case 'n':
      fontname = makefontspec(optarg,"36");
      break;
    case 'c':
      fgcolor = strdup(optarg);
      break;
/*    case 'b':
      bgcolor = strdup(optarg);
      break;
*/
    case 's':
      selectfgcolor = strdup(optarg);
      break;
    case 't':
      menutimeout = atoi(optarg);
      break;
    case 'a':
      menuanimation = atoi(optarg);
      break;
    case 'D':
      dump = 1;
      break;
    default:
      printf("Usage: %s [options]\n",argv[0]);
      return(opt_exit_failure);
    }
  }
  if (optind < argc-1)
  {
    fprintf(stderr,"%s: too many arguments\n",progname);
    return(opt_exit_failure);
  }
  return((daemonize ? opt_daemonize : opt_null));
}


int main(int argc, char *argv[])
{
  struct lirc_config *config;
  int daemonize=0;
  int currentchannel=0;
  struct animenuitem *selected_item=NULL;

  switch(process_options(argc,argv))
  {
    case opt_exit_success:
      return(EXIT_SUCCESS); break;
    case opt_exit_failure:
      return(EXIT_FAILURE); break;
    case opt_daemonize:
      daemonize=1; break;
  }
  if (menutimeout<0 || menuanimation<0) {
    fprintf(stderr,"menutimeout and menuanimation must be >= 0.\n");
    return(0);
  }

  if(!(rootmenu=animenu_create(NULL,"root.menu")))
  {
    fprintf(stderr,"Can't create root menu!\n");
    return(0);
  }
  if (dump) {
     animenu_dump(rootmenu);
     rootmenu->dispose(rootmenu);
     exit(0);
  }

  if (menutimeout != 0)
     signal(SIGALRM,sigalarm_handler);

  if(lirc_init("animenu",daemonize ? 0:1)==-1) exit(EXIT_FAILURE);

  if(lirc_readconfig(optind!=argc ? argv[optind]:NULL,&config,NULL)==0)
  {
    char *code;
    char *c;
    int ret;

    if(daemonize)
    {
      if(daemon(0,0)==-1)
      {
        fprintf(stderr,"%s: can't daemonize\n",
          progname);
        perror(progname);
        lirc_freeconfig(config);
        lirc_deinit();
        exit(EXIT_FAILURE);
      }
    }

    while(lirc_nextcode(&code)==0)
    {
      if(code==NULL) continue;
      while((ret=lirc_code2char(config,code,&c))==0 && c!=NULL)
      {
        struct easyoption *opt=parseoption(myoptions,c);
        if (opt)
        {
	  if (menutimeout != 0)
	     alarm(0);
          switch(opt->id)
          {
            case id_show:
	      if (rootmenu->visible) { rootmenu->hide(rootmenu); selected_item = NULL; }
	      else rootmenu->show(rootmenu);
              break;
            case id_next:
              rootmenu->next(rootmenu);
              break;
            case id_prev:
              rootmenu->prev(rootmenu);
              break;
            case id_select:
	      if(rootmenu->currentitem && rootmenu->currentitem->type==animenuitem_submenu && selected_item == NULL)
		 selected_item = rootmenu->currentitem;
              rootmenu->select(rootmenu);
              break;
            case id_back:
	      if((selected_item != NULL) && (selected_item->submenu->visible)) {
		 rootmenu->hide(selected_item->submenu);
		 rootmenu->currentitem = selected_item;
		 rootmenu->showcurrent(selected_item->submenu);
		 selected_item = NULL;
	      }
              break;
          }
	  if ((opt->id == id_show) && !(rootmenu->visible))
	     ;
	  else if (menutimeout != 0) {
	     alarm_pending=1; alarm(menutimeout);
	  }
        }
        else
          fprintf(stderr,"Command not recognised: %s\n",c);
      }
      free(code);
      if(ret==-1) break;
    }
    lirc_freeconfig(config);
  }

  rootmenu->dispose(rootmenu);

  lirc_deinit();

  exit(EXIT_SUCCESS);
}

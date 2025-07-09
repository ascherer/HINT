/*678:*/
#line 14618 "hint.w"

#include "basetypes.h"
#include <math.h> 
#include <string.h> 
#include <sys/stat.h> 
#include <fcntl.h> 
#include <ft2build.h> 
#include FT_FREETYPE_H
#include "error.h"
#include "format.h"
#include "get.h"
#include "hint.h"
#include "rendernative.h"


/*684:*/
#line 14800 "hint.w"


bool hint_map(void)
{return hget_map();
}

void hint_unmap(void)
{hget_unmap();
}
/*:684*/
#line 14633 "hint.w"


int main(int argc,char*argv[])
{char*stem_name= NULL,*prog_name= argv[0];
int stem_length= 0;
bool option_log= false;
int page_count= 0;
bool option_backward= false;
int px_h,px_v;
HINT_TRY{
/*680:*/
#line 14702 "hint.w"

debugflags= DBGBASIC;
if(argc<2)goto explain_usage;
argv++;
while(*argv!=NULL)
{if((*argv)[0]=='-')
{char option= (*argv)[1];
switch(option)
{case'l':option_log= true;break;
case'b':option_backward= true;break;
case'd':
argv++;if(*argv==NULL)goto explain_usage;
debugflags= strtol(*argv,NULL,16);
break;
default:goto explain_usage;
}
}
else
{int path_length= (int)strlen(*argv);
ALLOCATE(hin_name,path_length+6,char);
strcpy(hin_name,*argv);
if(path_length<4
||strncmp(hin_name+path_length-4,".hnt",4)!=0)
{strcat(hin_name,".hnt");
path_length+= 4;
}
stem_length= path_length-4;
ALLOCATE(stem_name,stem_length+6,char);
strncpy(stem_name,hin_name,stem_length);
stem_name[stem_length]= 0;
if(*(argv+1)!=NULL)goto explain_usage;
}
argv++;
}
/*:680*/
#line 14643 "hint.w"

/*682:*/
#line 14770 "hint.w"

#ifdef DEBUG
if(option_log)
{
strcat(stem_name,".hlg");
hlog= freopen(stem_name,"w",stdout);
if(hlog==NULL)
{fprintf(stderr,"Unable to open logfile %s\n",stem_name);
hlog= stderr;
}
stem_name[stem_length]= 0;
}
else
hlog= stderr;
#else
#line 14785 "hint.w"
 hlog= stderr;
#endif
#line 14787 "hint.w"
/*:682*/
#line 14644 "hint.w"

if(!hint_begin())goto explain_usage;
/*679:*/
#line 14692 "hint.w"

px_h= floor(0.5+(hint_hsize()/(double)ONE));
px_v= floor(0.5+(hint_vsize()/(double)ONE));
if(px_h!=0&&px_v!=0)
hint_resize(px_h,px_v,72.27,72.27);
/*:679*/
#line 14646 "hint.w"

if(option_backward)
{hpos= hend;
while(hint_backward())
{page_count--;
fprintf(hlog,"\nCompleted box being shipped out [%d]",page_count);
hint_show_page();
fprintf(hlog,"\n");
}
}
else
{while(hint_forward())
{page_count++;
fprintf(hlog,"\nCompleted box being shipped out [%d]",page_count);
hint_show_page();
fprintf(hlog,"\n");
}
}
hint_end();
/*683:*/
#line 14791 "hint.w"

if(hlog!=NULL)fclose(hlog);
/*:683*/
#line 14665 "hint.w"

return 0;
explain_usage:
/*681:*/
#line 14740 "hint.w"

fprintf(stderr,
"Usage: %s [options] filename.hnt\n",prog_name);
fprintf(stderr,
"Options:\n"
"\t -b     \t generate pages in backward order\n"
"\t -l     \t redirect stdout to a log file\n");
#ifdef DEBUG
fprintf(stderr,"\t -d XXX \t hexadecimal value. OR together these values:\n");
fprintf(stderr,"\t\t\t XX=%03X   basic debugging\n",DBGBASIC);
fprintf(stderr,"\t\t\t XX=%03X   tag debugging\n",DBGTAGS);
fprintf(stdout,"\t\t\t XX=%03X   node debugging\n",DBGNODE);
fprintf(stderr,"\t\t\t XX=%03X   definition debugging\n",DBGDEF);
fprintf(stderr,"\t\t\t XX=%03X   directory debugging\n",DBGDIR);
fprintf(stderr,"\t\t\t XX=%03X   range debugging\n",DBGRANGE);
fprintf(stderr,"\t\t\t XX=%03X   compression debugging\n",DBGCOMPRESS);
fprintf(stderr,"\t\t\t XX=%03X   buffer debugging\n",DBGBUFFER);
fprintf(stdout,"\t\t\t XX=%03X   TeX debugging\n",DBGTEX);
fprintf(stdout,"\t\t\t XX=%03X   Page debugging\n",DBGPAGE);
fprintf(stdout,"\t\t\t XX=%03X   Font debugging\n",DBGFONT);
fprintf(stdout,"\t\t\t XX=%03X   Render debugging\n",DBGRENDER);
fprintf(stdout,"\t\t\t XX=%03X   Label debugging\n",DBGLABEL);
#endif
#line 14763 "hint.w"
/*:681*/
#line 14668 "hint.w"

return 1;
}
else
return 1;
}
/*:678*/

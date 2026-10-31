/* Playable game: you are X, the computer is O, same screen as amoeba.cpp.
   The computer looks 3 or 4 of its own moves ahead (the search from
   review/harness/planner.cpp). Scores, predicted replies, fallback moves,
   rules, and the screen all come from amoeba.cpp. */
#define AMOEBA_NO_MAIN
#include "amoeba.cpp"

static int g_depth=3;
static unsigned g_rng=1;

static const int TOPK=10;   /* first-move candidates */
static const int THREATK=6; /* later moves: threes and fours only */

static void init_rng(void)
{
  const char *s=getenv("AMOEBA_SEED");
  char *end;
  unsigned long v;

  g_rng=(unsigned)time(NULL);
  if(s&&*s)
  {
    errno=0;
    v=strtoul(s,&end,10);
    if(!errno&&*end=='\0')g_rng=(unsigned)v;
  }
  srand(g_rng);
}

/* Restart the tie-break sequence before every engine call, including the
   computer's own move. The same position then always gets the same move. */
static void freeze_rng(void)
{srand(g_rng);}

static void swap_colours(tomb dst,tomb src)
{int i,j;
for(i=0;i<MAXX;i++)for(j=0;j<MAXY;j++)
  dst[i][j]=src[i][j]==X_SIGN?O_SIGN:src[i][j]==O_SIGN?X_SIGN:0;}

/* The stone at (x,y) completed five. Uses amoeba.cpp's line counter. */
static int made_five(tomb b,int x,int y,int c)
{return neighbours(b,x,y,c,1,0)>=IN_A_ROW
      ||neighbours(b,x,y,c,0,1)>=IN_A_ROW
      ||neighbours(b,x,y,c,1,1)>=IN_A_ROW
      ||neighbours(b,x,y,c,1,-1)>=IN_A_ROW;}

/* What colour would play if it were the computer in amoeba.cpp.
   O is scored on this board; X is scored after swapping colours.
   Sets 0-based (*x,*y). Returns 0 when the board is full. */
static int engine_square(int colour,tomb b,int *x,int *y)
{static tomb swapped,w1,w2;
int ok,gx,gy;
freeze_rng();
if(colour==O_SIGN)ok=pointto_m(&gx,&gy,w1,w2,b);
else {swap_colours(swapped,b);ok=pointto_m(&gx,&gy,w1,w2,swapped);}
if(!ok)return 0;
*x=gx-1;*y=gy-1;
return 1;}

/* Attack scores for colour, from minimax() in amoeba.cpp. */
static void engine_scores(int colour,tomb b,tomb out)
{static tomb swapped;
if(colour==O_SIGN)minimax(b,out,O_SIGN);
else {swap_colours(swapped,b);minimax(swapped,out,O_SIGN);}}

struct PlannerCand {unsigned v;int x,y;};

static int cmp_cand(const void *a,const void *b)
{unsigned xa=((const PlannerCand *)a)->v,xb=((const PlannerCand *)b)->v;
return xa<xb?1:xa>xb?-1:0;}

/* 1 when colour `me` can force five within `depth` of its own moves.
   The other side's reply is engine_square(), one move, not a branch. */
static int planner_search(tomb b,int me,int depth,int top,int *bx,int *by)
{static tomb mine[5],theirs[5];
PlannerCand c[MAXX*MAXY];
int ai=opponent(me),n=0,i,j,lim,k;
unsigned maxai=0;
engine_scores(me,b,mine[depth]);
engine_scores(ai,b,theirs[depth]);
for(i=0;i<MAXX;i++)for(j=0;j<MAXY;j++)
  if(!b[i][j]&&theirs[depth][i][j]>maxai)maxai=theirs[depth][i][j];
for(i=0;i<MAXX;i++)for(j=0;j<MAXY;j++)
{
  unsigned m,t,v;
  if(b[i][j])continue;
  m=mine[depth][i][j];
  t=theirs[depth][i][j];
  if(m==PM){if(bx){*bx=i;*by=j;}return 1;}
  if(maxai==PM){if(t==PM){c[n].v=1;c[n].x=i;c[n].y=j;n++;}continue;}
  if(top){v=m>t?m:t;if(v){c[n].v=v;c[n].x=i;c[n].y=j;n++;}}
  else if(m>=P6-1){c[n].v=m;c[n].x=i;c[n].y=j;n++;}
}
if(depth<=1)return 0;
qsort(c,(size_t)n,sizeof(PlannerCand),cmp_cand);
lim=maxai==PM?n:(top?TOPK:THREATK);
for(k=0;k<n&&k<lim;k++)
{
  int x=c[k].x,y=c[k].y,rx,ry,win=0;
  b[x][y]=(tombelem)me;
  if(engine_square(ai,b,&rx,&ry))
  {
    b[rx][ry]=(tombelem)ai;
    if(!made_five(b,rx,ry,ai))win=planner_search(b,me,depth-1,0,NULL,NULL);
    b[rx][ry]=0;
  }
  b[x][y]=0;
  if(win){if(bx){*bx=x;*by=y;}return 1;}
}
return 0;}

/* Computer's move. Sets 1-based (*x,*y), the same convention as pointto_m().
   Returns 0 when no empty square remains. Tries a win in 2 of its own moves,
   then 3, then 4, and stops at the first depth that works, so it takes the
   shortest win it can find within g_depth. */
static int planner_move(int *x,int *y)
{static tomb work;
int depth,bx,by;
for(depth=2;depth<=g_depth;depth++)
{
  memcpy(work,ptomb,sizeof work);
  if(planner_search(work,O_SIGN,depth,1,&bx,&by))
  {
    *x=bx+1;*y=by+1;
    return 1;
  }
}
return engine_square(O_SIGN,ptomb,&bx,&by)?(*x=bx+1,*y=by+1,1):0;}

static int parse_depth(int argc,char **argv)
{if(argc==1)return 3;
if(argc==2&&(strcmp(argv[1],"3")==0||strcmp(argv[1],"4")==0))return argv[1][0]-'0';
fprintf(stderr,"usage: amoeba_lookahead [3|4]\n");
exit(EXIT_FAILURE);}

int main(int argc,char **argv)
{int i,j,x,y,winner,next,result,aborted,exit_game=0;
int ch;
g_depth=parse_depth(argc,argv);
init_rng();
do
{x=MAXX/2+1;y=MAXY/2;
round_n=0;
game_n++;
for(i=0;i<MAXX;i++)for(j=0;j<MAXY;j++)
{
  ptomb[i][j]=0;wtomb1[i][j]=0;wtomb2[i][j]=0;
}
tableout(MAXY,MAXX);
info(game_n,round_n,x_won,o_won);
next=starting_player;
aborted=0;
while(!(winner=game_result(ptomb)))
{if(next==X_SIGN)
        {result=pointto(&x,&y,ptomb);
        if(result==MOVE_QUIT){aborted=1;break;}
        if(result==MOVE_LOADED)continue;
        putx(&x,&y,ptomb);}
else {if(!planner_move(&x,&y))continue;
        puto(&x,&y,ptomb);}
if(next!=starting_player)round_n++;
info(game_n,round_n,x_won,o_won);
next=opponent(next);
}
gotoxy(60,6);
textcolor(LIGHTCYAN);
cprintf("WINNER: ");
switch(winner)
{
case X_SIGN:
        textcolor(LIGHTBLUE);
        cprintf("X");
        x_won++;
        starting_player=O_SIGN;
        break;
case O_SIGN:
        textcolor(LIGHTRED);
        cprintf("O");
        o_won++;
        starting_player=X_SIGN;
        break;
default:cprintf("%s",aborted?"- (aborted)":"- (draw)");
}
info(game_n,round_n,x_won,o_won);
textcolor(LIGHTGRAY);
textbackground(DARKGRAY);
gotoxy(60,20);
printf("NEW GAME?(y/n)");
do
{
ch=readkey();
}while(ch!='n'&&ch!='y');
if(ch=='n')exit_game=1;
}while(!exit_game);
printf("\033[0;100m");
clrscr();
return 0;}

/*      AMOEBA program - Major Marcell
  utolso modositas: 1998.12.08-1999.01.10
  amoeba.cpp: AMOEBA.C with its known defects fixed, see README.md*/
#include <stdio.h>
#include "conio_fixed.h"
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define MAXPLAYERS 2
#define MAXX 24
#define MAXY 23
#define MAX_SAVES 5
#define MAX_SAVENAME_LEN 8
#define X_SIGN 1
#define O_SIGN 2
#define DRAW 3
#define IN_A_ROW 5
#define LINE_LEN 64
#define MAX_COUNTER 1000000000

/* PF scores every four with one winning square: blocked, broken, or both. Only an open four
   (P7) has two. PF is one more than an open three (P6). On their own, a double four beats a
   four-three, which beats a double three, which beats a lone four, which beats an open three;
   a four-three whose three is broken only ties a double three. These steps are one point
   wide, so a lesser pattern in another direction can reverse them. The step from two threats
   to one is wide: a square with two threes or fours always outscores a square with one.
   In self-play this beat larger values that keep every step in order. */
unsigned int P0=1,P1=5,P2=21,P3=85,P4=341,P5=1365,P6=5461,
             P7=21845,PF=5462,PM=UINT_MAX;

typedef unsigned int tombelem;
typedef tombelem tomb[MAXX][MAXY];
tomb wtomb1,wtomb2,ptomb;

int game_n=0,round_n,x_won=0,o_won=0;
int starting_player=X_SIGN;

enum {MOVE_PLACED,MOVE_QUIT,MOVE_LOADED};
enum {LOAD_OK,LOAD_NOFILE,LOAD_BAD};

typedef struct
{
  char name[MAX_SAVENAME_LEN+1];
  int game_n,round_n,x_won,o_won,starting_player;
  tomb board;
} savestate;

int on_board(int x,int y)
{return x>=0&&x<MAXX&&y>=0&&y<MAXY;}

int opponent(int ntype)
{return ntype==X_SIGN?O_SIGN:X_SIGN;}

/* getch() that leaves the game when the input is closed instead of spinning on EOF */
int readkey()
{int c=getch();
if(c==EOF)
  {
    textcolor(LIGHTGRAY);
    textbackground(BLACK);
    clrscr();
    exit(EXIT_FAILURE);
  }
return c;}

void tableout(int MaxSor,int MaxOszl)
{int i,j;
char outc;
clrscr();
textcolor(LIGHTMAGENTA);
gotoxy(60,1);
cprintf("MENU");
textcolor(YELLOW);
gotoxy(60,3);
cprintf("PLAY:SPACE+ARROWS");
gotoxy(60,4);
cprintf("LOAD:\'l\'");
gotoxy(60,5);
cprintf("SAVE:\'o\'");
gotoxy(60,6);
cprintf("QUIT:\'q\'");
textcolor(LIGHTGRAY);
gotoxy(1,1);
for(i=0;i<MaxSor;++i){
        for(j=0;j<MaxOszl;++j){
        cprintf("-+");
        }
        textcolor(LIGHTCYAN);
        putch('#');
        cprintf("%2d.",i+1);
        textcolor(LIGHTGRAY);
        putch('\n');
}
textcolor(LIGHTCYAN);
gotoxy(1,25);
for(i=0;i<MaxOszl;++i){outc='A'+i;cprintf(" %c",outc);}
textcolor(LIGHTGRAY);}

/* putx()/puto() place a stone at the 1-based square (*x,*y); they return 0 and change
   nothing if the square is off the board or already taken */
int putx(int *x,int *y,tomb pt)
{if(!on_board(*x-1,*y-1)||pt[*x-1][*y-1])return 0;
gotoxy(*x*2,*y);
textcolor(LIGHTBLUE);
cprintf("X");
gotoxy(*x,*y);
textcolor(LIGHTGRAY);
pt[*x-1][*y-1]=X_SIGN;
return 1;}

int puto(int *x,int *y,tomb pt)
{if(!on_board(*x-1,*y-1)||pt[*x-1][*y-1])return 0;
gotoxy(*x*2,*y);
textcolor(LIGHTRED);
cprintf("O");
gotoxy(*x,*y);
textcolor(LIGHTGRAY);
pt[*x-1][*y-1]=O_SIGN;
return 1;}

/* same argument order as tableout(): rows, then columns */
void draw_moves(int MaxSor,int MaxOszl)
{
  int i,j;

  for(i=0;i<MaxOszl;++i)
    for(j=0;j<MaxSor;++j)
    {
      if(ptomb[i][j]==O_SIGN)
      {
        gotoxy((i+1)*2,j+1);
        textcolor(LIGHTRED);
        cprintf("O");
        //gotoxy(*x,*y);
        textcolor(LIGHTGRAY);
      }
      else if(ptomb[i][j]==X_SIGN)
      {
        gotoxy((i+1)*2,j+1);
        textcolor(LIGHTBLUE);
        cprintf("X");
        //gotoxy(i,j);
        textcolor(LIGHTGRAY);
      }
    }
}

int check_space(tomb w,int posx,int posy,int ntype,int dx,int dy)
{int n=0,i;
tombelem etype=opponent(ntype);
for (i=0;i<IN_A_ROW&&on_board(posx+dx*i,posy+dy*i)&&w[posx+dx*i][posy+dy*i]!=etype;++i)++n;
for (i=1;i<IN_A_ROW&&on_board(posx-dx*i,posy-dy*i)&&w[posx-dx*i][posy-dy*i]!=etype;++i)++n;
if(n>=6)return 6;
else if(n==5)return n;
else return 0;}

int neighbours(tomb w,int posx,int posy,int ntype,int dx,int dy)
{int n=0,i;
for(i=0;i<IN_A_ROW&&on_board(posx+dx*i,posy+dy*i)&&w[posx+dx*i][posy+dy*i]==(tombelem)ntype;++i)++n;
for(i=1;i<IN_A_ROW&&on_board(posx-dx*i,posy-dy*i)&&w[posx-dx*i][posy-dy*i]==(tombelem)ntype;++i)++n;
return n;}

/* own stones in an unbroken line starting one step from (x,y) towards (dx,dy), at most IN_A_ROW-1 */
int run_from(tomb w,int x,int y,int ntype,int dx,int dy)
{int n=0;
while(n<IN_A_ROW-1&&on_board(x+dx*(n+1),y+dy*(n+1))&&w[x+dx*(n+1)][y+dy*(n+1)]==(tombelem)ntype)++n;
return n;}

/* blocked ends of a pattern whose outside neighbours are (ax,ay) and (bx,by): an end is
   blocked by an opponent stone or by the edge of the board */
int locks(tomb w,int ax,int ay,int bx,int by,int etype)
{int locked=0;
if(!on_board(ax,ay)||w[ax][ay]==(tombelem)etype)locked++;
if(!on_board(bx,by)||w[bx][by]==(tombelem)etype)locked++;
return locked;}

/* The pattern ntype makes along (dx,dy) by playing the empty square (posx,posy).
   side==0:  the unbroken run through the square; returns its length 1-4, or 5 for five or more.
   side==1 or -1: the run, then one empty gap ahead (1) or behind (-1), then more own stones.
     Counting the stones on both sides of the gap, 3 gives 8 (broken three: filling the gap
     makes four) and 4 or more gives 9 (broken four: filling the gap makes five). The scan
     stops at the first square that is not an own stone, so opponents, the edge and a
     second gap all end it. Returns 0 if there is no such pattern on that side.
   *lock gets the number of blocked ends of the pattern. */
int compdirection(tomb w,int posx,int posy,int ntype,int dx,int dy,int side,int *lock)
{int ahead,behind,n,e,o,gx,gy,g,etype=opponent(ntype);
ahead=run_from(w,posx,posy,ntype,dx,dy);
behind=run_from(w,posx,posy,ntype,-dx,-dy);
n=1+ahead+behind;
if(!side)
        {*lock=locks(w,posx+dx*(ahead+1),posy+dy*(ahead+1),posx-dx*(behind+1),posy-dy*(behind+1),etype);
        return n>=IN_A_ROW?5:n;}
*lock=0;
if(n>=IN_A_ROW-1)return 0;      /* already a four or five: a gap adds nothing */
e=side>0?ahead:behind;          /* run length on the gap side */
o=side>0?behind:ahead;          /* and on the other side */
gx=posx+side*dx*(e+1);gy=posy+side*dy*(e+1);
if(!on_board(gx,gy)||w[gx][gy])return 0;
g=run_from(w,gx,gy,ntype,side*dx,side*dy);
if(g==0||n+g<3)return 0;
*lock=locks(w,gx+side*dx*(g+1),gy+side*dy*(g+1),posx-side*dx*(o+1),posy-side*dy*(o+1),etype);
return n+g>=4?9:8;}

/* Score of one direction. space is check_space(), lock and code come from compdirection().
   A free four (code 4 or 9, no blocked end) always has at least 6 squares of room, so it
   cannot occur with space 5 and has no entry there. Of all the fours, only the free unbroken
   one has two winning squares and scores P7; the rest score PF. */
tombelem direction_score(int space,int lock,int code)
{
switch(space)
{
case 6:
if(!lock)
        {switch(code)
                {case 9:return PF;
                case 8:return P6-1;
                case 5:return PM;
                case 4:return P7;
                case 3:return P6;
                case 2:return P3;}}
else {switch(code)
        {case 9:return PF;
        case 8:return P4-1;
        case 5:return PM;
        case 4:return PF;
        case 3:return P4;
        case 2:return P1;}}
break;
case 5:
if(!lock)
        {switch(code)
                {case 8:return P4-1;
                case 5:return PM;
                case 3:return P4;
                case 2:return P2;}}
else {switch(code)
        {case 9:return PF;
        case 8:return P4-1;
        case 5:return PM;
        case 4:return PF;
        case 3:return P4;
        case 2:return P1;}}
break;}
return 0;}

/* value of playing nt on the empty square: each direction counts with its best reading */
tombelem compare(tomb playt,int x_coord,int y_coord,int nt)
{tombelem counter,best,s;
int lock,space,code,side,i;
static const int directions[4][2]={{0,1},{1,0},{1,1},{1,-1}};
counter=0;
for(i=0;i<4;i++)
{
space=check_space(playt,x_coord,y_coord,nt,directions[i][0],directions[i][1]);
best=0;
for(side=-1;side<=1;side++)
        {code=compdirection(playt,x_coord,y_coord,nt,directions[i][0],directions[i][1],side,&lock);
        s=direction_score(space,lock,code);
        if(s>best)best=s;}
if(best==PM)return PM;
counter+=best;
}
return counter;}

void minimax(tomb pt,tomb wt,int ntype)
{int i,j;
for(i=0;i<MAXX;i++)for(j=0;j<MAXY;j++)wt[i][j]=0;
for(i=0;i<MAXX;i++)for(j=0;j<MAXY;j++)if(pt[i][j]==0)wt[i][j]=compare(pt,i,j,ntype);
}

/* Lookahead. The scores above only see what one move makes. A four leaves the opponent one
   square to block, so a run of fours that ends with two squares to make five wins by force.
   A three is a move after which, moving again, the side would win by fours: the opponent
   must answer it, by blocking or with a four. The search looks for such forced wins, by
   fours alone and by threes and fours. The computer plays its own forced win, and when the
   player has one, it plays a square that takes it away.
   The search works on a flat copy of the board, sq = x*MAXY+y, and keeps, for every line of
   5 squares (a window), how many stones of each side it holds. Windows with at least 2
   stones of one side and none of the other are "live" for that side and kept in a list,
   so the search only looks at those. */
#define NSQ (MAXX*MAXY)
#define MAXWIN (4*NSQ)
#define FOUR_DEPTH 20           /* fours in a row */
#define THREE_DEPTH 2           /* threes in one forced win */
#define SEARCH_LINE (2*FOUR_DEPTH+2*THREE_DEPTH+8)
#define SEARCH_NODES 40000L     /* per search, so a move takes a fraction of a second */
#define DEFENCE_NODES (10*SEARCH_NODES) /* for checking the blocking squares, in all */

int nwin=0,wcell[MAXWIN][IN_A_ROW],wcount[MAXWIN][3];
int sqwin[NSQ][4*IN_A_ROW],nsqwin[NSQ];
int live[3][MAXWIN],nlive[3],livepos[3][MAXWIN];
int sb[NSQ],seen[NSQ],stamp=0;
long budget;

void build_windows()
{static const int d[4][2]={{0,1},{1,0},{1,1},{1,-1}};
int a,x,y,k,s;
for(a=0;a<4;a++)for(x=0;x<MAXX;x++)for(y=0;y<MAXY;y++)
        if(on_board(x+d[a][0]*(IN_A_ROW-1),y+d[a][1]*(IN_A_ROW-1)))
                {for(k=0;k<IN_A_ROW;k++)
                        {s=(x+d[a][0]*k)*MAXY+y+d[a][1]*k;
                        wcell[nwin][k]=s;
                        sqwin[s][nsqwin[s]++]=nwin;}
                nwin++;}}

/* puts window a on side c's live list, or takes it off, as its counts now require */
void update_live(int a)
{int c,p,last;
for(c=1;c<=2;c++)
        {int want=wcount[a][c]>=2&&!wcount[a][3-c];
        p=livepos[c][a];
        if(want&&p<0){livepos[c][a]=nlive[c];live[c][nlive[c]++]=a;}
        else if(!want&&p>=0)
                {last=live[c][--nlive[c]];
                live[c][p]=last;livepos[c][last]=p;livepos[c][a]=-1;}}}

void put(int s,int c)
{int i;
sb[s]=c;
for(i=0;i<nsqwin[s];i++){wcount[sqwin[s][i]][c]++;update_live(sqwin[s][i]);}}

void take(int s)
{int i,c=sb[s];
sb[s]=0;
for(i=0;i<nsqwin[s];i++){wcount[sqwin[s][i]][c]--;update_live(sqwin[s][i]);}}

void search_load(tomb pt)
{int x,y;
if(!nwin)build_windows();
memset(wcount,0,sizeof wcount);
memset(livepos,-1,sizeof livepos);
nlive[1]=nlive[2]=0;
for(x=0;x<MAXX;x++)for(y=0;y<MAXY;y++)
        {sb[x*MAXY+y]=0;
        if(pt[x][y])put(x*MAXY+y,(int)pt[x][y]);}}

/* starts a new set of squares for add_sq(); before the counter would overflow, it clears
   the marks and starts again from 1 */
void new_stamp()
{if(stamp==INT_MAX){memset(seen,0,sizeof seen);stamp=0;}
stamp++;}

/* appends s to out[n] if it is empty and not added since the last new_stamp() */
int add_sq(int s,int *out,int n)
{if(!sb[s]&&seen[s]!=stamp){seen[s]=stamp;out[n++]=s;}
return n;}

/* appends the empty squares of c's live windows holding lo..hi of its stones (lo >= 2):
   with 3..3 the squares where c makes a four, with 2..3 also its threes */
int collect(int c,int lo,int hi,int *out,int n)
{int i,k,a;
for(i=0;i<nlive[c];i++)
        {a=live[c][i];
        if(wcount[a][c]>=lo&&wcount[a][c]<=hi)
                for(k=0;k<IN_A_ROW;k++)n=add_sq(wcell[a][k],out,n);}
return n;}

/* the empty squares where c makes five, at most max of them */
int wins_of(int c,int *out,int max)
{int i,k,a,n=0;
new_stamp();
for(i=0;i<nlive[c]&&n<max;i++)
        {a=live[c][i];
        if(wcount[a][c]==IN_A_ROW-1)
                for(k=0;k<IN_A_ROW;k++)n=add_sq(wcell[a][k],out,n);}
return n;}

/* c to move. Returns the length of a forced win by fours, stored in line as c's moves and
   the opponent's blocks in turn, ending with the squares where c makes five; or 0. */
int win_by_fours(int c,int depth,int *line)
{int o=3-c,ow[2],nw,w[2],mv[NSQ],n,i,k,s;
if(--budget<0)return 0;
if(wins_of(c,line,1))return 1;
nw=wins_of(o,ow,2);
if(nw>1||!depth)return 0;
new_stamp();
n=collect(c,IN_A_ROW-2,IN_A_ROW-2,mv,0);
for(i=0;i<n;i++)
        {s=mv[i];
        if(nw==1&&s!=ow[0])continue;           /* the opponent's five must be blocked */
        put(s,c);
        if(wins_of(c,w,2)>1){take(s);line[0]=s;line[1]=w[0];line[2]=w[1];return 3;}
        put(w[0],o);
        k=win_by_fours(c,depth-1,line+2);
        take(w[0]);take(s);
        if(k){line[0]=s;line[1]=w[0];return k+2;}}
return 0;}

int win_by_threes(int c,int depth,int *line);

/* The opponent of c to move, after c's three at s, which threatens the win by fours in
   t[0..tk-1]. Returns 1 if some answer stops c, 0 if c wins against every answer tried:
   the squares of that win, the squares of the windows the three uses, and the opponent's
   fours. */
int three_answered(int c,int depth,int s,const int *t,int tk)
{int o=3-c,cand[NSQ],line[SEARCH_LINE],w[2],n=0,i,j,k,a,d,ok;
if(budget<0)return 1;
if(wins_of(o,w,1))return 1;                    /* the opponent makes five */
k=wins_of(c,w,2);
if(k>1)return 0;
if(k==1)                                       /* c has a four: one answer */
        {put(w[0],o);
        ok=win_by_threes(c,depth-1,line)>0;
        take(w[0]);
        return !ok;}
if(win_by_fours(o,FOUR_DEPTH,line))return 1;   /* the opponent wins first */
new_stamp();
for(i=0;i<tk;i++)
        {n=add_sq(t[i],cand,n);
        for(j=0;j<nsqwin[t[i]];j++)            /* a stone that turns a block into a four */
                {a=sqwin[t[i]][j];
                if(wcount[a][o]>=IN_A_ROW-3&&!wcount[a][c])
                        for(k=0;k<IN_A_ROW;k++)n=add_sq(wcell[a][k],cand,n);}}
for(i=0;i<nsqwin[s];i++)
        {a=sqwin[s][i];
        if(wcount[a][c]>=IN_A_ROW-2&&!wcount[a][o])
                for(k=0;k<IN_A_ROW;k++)n=add_sq(wcell[a][k],cand,n);}
n=collect(o,IN_A_ROW-2,IN_A_ROW-2,cand,n);
for(i=0;i<n;i++)
        {d=cand[i];
        put(d,o);
        k=wins_of(o,w,2);
        if(k>1)ok=0;                           /* the answer makes two fours */
        else if(k==1)                          /* a four: c blocks, then the three stands */
                {put(w[0],c);
                k=win_by_fours(c,FOUR_DEPTH,line);
                ok=k&&!three_answered(c,depth,s,line,k);
                take(w[0]);}
        else ok=win_by_threes(c,depth-1,line)>0;
        take(d);
        if(!ok)return 1;}
return 0;}

/* c to move. Like win_by_fours(), but c may also play threes, up to depth of them. Returns
   the length of the line (c's first move, then the win it threatens), or 0. */
int win_by_threes(int c,int depth,int *line)
{int o=3-c,mv[NSQ],w[2],n,i,k,s;
k=win_by_fours(c,FOUR_DEPTH,line);
if(k||depth<=0||budget<0)return k;
if(wins_of(o,w,1))return 0;                    /* no time for a three */
new_stamp();
n=collect(c,IN_A_ROW-3,IN_A_ROW-2,mv,0);
for(i=0;i<n&&budget>=0;i++)
        {s=mv[i];
        put(s,c);
        if(wins_of(c,w,1))                     /* a four: the opponent blocks */
                {put(w[0],o);
                k=win_by_threes(c,depth-1,line+2);
                take(w[0]);
                if(k){line[1]=w[0];k+=2;}}
        else
                {k=win_by_fours(c,FOUR_DEPTH,line+1);
                if(k&&three_answered(c,depth,s,line+1,k))k=0;
                if(k)k++;}
        take(s);
        if(k){line[0]=s;return k;}}
return 0;}

/* c to move: the forced win with the fewest threes that the search finds. Taking the
   shortest each move makes an attack head for its end: picking any win found let it switch
   to another, equally long win after every reply and never finish. (The search is limited,
   so a shorter win can be missed; this is how it behaves, not a proof that it ends.) */
int forced_win(int c,int *line)
{int d,k;
for(d=0;d<=THREE_DEPTH;d++)
        {budget=SEARCH_NODES;
        if((k=win_by_threes(c,d,line)))return k;}
return 0;}

/* Whether O at d stops the player's (X's) forced win, searching with the given nodes.
   Returns 1 if the search finds no forced win for the player, 0 if it finds one, -1 if it
   cannot tell: that proves nothing. Forced replies are played out first, keeping the turn
   order: if O has a four, X blocks it; if that block makes an X four, O blocks it; and so
   on. Then X moves and is searched. If X's block leaves O a free move, O is safe when X has
   no forced win even if O passed; otherwise it can't tell without searching O's move. */
int blocks_win(int d,long nodes)
{int line[SEARCH_LINE],w[2],v[2],placed[NSQ],np=0,r,k;
budget=nodes;
put(d,O_SIGN);placed[np++]=d;
for(;;)                                        /* X to move */
        {k=wins_of(O_SIGN,w,2);
        if(k>1){r=1;break;}                    /* two squares to make five: O wins */
        if(!k)
                {k=win_by_threes(X_SIGN,THREE_DEPTH,line);
                r=k?0:budget<0?-1:1;
                break;}
        put(w[0],X_SIGN);placed[np++]=w[0];     /* X blocks O's four; O to move */
        k=wins_of(X_SIGN,v,2);
        if(k>1){r=0;break;}                    /* the block makes two X fours */
        if(!k)                                 /* O moves freely */
                {k=win_by_threes(X_SIGN,THREE_DEPTH,line);
                r=k||budget<0?-1:1;
                break;}
        put(v[0],O_SIGN);placed[np++]=v[0];}    /* O blocks X's four */
while(np)take(placed[--np]);
return r;}

/* The computer's (O's) move from the lookahead, or 0 when nothing is forced. It plays its
   own forced win. Otherwise, if the player has one, it tries the squares that might stop it,
   best greedy score (wt1 + wt2) first, and plays the first one that blocks_win() finds
   safe. A square it couldn't tell about is only kept as a fallback, for when no square is
   found safe. All the checks share DEFENCE_NODES; each gets at most SEARCH_NODES of it. */
int lookahead_move(tomb pt,tomb wt1,tomb wt2,int *bx,int *by)
{int line[SEARCH_LINE],cand[NSQ],w[2],n=0,i,j,k,a,d,r,unsure=-1,o=O_SIGN,x=X_SIGN;
long spent=0;
search_load(pt);
if(wins_of(o,w,1)||wins_of(x,w,1))return 0;   /* the greedy scores handle fives */
if(forced_win(o,line)){*bx=line[0]/MAXY;*by=line[0]%MAXY;return 1;}
k=forced_win(x,line);
if(!k)return 0;
new_stamp();
for(i=0;i<k;i++)n=add_sq(line[i],cand,n);
for(i=0;i<nsqwin[line[0]];i++)
        {a=sqwin[line[0]][i];
        if(wcount[a][x]&&!wcount[a][o])for(j=0;j<IN_A_ROW;j++)n=add_sq(wcell[a][j],cand,n);}
n=collect(x,IN_A_ROW-3,IN_A_ROW-2,cand,n);
for(i=1;i<n;i++)                               /* best greedy score first */
        {d=cand[i];
        for(j=i;j>0&&wt1[cand[j-1]/MAXY][cand[j-1]%MAXY]+wt2[cand[j-1]/MAXY][cand[j-1]%MAXY]
                <wt1[d/MAXY][d%MAXY]+wt2[d/MAXY][d%MAXY];j--)cand[j]=cand[j-1];
        cand[j]=d;}
for(i=0;i<n&&spent<DEFENCE_NODES;i++)
        {d=cand[i];
        k=DEFENCE_NODES-spent<SEARCH_NODES?(int)(DEFENCE_NODES-spent):(int)SEARCH_NODES;
        r=blocks_win(d,k);
        spent+=k-budget;
        if(r>0){*bx=d/MAXY;*by=d%MAXY;return 1;}
        if(r<0&&unsure<0)unsure=d;}
if(unsure<0)return 0;
*bx=unsure/MAXY;*by=unsure%MAXY;
return 1;}

void info(int gn,int rn,int xn,int on)
{textcolor(LIGHTGREEN);
gotoxy(60,10);cprintf("GAME:  %3d.",gn);
gotoxy(60,11);cprintf("ROUND: %3d.",rn);
textcolor(LIGHTBLUE);
gotoxy(60,12);cprintf("X WON: %3d",xn);
gotoxy(60,13);cprintf("(player)");
textcolor(LIGHTRED);
gotoxy(60,14);cprintf("O WON: %3d",on);
gotoxy(60,15);cprintf("(computer)");
}

int whowins(tomb pt)
{int i,j,win=0,who;
for (who=1;who<=MAXPLAYERS;++who)
for (i=0;i<MAXX;++i)
for (j=0;j<MAXY;++j)
        {
        if(pt[i][j]==(tombelem)who)
                {
                if(neighbours(pt,i,j,who,1,1)>=IN_A_ROW){win=who;goto vege;}
                if(neighbours(pt,i,j,who,0,1)>=IN_A_ROW){win=who;goto vege;}
                if(neighbours(pt,i,j,who,1,0)>=IN_A_ROW){win=who;goto vege;}
                if(neighbours(pt,i,j,who,1,-1)>=IN_A_ROW){win=who;goto vege;}
                }
        }
vege:;
return (win);}

int stones(tomb pt)
{int i,j,n=0;
for(i=0;i<MAXX;i++)for(j=0;j<MAXY;j++)if(pt[i][j])n++;
return n;}

/* X_SIGN or O_SIGN if that side has five in a row, DRAW if the board is full, else 0 */
int game_result(tomb pt)
{int win=whowins(pt);
if(win)return win;
return stones(pt)==MAXX*MAXY?DRAW:0;}

void redraw_screen()
{
  tableout(MAXY,MAXX);
  draw_moves(MAXY,MAXX);
  info(game_n, round_n, x_won, o_won);
}

void message(const char *msg)
{
  gotoxy(1,24);
  clreol();
  textcolor(LIGHTRED);
  cprintf("%s",msg);
}

void wait_enter()
{
  int ch;

  do
  {
    ch=readkey();
  }while(ch!='\n'&&ch!='\r');
}

/* asks for a slot number and returns 1..MAX_SAVES */
int choose_slot(const char *prompt)
{
  char msg[LINE_LEN];
  int ch;

  snprintf(msg,sizeof msg,"%s (1-%d):",prompt,MAX_SAVES);
  message(msg);
  do
  {
    ch=readkey();
  }while(ch<'1'||ch>('0'+MAX_SAVES));
  return ch-'0';
}

void slot_filename(char *buf,size_t len,int slot)
{
  snprintf(buf,len,"save%d",slot);
}

/* Reads one line without its line ending. Returns 1 for a line, 0 at the end of the file,
   -1 for a line longer than the buffer (the rest of that line is skipped). */
int read_line(FILE *f,char *buf,size_t len)
{
  size_t n;
  int c;

  if(!fgets(buf,(int)len,f))return 0;
  n=strlen(buf);
  if(n>0&&buf[n-1]=='\n')buf[--n]=0;
  else if(!feof(f))
  {
    while((c=fgetc(f))!=EOF&&c!='\n');
    return -1;
  }
  if(n>0&&buf[n-1]=='\r')buf[--n]=0;
  return 1;
}

/* Parses a decimal integer at *p and moves *p past it. Fails without a digit or if the
   value is outside [min,max]. */
int parse_number(const char **p,long min,long max,int *out)
{
  char *end;
  long v;
  const char *s=*p;

  if(*s=='-')s++;
  if(*s<'0'||*s>'9')return 0;
  errno=0;
  v=strtol(*p,&end,10);
  if(errno||v<min||v>max)return 0;
  *out=(int)v;
  *p=end;
  return 1;
}

int valid_name(const char *s)
{
  size_t i,n=strlen(s);

  if(n>MAX_SAVENAME_LEN)return 0;
  for(i=0;i<n;i++)if(s[i]<'a'||s[i]>'z')return 0;
  return 1;
}

int load_error(FILE *ifp,char *err,size_t errlen,const char *filename,int lineno,const char *what)
{
  snprintf(err,errlen,"%s line %d: %s",filename,lineno,what);
  fclose(ifp);
  return LOAD_BAD;
}

/* Reads a save file into *st without touching the running game.
   Returns LOAD_OK, LOAD_NOFILE or LOAD_BAD; on failure err describes the problem. */
int parse_save(const char *filename,savestate *st,char *err,size_t errlen)
{
  static const char *fields[5]={"game number","round number","X win count","O win count","starting player"};
  int *values[5];
  char line[LINE_LEN];
  const char *p;
  FILE *ifp;
  int i,r,lineno=1,x,y,player;

  memset(st,0,sizeof *st);
  values[0]=&st->game_n;values[1]=&st->round_n;values[2]=&st->x_won;
  values[3]=&st->o_won;values[4]=&st->starting_player;
  ifp = fopen(filename, "r");
  if(ifp == NULL)
  {
    snprintf(err,errlen,"Cannot open %s",filename);
    return LOAD_NOFILE;
  }
  r=read_line(ifp,line,sizeof line);
  if(r==0)return load_error(ifp,err,errlen,filename,lineno,"file is empty");
  if(r<0||!valid_name(line))return load_error(ifp,err,errlen,filename,lineno,"bad name");
  strcpy(st->name,line);
  for(i=0;i<5;i++)
  {
    lineno++;
    r=read_line(ifp,line,sizeof line);
    p=line;
    if(r==0)
    {
      snprintf(line,sizeof line,"missing %s",fields[i]);
      return load_error(ifp,err,errlen,filename,lineno,line);
    }
    if(r<0||!parse_number(&p,i==4?X_SIGN:0,i==4?O_SIGN:MAX_COUNTER,values[i])||*p)
    {
      snprintf(line,sizeof line,"bad %s",fields[i]);
      return load_error(ifp,err,errlen,filename,lineno,line);
    }
  }
  while((r=read_line(ifp,line,sizeof line))!=0)
  {
    lineno++;
    if(r<0)return load_error(ifp,err,errlen,filename,lineno,"line too long");
    if(!line[0])continue;      /* the old loader skipped blank lines too */
    p=line;
    if(*p++!='['||!parse_number(&p,-MAX_COUNTER,MAX_COUNTER,&x)||*p++!=','
       ||!parse_number(&p,-MAX_COUNTER,MAX_COUNTER,&y)||*p++!=']'||*p++!='='
       ||!parse_number(&p,-MAX_COUNTER,MAX_COUNTER,&player)||*p)
      return load_error(ifp,err,errlen,filename,lineno,"stone must look like [x,y]=1");
    if(!on_board(x,y))return load_error(ifp,err,errlen,filename,lineno,"stone is off the board");
    if(player!=X_SIGN&&player!=O_SIGN)return load_error(ifp,err,errlen,filename,lineno,"player must be 1 or 2");
    if(st->board[x][y])return load_error(ifp,err,errlen,filename,lineno,"square is given twice");
    st->board[x][y]=player;
  }
  if(ferror(ifp))return load_error(ifp,err,errlen,filename,lineno,"read error");
  fclose(ifp);
  return LOAD_OK;
}

void apply_save(const savestate *st)
{
  game_n=st->game_n;
  round_n=st->round_n;
  x_won=st->x_won;
  o_won=st->o_won;
  starting_player=st->starting_player;
  memcpy(ptomb,st->board,sizeof ptomb);
  memset(wtomb1,0,sizeof wtomb1);
  memset(wtomb2,0,sizeof wtomb2);
}

void current_state(savestate *st,const char *name)
{
  memset(st,0,sizeof *st);
  snprintf(st->name,sizeof st->name,"%s",name);
  st->game_n=game_n;
  st->round_n=round_n;
  st->x_won=x_won;
  st->o_won=o_won;
  st->starting_player=starting_player;
  memcpy(st->board,ptomb,sizeof st->board);
}

/* Same format as the original program: name, five counters, then one [x,y]=player line per stone */
int write_save(const char *filename,const savestate *st,char *err,size_t errlen)
{
  FILE *ofp;
  int i,j,ok;

  ofp = fopen(filename, "w");
  if (ofp == NULL)
  {
    snprintf(err,errlen,"Can't open output file %s!",filename);
    return 0;
  }
  fprintf(ofp, "%s\n", st->name);
  fprintf(ofp, "%d\n", st->game_n);
  fprintf(ofp, "%d\n", st->round_n);
  fprintf(ofp, "%d\n", st->x_won);
  fprintf(ofp, "%d\n", st->o_won);
  fprintf(ofp, "%d\n", st->starting_player);
  for(i=0;i<MAXX;i++)for(j=0;j<MAXY;j++)
  {
    if(st->board[i][j]==O_SIGN)
    {
      fprintf(ofp, "[%d,%d]=%d\n", i, j, O_SIGN);
    }
    else if(st->board[i][j]==X_SIGN)
    {
      fprintf(ofp, "[%d,%d]=%d\n", i, j, X_SIGN);
    }
  }
  ok=!ferror(ofp);
  if(fclose(ofp)!=0)ok=0;
  if(!ok)snprintf(err,errlen,"Error writing %s!",filename);
  return ok;
}

void show_slots()
{
  savestate st;
  char filename[16],err[LINE_LEN*2];
  int i,r;

  for(i=0;i<MAX_SAVES;i++)
  {
    slot_filename(filename,sizeof filename,i+1);
    r=parse_save(filename,&st,err,sizeof err);
    gotoxy(20,7+i);
    cprintf("%d.:",i+1);
    if(r==LOAD_OK)cprintf("%s",st.name[0]?st.name:"(no name)");
    else if(r==LOAD_BAD)cprintf("(damaged)");
  }
}

void save_game()
{
  int ch,pos;
  char buf[MAX_SAVENAME_LEN+1];
  char filename[16],err[LINE_LEN*2],msg[LINE_LEN*3];
  savestate st;

  show_slots();
  slot_filename(filename,sizeof filename,choose_slot("Save to slot"));
  message("Saved state name(allowed characters are [a-z]):");
  pos=0;
  do
  {
    ch=readkey();
    if(ch>='a'&&ch<='z')
    {
      buf[pos++]=ch;
      cprintf("%c",ch);
    }else if(ch=='\n'||ch=='\r')
    {
      break;
    }
  }while(pos<MAX_SAVENAME_LEN);
  buf[pos]=0;
  current_state(&st,buf);
  if(!write_save(filename,&st,err,sizeof err))
  {
    snprintf(msg,sizeof msg,"%s (press enter)",err);
    message(msg);
    wait_enter();
  }
  redraw_screen();
}

/* Returns 1 if a saved game replaced the current one. On any error the current game is kept. */
int load_game()
{
  char filename[16],err[LINE_LEN*2],msg[LINE_LEN*3];
  savestate st;

  show_slots();
  slot_filename(filename,sizeof filename,choose_slot("Game to load"));
  if(parse_save(filename,&st,err,sizeof err)!=LOAD_OK)
  {
    snprintf(msg,sizeof msg,"%s (press enter)",err);
    message(msg);
    wait_enter();
    redraw_screen();
    return 0;
  }
  apply_save(&st);
  redraw_screen();
  return 1;
}

/* Human move. Returns MOVE_PLACED with the cursor on an empty square, MOVE_QUIT for 'q',
   or MOVE_LOADED when a saved game replaced the current one. */
int pointto(int *x,int *y,tomb pt)
{int c;
gotoxy(*x*2,*y);
do
{
c=readkey();
if(c==27)
  {
    c=readkey();
    if(c==91)
    {
      c=readkey();
      if(c==65&&*y>1)--*y;
      if(c==66&&*y<MAXY)++*y;
      if(c==68&&*x>1)--*x;
      if(c==67&&*x<MAXX)++*x;
    }
  }
if(c=='w'&&*y>1)--*y;
if(c=='s'&&*y<MAXY)++*y;
if(c=='a'&&*x>1)--*x;
if(c=='d'&&*x<MAXX)++*x;
if(c=='o')save_game();
if(c=='l'&&load_game())return MOVE_LOADED;
gotoxy(*x*2,*y);
}
while ((c!=' '||pt[*x-1][*y-1]!=0)&&c!='q');
return c=='q'?MOVE_QUIT:MOVE_PLACED;}

/* Computer move: sets the 1-based (*x,*y) to an empty square and returns 1,
   or returns 0 when the board has no empty square left. */
int pointto_m(int *x,int *y,tomb wt1,tomb wt2,tomb pt)
{int i,j,xx=-1,xy=-1,ox=-1,oy=-1;
tombelem max_x=0,max_o=0;
if(!stones(pt)){*x=13;*y=12;return 1;}  /* opening move on the empty board */
minimax(pt,wt1,X_SIGN);
minimax(pt,wt2,O_SIGN);
if(lookahead_move(pt,wt1,wt2,&i,&j)){*x=i+1;*y=j+1;return 1;}
for(i=0;i<MAXX;i++)for(j=0;j<MAXY;j++)
        {if(pt[i][j])continue;
        if(xx<0||wt1[i][j]>max_x){max_x=wt1[i][j];xx=i;xy=j;}
        else if(wt1[i][j]==max_x&&(rand()%5)==4){max_x=wt1[i][j];xx=i;xy=j;}}
for(i=0;i<MAXX;i++)for(j=0;j<MAXY;j++)
        {if(pt[i][j])continue;
        if(ox<0||wt2[i][j]>max_o){max_o=wt2[i][j];ox=i;oy=j;}
        else if(wt2[i][j]==max_o&&(rand()%5)==4){max_o=wt2[i][j];ox=i;oy=j;}}
if(xx<0)return 0;
if(max_x>max_o)
        {*x=xx+1;*y=xy+1;}
        else {*x=ox+1;*y=oy+1;}
return 1;}

/* AMOEBA_SEED=<number> fixes the tie-breaking sequence (for tests); otherwise use the clock */
void seed_rng()
{
  const char *s=getenv("AMOEBA_SEED");
  char *end;
  unsigned long v;

  if(s&&*s)
  {
    errno=0;
    v=strtoul(s,&end,10);
    if(!errno&&!*end){srand((unsigned)v);return;}
  }
  srand((unsigned)time(NULL));
}

#ifndef AMOEBA_NO_MAIN
int main (void)
{
  int    i,j,x,y,winner,next,result,aborted,exit_game=0;
  int    ch;
  seed_rng();
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
          if(result==MOVE_LOADED)continue;   /* saves are made on X's turn: X moves next */
          putx(&x,&y,ptomb);}
  else {if(!pointto_m(&x,&y,wtomb1,wtomb2,ptomb))continue;  /* full board: game_result() says draw */
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
  default:cprintf("%s",aborted?"- (aborted)":"- (draw)");   /* nobody scores, same starter */
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
  if (ch=='n')exit_game=1;
  }while (!exit_game);
  printf("\033[0;100m");
  clrscr();
  return 0;
}
#endif

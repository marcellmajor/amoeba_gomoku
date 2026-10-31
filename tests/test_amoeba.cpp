// Unit tests for amoeba.cpp (build and run commands are in README.md).
// The optional argument is the folder holding the original save1..save5; they are copied
// to a temporary folder and only the copies are used.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <string>

#define AMOEBA_NO_MAIN
#include "../amoeba.cpp"

static int checks=0,failures=0;
#define CHECK(c) do{++checks;if(!(c)){++failures;fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#c);}}while(0)
#define CHECK_EQ(a,b) do{++checks;long long a_=(long long)(a),b_=(long long)(b);if(a_!=b_){++failures;\
  fprintf(stderr,"FAIL %s:%d: %s is %lld, expected %lld\n",__FILE__,__LINE__,#a,a_,b_);}}while(0)

static char tmpdir[512];

static std::string tmp(const char *name){return std::string(tmpdir)+"/"+name;}

static void write_file(const std::string &path,const std::string &text)
{FILE *f=fopen(path.c_str(),"wb");
if(!f){perror(path.c_str());exit(2);}
fwrite(text.data(),1,text.size(),f);
fclose(f);}

static bool read_file(const std::string &path,std::string *text)
{FILE *f=fopen(path.c_str(),"rb");
char buf[4096];
size_t n;
if(!f)return false;
text->clear();
while((n=fread(buf,1,sizeof buf,f))>0)text->append(buf,n);
fclose(f);
return true;}

static void clear(tomb b){memset(b,0,sizeof(tomb));}

/* full board without five in a row on any axis */
static void fill_fixture(tomb b)
{for(int x=0;x<MAXX;x++)for(int y=0;y<MAXY;y++)b[x][y]=((x+2*y)%4<2)?X_SIGN:O_SIGN;}

/* Lays pat along (dx,dy) from (x0,y0): 'X', 'O', '.' empty, 'c' the empty square to score.
   Returns compare() for X at 'c'. */
static tombelem score_at(int x0,int y0,int dx,int dy,const char *pat)
{static tomb b;
int cx=-1,cy=-1;
clear(b);
for(int i=0;pat[i];i++)
        {int x=x0+dx*i,y=y0+dy*i;
        if(!on_board(x,y)){fprintf(stderr,"pattern %s leaves the board\n",pat);exit(2);}
        b[x][y]=pat[i]=='X'?X_SIGN:pat[i]=='O'?O_SIGN:0;
        if(pat[i]=='c'){cx=x;cy=y;}}
return compare(b,cx,cy,X_SIGN);}

static const int axes[4][2]={{0,1},{1,0},{1,1},{1,-1}};

static void test_empty_board()
{static tomb b;
int x=0,y=0,nonzero=0;
clear(b);
for(int i=0;i<MAXX;i++)for(int j=0;j<MAXY;j++)
        if(compare(b,i,j,X_SIGN)||compare(b,i,j,O_SIGN))nonzero++;
CHECK_EQ(nonzero,0);
CHECK_EQ(whowins(b),0);
CHECK_EQ(pointto_m(&x,&y,wtomb1,wtomb2,b),1);
CHECK_EQ(x,13);CHECK_EQ(y,12);}

/* contiguous and single-gap patterns in the middle of the board, on every axis */
static void test_patterns_all_axes()
{struct {const char *pat;tombelem expect;} cases[]={
        {"Xc",P3},{"XXc",P6},{"XXXc",P7},{"XXcX",P7},             /* free two, three, four; gap filled */
        {"XXXXc",PM},{"XXcXX",PM},{"XXXcXX",PM},                  /* five, also by overline */
        {"OXc",P1},{"OXXc",P4},{"OXXXc",PF},                      /* blocked by an opponent */
        {"O.XXc.O",P4},{"O.Xc..O",P2},{"OXXcO",0},{"OcX.XO",0},   /* exactly 5 squares of room, or too few */
        {"cX.X",P6-1},{"c.XX",P6-1},                              /* broken three */
        {"cXX.X",PF},{"XX.Xc",PF},{"cX.XX",PF},                   /* broken four */
        {"OcXX.X.",PF},{"OcXX.XO",PF},                            /* blocked broken four */
        {"cXX.XO",PF},             /* the blocked broken four beats the free-three reading */
        {"cX.X.X",P6-1},           /* a second gap ends the scan: three, not four */
        {"cX.OX",P3},              /* an opponent after the gap: plain two */
        {"X.c",0},{"c",0}};        /* X_X and a lone stone are not scored */
for(auto &t:cases)
        {int len=(int)strlen(t.pat);
        for(int a=0;a<4;a++)
                {int dx=axes[a][0],dy=axes[a][1];
                int x0=11-dx*(len/2),y0=11-dy*(len/2);
                tombelem got=score_at(x0,y0,dx,dy,t.pat);
                if(got!=t.expect)fprintf(stderr,"  pattern %s on axis (%d,%d)\n",t.pat,dx,dy);
                CHECK_EQ(got,t.expect);}}}

/* the edge of the board closes a run, a free square before the edge does not */
static void test_edges_and_corners()
{CHECK_EQ(score_at(0,11,1,0,"XXXc."),PF);
CHECK_EQ(score_at(0,11,1,0,".XXXc."),P7);
CHECK_EQ(score_at(0,11,1,0,"XXXc.O"),PF);     /* edge-closed four with exactly room for five */
CHECK_EQ(score_at(0,11,1,0,"cX.X"),P4-1);
CHECK_EQ(score_at(20,11,1,0,"cXXX"),PF);
CHECK_EQ(score_at(20,11,1,0,"XX.c"),P4-1);
CHECK_EQ(score_at(11,0,0,1,"XXXc"),PF);
CHECK_EQ(score_at(11,19,0,1,"cXXX"),PF);
CHECK_EQ(score_at(0,0,1,1,"XXXc"),PF);
CHECK_EQ(score_at(0,0,1,1,"cXXX"),PF);
CHECK_EQ(score_at(20,19,1,1,"cXXX"),PF);
CHECK_EQ(score_at(0,22,1,-1,"XXXc"),PF);
CHECK_EQ(score_at(20,3,1,-1,"cXXX"),PF);
CHECK_EQ(score_at(1,1,1,1,".XXXc"),P7);}

/* Lays one pattern per axis (NULL leaves that axis empty) so that each pattern's 'c' falls on
   (cx,cy). Letters as in score_at(). */
static void lay_cross(tomb b,int cx,int cy,const char *const pats[4])
{for(int a=0;a<4;a++)
        {const char *p=pats[a];
        if(!p)continue;
        int c=(int)(strchr(p,'c')-p);
        for(int i=0;p[i];i++)
                {int x=cx+axes[a][0]*(i-c),y=cy+axes[a][1]*(i-c);
                if(!on_board(x,y)){fprintf(stderr,"pattern %s leaves the board\n",p);exit(2);}
                b[x][y]=p[i]=='X'?X_SIGN:p[i]=='O'?O_SIGN:0;}}}

/* lay_cross() through (11,11), scored for X there */
static tombelem score_cross(const char *const pats[4])
{static tomb b;
clear(b);
lay_cross(b,11,11,pats);
return compare(b,11,11,X_SIGN);}

/* Patterns in several directions add up. On their own, each combination ranks one point or
   more above the next, and any two threats outscore any single one. */
static void test_combination_ranking()
{struct {const char *what;const char *pats[4];tombelem expect;} ranks[]={
        {"open four",{".XXXc.",NULL,NULL,NULL},P7},
        {"double four",{"OXXXc.",".XX.cX.",NULL,NULL},2*PF},
        {"four-three",{"OXXXc.",".XXc..",NULL,NULL},PF+P6},
        {"double three",{".XXc..",".XXc..",NULL,NULL},2*P6},
        {"four",{".XX.cX.",NULL,NULL,NULL},PF},
        {"open three",{".XXc..",NULL,NULL,NULL},P6}};
static const char *const four_broken_three[4]={".XX.cX.","..cX.X.",NULL,NULL};
static const char *const weakest_pair[4]={"..cX.X.","..cX.X.",NULL,NULL};
static const char *const strongest_single[4]={"OXXXc.","OXXc","OXXc","OXXc"};
tombelem prev=PM;
for(auto &r:ranks)
        {tombelem got=score_cross(r.pats);
        if(got!=r.expect||got>=prev)fprintf(stderr,"  combination: %s\n",r.what);
        CHECK_EQ(got,r.expect);
        CHECK(got<prev);
        prev=got;}
CHECK_EQ(score_cross(four_broken_three),2*P6);     /* only ties a double three */
/* two broken threes against a four with a blocked three in every other direction */
CHECK_EQ(score_cross(weakest_pair),2*(P6-1));
CHECK_EQ(score_cross(strongest_single),PF+3*P4);
CHECK(score_cross(weakest_pair)>score_cross(strongest_single));}

/* The computer prefers its own four-three to blocking a double three, and blocks a
   four-three rather than making its own double three. */
static void test_combination_moves()
{static tomb b;
static const char *const four_three_o[4]={"XOOOc.",".OOc..",NULL,NULL};
static const char *const double_three_x[4]={".XXc..",".XXc..",NULL,NULL};
static const char *const four_three_x[4]={"OXXXc.",".XXc..",NULL,NULL};
static const char *const double_three_o[4]={".OOc..",".OOc..",NULL,NULL};
int x,y;
clear(b);
lay_cross(b,11,11,four_three_o);
lay_cross(b,4,4,double_three_x);
CHECK_EQ(compare(b,11,11,O_SIGN),PF+P6);
CHECK_EQ(compare(b,4,4,X_SIGN),2*P6);
srand(1);
CHECK_EQ(pointto_m(&x,&y,wtomb1,wtomb2,b),1);
CHECK_EQ(x,12);CHECK_EQ(y,12);
clear(b);
lay_cross(b,11,11,four_three_x);
lay_cross(b,4,4,double_three_o);
CHECK_EQ(compare(b,11,11,X_SIGN),PF+P6);
CHECK_EQ(compare(b,4,4,O_SIGN),2*P6);
srand(1);
CHECK_EQ(pointto_m(&x,&y,wtomb1,wtomb2,b),1);
CHECK_EQ(x,12);CHECK_EQ(y,12);}

/* Lays rows of 'X', 'O' and '.' with the first row's first square at (x0,y0). */
static void lay_map(tomb b,int x0,int y0,const char *const rows[],int nrows)
{for(int r=0;r<nrows;r++)for(int i=0;rows[r][i];i++)
        b[x0+i][y0+r]=rows[r][i]=='X'?X_SIGN:rows[r][i]=='O'?O_SIGN:0;}

static tombelem board_max(tomb w,tomb b)
{tombelem m=0;
for(int i=0;i<MAXX;i++)for(int j=0;j<MAXY;j++)if(!b[i][j]&&w[i][j]>m)m=w[i][j];
return m;}

/* 1 if X has a forced win on b with X to move */
static int x_forced(tomb b)
{int line[SEARCH_LINE];
search_load(b);
return forced_win(X_SIGN,line)>0;}

/* Positions where the greedy scores alone pick a losing move and the lookahead does not. */
static void test_lookahead_moves()
{static tomb b;
static const char *const four_three_o[4]={"XOOOc.",".OOc..",NULL,NULL};
static const char *const double_three_o[4]={".OOc..",".OOc..",NULL,NULL};
static const char *const triple_three_x[4]={".XXc..",".XXc..",NULL,".XXc.."};
static const char *const open_three_x[]={".XXX."};
static const char *const open_three[]={  /* rows 10-14 from x=9 */
        ".....X.",
        "O..O.X.",
        ".....XO",
        ".......",
        "..XX..."};
static const char *const threes_x[]={    /* rows 11-15 from x=9 */
        ".X",
        "XX",
        "..",
        "..",
        ".X"};
int x,y;
/* its own win by fours (a four-three) beats blocking X's open three */
clear(b);
lay_cross(b,11,11,four_three_o);
lay_map(b,2,3,open_three_x,1);
srand(1);
CHECK_EQ(pointto_m(&x,&y,wtomb1,wtomb2,b),1);
CHECK_EQ(x,12);CHECK_EQ(y,12);
CHECK(board_max(wtomb1,b)>board_max(wtomb2,b));    /* the greedy scores would block */
/* its own double three beats blocking X's triple-three square: its threes come first */
clear(b);
lay_cross(b,11,11,double_three_o);
lay_cross(b,5,5,triple_three_x);
srand(1);
CHECK_EQ(pointto_m(&x,&y,wtomb1,wtomb2,b),1);
CHECK_EQ(x,12);CHECK_EQ(y,12);
CHECK(board_max(wtomb1,b)>board_max(wtomb2,b));
/* which end of X's open three to block: the top end (14,9) still loses */
clear(b);
lay_map(b,9,10,open_three,5);
CHECK(x_forced(b));
srand(1);
CHECK_EQ(pointto_m(&x,&y,wtomb1,wtomb2,b),1);
CHECK_EQ(x,15);CHECK_EQ(y,14);                      /* (14,13) */
b[14][9]=O_SIGN;CHECK(x_forced(b));b[14][9]=0;
b[14][13]=O_SIGN;CHECK(!x_forced(b));b[14][13]=0;
/* X wins with threes, starting at (8,13); the greedy block below the pair at (10,13) still
   loses, (11,12) beside it does not */
clear(b);
lay_map(b,9,11,threes_x,5);
b[14][13]=O_SIGN;
CHECK(x_forced(b));
srand(1);
CHECK_EQ(pointto_m(&x,&y,wtomb1,wtomb2,b),1);
CHECK_EQ(x,12);CHECK_EQ(y,13);                      /* (11,12) */
b[10][13]=O_SIGN;CHECK(x_forced(b));b[10][13]=0;
b[11][12]=O_SIGN;CHECK(!x_forced(b));b[11][12]=0;
/* a square where O makes a four also counts as a block: X must answer the four first.
   (10,13) blocks X's open three and makes O's four, so it beats the other end */
clear(b);
b[10][10]=b[10][11]=b[10][12]=X_SIGN;
b[11][13]=b[12][13]=b[13][13]=O_SIGN;b[14][13]=X_SIGN;
CHECK(x_forced(b));
srand(1);
CHECK_EQ(pointto_m(&x,&y,wtomb1,wtomb2,b),1);
CHECK_EQ(x,11);CHECK_EQ(y,14);
search_load(b);
CHECK_EQ(blocks_win(10*MAXY+13,SEARCH_NODES),1);
CHECK_EQ(blocks_win(5*MAXY+5,SEARCH_NODES),0);      /* far away: X still wins */
CHECK_EQ(blocks_win(10*MAXY+13,5),-1);              /* too few nodes: not known, not safe */
/* the generation counter for add_sq() wraps instead of overflowing */
stamp=INT_MAX;
CHECK(x_forced(b));
CHECK(stamp>0&&stamp<1000000);}

static int near_stone(int s)
{int x=s/MAXY,y=s%MAXY;
for(int dx=-4;dx<=4;dx++)for(int dy=-4;dy<=4;dy++)
        if(on_board(x+dx,y+dy)&&sb[(x+dx)*MAXY+y+dy])return 1;
return 0;}

static int has_five(int c)
{for(int i=0;i<nlive[c];i++)if(wcount[live[c][i]][c]==IN_A_ROW)return 1;
return 0;}

/* Checks a forced win the long way, on the search board: c to move wins with at most d
   threes against every reply. c picks each move with a fresh search, as the game does,
   and blocks a five of the defender's first, as the greedy scores do. Replies farther than
   4 from every stone share no window with one and change nothing, so they are left out.
   *unsure is set when a search runs out of nodes. */
static int holds_reply(int c,int d,int plies,int *unsure);
static int holds(int c,int d,int plies,int *unsure)
{int line[SEARCH_LINE],w[2],k,s,r;
if(plies<=0){*unsure=1;return 0;}
if(wins_of(3-c,w,1)&&!wins_of(c,line,1))
        {if(wins_of(3-c,w,2)>1)return 0;
        put(w[0],c);r=holds_reply(c,d,plies-1,unsure);take(w[0]);return r;}
budget=50*SEARCH_NODES;
k=win_by_fours(c,FOUR_DEPTH,line);
if(!k&&d>0){budget=50*SEARCH_NODES;if((k=win_by_threes(c,d,line)))d--;}
if(!k){if(budget<0)*unsure=1;return 0;}
s=line[0];put(s,c);
r=has_five(c)||holds_reply(c,d,plies-1,unsure);
take(s);
return r;}

/* the defender (3-c) to move */
static int holds_reply(int c,int d,int plies,int *unsure)
{int o=3-c,w[2],cw,r,ok;
if(wins_of(o,w,1))return 0;
cw=wins_of(c,w,2);
if(cw>1)return 1;
for(r=0;r<NSQ;r++)
        {if(sb[r]||(cw==1&&r!=w[0])||!near_stone(r))continue;
        put(r,o);
        ok=!has_five(o)&&holds(c,d,plies,unsure);
        take(r);
        if(!ok)return 0;}
return 1;}

/* On random positions, every forced win the search finds holds against every reply. */
static void test_forced_wins_hold()
{static tomb b;
int claims=0,with_threes=0,refuted=0,unsure_n=0;
srand(3);
for(int p=0;p<150;p++)
        {clear(b);
        int n=10+rand()%18,w[2];
        for(int i=0;i<n;i++)b[7+rand()%11][6+rand()%11]=1+rand()%2;
        search_load(b);
        if(has_five(1)||has_five(2)||wins_of(1,w,1)||wins_of(2,w,1))continue;
        for(int c=1;c<=2;c++)
                {int line[SEARCH_LINE],d,k=0,unsure=0;
                search_load(b);
                for(d=0;d<=THREE_DEPTH&&!k;d++){budget=SEARCH_NODES;k=win_by_threes(c,d,line);}
                if(!k)continue;
                d--;
                claims++;
                if(d)with_threes++;
                if(!holds(c,d,200,&unsure)){if(unsure)unsure_n++;else refuted++;}}}
CHECK_EQ(refuted,0);
CHECK(claims>=50);
CHECK(with_threes>=15);
CHECK(unsure_n*50<=claims);}

/* the search board's stones, window counts and live lists, to check that they come back */
static int sb_copy[NSQ],wc_copy[MAXWIN][3],nl_copy[3];
static void snapshot()
{memcpy(sb_copy,sb,sizeof sb);memcpy(wc_copy,wcount,sizeof wcount);
nl_copy[1]=nlive[1];nl_copy[2]=nlive[2];}
static int restored()
{if(memcmp(sb_copy,sb,sizeof sb)||memcmp(wc_copy,wcount,sizeof wcount))return 0;
for(int c=1;c<=2;c++)
        {if(nlive[c]!=nl_copy[c])return 0;
        for(int i=0;i<nlive[c];i++)if(livepos[c][live[c][i]]!=i)return 0;}
return 1;}

/* blocks_win() plays out forced replies in turn order, on every path, and puts the board
   back each time */
static void test_blocks_win()
{static tomb b;
struct {const char *what;int x,y;long nodes;int expect;} cases[]={
        {"O's four, X's block makes an X four, O blocks it",10,11,SEARCH_NODES,1},
        {"far away: X still wins by fours",3,3,SEARCH_NODES,0}};
/* X at 6-8 and 14, O at 11-13 on row 11: O at (10,11) makes a four, X must block at (9,11),
   which makes an X four, so O blocks at (5,11) and X's attack is gone */
clear(b);
b[6][11]=b[7][11]=b[8][11]=b[14][11]=X_SIGN;
b[11][11]=b[12][11]=b[13][11]=O_SIGN;
CHECK(x_forced(b));
for(auto &t:cases)
        {search_load(b);snapshot();
        int r=blocks_win(t.x*MAXY+t.y,t.nodes);
        if(r!=t.expect)fprintf(stderr,"  blocks_win: %s\n",t.what);
        CHECK_EQ(r,t.expect);
        CHECK(restored());}
/* X also has three in column 9 above the block: blocking O's four makes two X fours */
b[9][8]=b[9][9]=b[9][10]=X_SIGN;
search_load(b);snapshot();
CHECK_EQ(blocks_win(10*MAXY+11,SEARCH_NODES),0);
CHECK(restored());
/* O's four far from X's open three: after X blocks it, O moves freely, but X would still
   win if O passed, so it can't tell */
clear(b);
b[10][10]=b[10][11]=b[10][12]=X_SIGN;
b[15][5]=b[16][5]=b[17][5]=O_SIGN;b[18][5]=X_SIGN;
search_load(b);snapshot();
CHECK_EQ(blocks_win(14*MAXY+5,SEARCH_NODES),-1);
CHECK(restored());
/* an open four for O: two squares to make five */
b[18][5]=0;
search_load(b);snapshot();
CHECK_EQ(blocks_win(14*MAXY+5,SEARCH_NODES),1);
CHECK(restored());}

/* Every arrangement of the 8 squares within distance 4 of a square, on every axis and
   next to every edge and corner: the codes are the defined ones, a free four never shows
   up with only 5 squares of room, and every live pattern has a score. */
static void test_exhaustive_lines()
{static tomb b;
int pos[64][2],npos=0;
long cases=0,bad=0,seen8=0,seen9=0;
clear(b);
for(int a=0;a<4;a++)
        {int dx=axes[a][0],dy=axes[a][1];
        npos=0;
        for(int k=0;k<5;k++)
                {int lo=k,hx=MAXX-1-k,hy=MAXY-1-k;
                int p[][2]={{lo,lo},{lo,hy},{hx,lo},{hx,hy},{lo,11},{hx,11},{11,lo},{11,hy}};
                for(auto &q:p){pos[npos][0]=q[0];pos[npos][1]=q[1];npos++;}}
        pos[npos][0]=11;pos[npos][1]=11;npos++;
        for(int pi=0;pi<npos;pi++)
                {int cx=pos[pi][0],cy=pos[pi][1];
                for(int m=0;m<6561;m++)
                        {int rest=m,ok=1,sq[8][2],nsq=0;
                        for(int k=1;k<=4;k++)for(int s=-1;s<=1;s+=2)
                                {int v=rest%3,x=cx+s*k*dx,y=cy+s*k*dy;
                                rest/=3;
                                if(!on_board(x,y)){if(v)ok=0;continue;}
                                b[x][y]=v;sq[nsq][0]=x;sq[nsq][1]=y;nsq++;}
                        if(ok)
                                {int space=check_space(b,cx,cy,X_SIGN,dx,dy);
                                cases++;
                                for(int side=-1;side<=1;side++)
                                        {int lock,code=compdirection(b,cx,cy,X_SIGN,dx,dy,side,&lock);
                                        int valid=side?(code==0||code==8||code==9):(code>=1&&code<=5);
                                        if(code==8)seen8++;
                                        if(code==9)seen9++;
                                        if(!valid)bad++;
                                        if(space==5&&lock==0&&(code==4||code==9))bad++;
                                        if(space>=5&&code>=2&&direction_score(space,lock,code)==0)bad++;
                                        if(code==5&&direction_score(space,lock,code)!=PM)bad++;
                                        if(!side)
                                                {int n;
                                                b[cx][cy]=X_SIGN;
                                                n=neighbours(b,cx,cy,X_SIGN,dx,dy);
                                                b[cx][cy]=0;
                                                if(code!=(n>=IN_A_ROW?5:n))bad++;}}}
                        for(int i=0;i<nsq;i++)b[sq[i][0]][sq[i][1]]=0;}}}
CHECK(cases>100000);
CHECK_EQ(bad,0);
CHECK(seen8>0);
CHECK(seen9>0);}

/* random positions evaluated everywhere: the sanitizers catch any out-of-bounds read */
static void test_random_boards()
{static tomb b;
srand(1);
for(int n=0;n<200;n++)
        {for(int x=0;x<MAXX;x++)for(int y=0;y<MAXY;y++)b[x][y]=rand()%3;
        minimax(b,wtomb1,X_SIGN);
        minimax(b,wtomb2,O_SIGN);
        whowins(b);}
CHECK(true);}

static void test_winner()
{static tomb local;
clear(local);clear(ptomb);
for(int i=0;i<5;i++)local[3+i][4]=X_SIGN;
CHECK_EQ(whowins(local),X_SIGN);
CHECK_EQ(whowins(ptomb),0);
clear(local);
for(int i=0;i<5;i++)ptomb[10][2+i]=O_SIGN;
CHECK_EQ(whowins(local),0);           /* the global board must not leak in */
CHECK_EQ(whowins(ptomb),O_SIGN);
clear(ptomb);
clear(local);for(int i=0;i<5;i++)local[23][18+i]=O_SIGN;CHECK_EQ(whowins(local),O_SIGN);
clear(local);for(int i=0;i<5;i++)local[i][i]=X_SIGN;CHECK_EQ(whowins(local),X_SIGN);
clear(local);for(int i=0;i<5;i++)local[19+i][4-i]=X_SIGN;CHECK_EQ(whowins(local),X_SIGN);
clear(local);for(int i=0;i<6;i++)local[5+i][20]=O_SIGN;CHECK_EQ(whowins(local),O_SIGN);
clear(local);for(int i=0;i<4;i++)local[5+i][20]=O_SIGN;CHECK_EQ(whowins(local),0);
fill_fixture(local);
CHECK_EQ(whowins(local),0);
CHECK_EQ(game_result(local),DRAW);
local[2][0]=local[3][0]=X_SIGN;        /* row 0 becomes XXXXX: a win beats the draw */
CHECK_EQ(game_result(local),X_SIGN);
clear(local);
CHECK_EQ(game_result(local),0);}

static void test_legal_moves()
{static tomb b;
static const int holes[5][2]={{0,0},{5,17},{11,11},{17,5},{23,22}};
int x=7,y=7,chosen[5]={0},illegal=0;
tombelem most=0;
fill_fixture(b);
CHECK_EQ(pointto_m(&x,&y,wtomb1,wtomb2,b),0);      /* full board: no legal move */
CHECK_EQ(x,7);CHECK_EQ(y,7);
for(auto &h:holes)b[h[0]][h[1]]=0;
minimax(b,wtomb1,X_SIGN);
minimax(b,wtomb2,O_SIGN);
for(auto &h:holes)
        {if(wtomb1[h[0]][h[1]]>most)most=wtomb1[h[0]][h[1]];
        if(wtomb2[h[0]][h[1]]>most)most=wtomb2[h[0]][h[1]];}
CHECK_EQ(most,0);                                  /* every legal move scores zero */
for(unsigned seed=1;seed<=200;seed++)
        {int found=-1;
        srand(seed);
        CHECK_EQ(pointto_m(&x,&y,wtomb1,wtomb2,b),1);
        for(int k=0;k<5;k++)if(x-1==holes[k][0]&&y-1==holes[k][1])found=k;
        if(found<0)illegal++;else chosen[found]++;}
CHECK_EQ(illegal,0);
for(int k=0;k<5;k++)CHECK(chosen[k]>0);
/* placing never overwrites */
clear(b);
b[3][3]=O_SIGN;
x=4;y=4;
CHECK_EQ(putx(&x,&y,b),0);
CHECK_EQ(puto(&x,&y,b),0);
CHECK_EQ(b[3][3],O_SIGN);
x=0;y=5;CHECK_EQ(putx(&x,&y,b),0);
x=25;y=5;CHECK_EQ(puto(&x,&y,b),0);
x=5;y=24;CHECK_EQ(puto(&x,&y,b),0);
x=5;y=5;CHECK_EQ(puto(&x,&y,b),1);CHECK_EQ(b[4][4],O_SIGN);
/* the opening square is only used on an empty board */
clear(b);
b[12][11]=X_SIGN;
CHECK_EQ(pointto_m(&x,&y,wtomb1,wtomb2,b),1);
CHECK(!(x==13&&y==12));
CHECK_EQ(b[x-1][y-1],0);}

/* one X with 8 equally good neighbours: the old rule (first best, later ties replace it
   with probability 1/5) gives (4/5)^7 = 0.21 to the first in scan order and 0.2 to the last */
static void test_tie_breaking()
{static tomb b;
const int runs=4000;
int first=0,last=0,other=0,x,y;
clear(b);
b[12][10]=X_SIGN;
for(int s=1;s<=runs;s++)
        {srand(s);
        pointto_m(&x,&y,wtomb1,wtomb2,b);
        x--;y--;
        if(x==11&&y==9)first++;
        else if(x==13&&y==11)last++;
        else if(x<11||x>13||y<9||y>11||(x==12&&y==10))other++;}
CHECK_EQ(other,0);
CHECK(first>runs*0.17&&first<runs*0.25);
CHECK(last>runs*0.16&&last<runs*0.24);}

static void test_seed()
{int a,b;
setenv("AMOEBA_SEED","12345",1);
seed_rng();
a=rand();
srand(12345);
b=rand();
CHECK_EQ(a,b);
unsetenv("AMOEBA_SEED");}

static void set_game(int g,int r,int xw,int ow,int sp)
{game_n=g;round_n=r;x_won=xw;o_won=ow;starting_player=sp;}

static void test_save_round_trip()
{savestate st,back;
char err[256];
std::string path=tmp("rt"),text;
clear(ptomb);
ptomb[0][0]=X_SIGN;ptomb[23][0]=O_SIGN;ptomb[0][22]=O_SIGN;ptomb[23][22]=X_SIGN;ptomb[11][11]=X_SIGN;
set_game(3,7,2,1,O_SIGN);
current_state(&st,"abc");
CHECK(write_save(path.c_str(),&st,err,sizeof err));
CHECK_EQ(parse_save(path.c_str(),&back,err,sizeof err),LOAD_OK);
CHECK(!strcmp(back.name,"abc"));
CHECK_EQ(back.game_n,3);CHECK_EQ(back.round_n,7);CHECK_EQ(back.x_won,2);CHECK_EQ(back.o_won,1);
CHECK_EQ(back.starting_player,O_SIGN);
CHECK(!memcmp(back.board,ptomb,sizeof ptomb));
current_state(&st,"");                       /* empty name: written as an empty line */
CHECK(write_save(path.c_str(),&st,err,sizeof err));
read_file(path,&text);
CHECK(text.compare(0,3,"\n3\n")==0);
CHECK_EQ(parse_save(path.c_str(),&back,err,sizeof err),LOAD_OK);
CHECK_EQ(back.name[0],0);
CHECK_EQ(back.game_n,3);
CHECK_EQ(write_save((std::string(tmpdir)+"/no/such/dir/save1").c_str(),&st,err,sizeof err),0);
CHECK(strstr(err,"no/such/dir/save1")!=NULL);   /* the message names the file, not the save name */
clear(ptomb);}

static void test_legacy_saves(const char *project)
{static const char *names[]={"save1","save2","save3","save4","save5"};
static const int counts[]={20,12,2,26,24};
savestate st;
char err[256];
std::string text,again;
for(int i=0;i<5;i++)
        {std::string src=std::string(project)+"/"+names[i],copy=tmp(names[i]),rt=tmp("legacy_rt");
        if(!read_file(src,&text)){fprintf(stderr,"  (skipping %s: not found)\n",src.c_str());continue;}
        write_file(copy,text);
        int r=parse_save(copy.c_str(),&st,err,sizeof err);
        if(r!=LOAD_OK)fprintf(stderr,"  %s\n",err);
        CHECK_EQ(r,LOAD_OK);
        CHECK_EQ(stones(st.board),counts[i]);
        CHECK_EQ(st.starting_player,X_SIGN);
        CHECK(write_save(rt.c_str(),&st,err,sizeof err));
        read_file(rt,&again);
        CHECK(again==text);}                   /* rewriting a legacy save gives the same bytes */
if(read_file(std::string(project)+"/save1~",&text))
        {write_file(tmp("empty"),text);
        CHECK_EQ(parse_save(tmp("empty").c_str(),&st,err,sizeof err),LOAD_BAD);
        CHECK(strstr(err,"file is empty")!=NULL);}}

static void test_bad_saves()
{static const char *head="abc\n1\n2\n0\n0\n1\n";
struct {std::string text;const char *msg;} bad[]={
        {"",                              "line 1: file is empty"},
        {"abcdefghi\n1\n2\n0\n0\n1\n",   "line 1: bad name"},
        {"ab1\n1\n2\n0\n0\n1\n",         "line 1: bad name"},
        {"a b\n1\n2\n0\n0\n1\n",         "line 1: bad name"},
        {std::string(300,'z')+"\n1\n2\n0\n0\n1\n","line 1: bad name"},
        {"abc\n1\n2\n",                  "line 4: missing X win count"},
        {"abc\n1\n2\n0\n0\n",            "line 6: missing starting player"},
        {"abc\nx\n2\n0\n0\n1\n",         "line 2: bad game number"},
        {"abc\n1 \n2\n0\n0\n1\n",        "line 2: bad game number"},
        {"abc\n99999999999999999999\n2\n0\n0\n1\n","line 2: bad game number"},
        {"abc\n1\n-1\n0\n0\n1\n",        "line 3: bad round number"},
        {"abc\n1\n2\n0\n0\n0\n",         "line 6: bad starting player"},
        {"abc\n1\n2\n0\n0\n3\n",         "line 6: bad starting player"},
        {std::string(head)+"[1,2=1\n",   "line 7: stone must look like"},
        {std::string(head)+"[1,2]1\n",   "line 7: stone must look like"},
        {std::string(head)+"1,2]=1\n",   "line 7: stone must look like"},
        {std::string(head)+"[a,b]=1\n",  "line 7: stone must look like"},
        {std::string(head)+"[1,2]=1x\n", "line 7: stone must look like"},
        {std::string(head)+"[1,2]=\n",   "line 7: stone must look like"},
        {std::string(head)+"[1,1]=1\n[2,2]=2\n[9999999999,2]=1\n","line 9: stone must look like"},
        {std::string(head)+"[24,0]=1\n", "line 7: stone is off the board"},
        {std::string(head)+"[0,23]=2\n", "line 7: stone is off the board"},
        {std::string(head)+"[-1,5]=1\n", "line 7: stone is off the board"},
        {std::string(head)+"[1,1]=0\n",  "line 7: player must be 1 or 2"},
        {std::string(head)+"[1,1]=3\n",  "line 7: player must be 1 or 2"},
        {std::string(head)+"[1,1]=1\n[1,1]=2\n","line 8: square is given twice"},
        {std::string(head)+std::string(200,'[')+"\n","line 7: line too long"}};
savestate st;
char err[256];
std::string path=tmp("bad");
for(auto &t:bad)
        {write_file(path,t.text);
        int r=parse_save(path.c_str(),&st,err,sizeof err);
        CHECK_EQ(r,LOAD_BAD);
        if(!strstr(err,t.msg))fprintf(stderr,"FAIL message \"%s\" lacks \"%s\"\n",err,t.msg);
        CHECK(strstr(err,t.msg)!=NULL);}
CHECK_EQ(parse_save(tmp("missing").c_str(),&st,err,sizeof err),LOAD_NOFILE);
write_file(path,std::string(head)+"\n[1,1]=1\r\n\n[2,3]=2\n\n");   /* blank lines and CRLF are fine */
CHECK_EQ(parse_save(path.c_str(),&st,err,sizeof err),LOAD_OK);
CHECK_EQ(stones(st.board),2);
CHECK_EQ(st.board[2][3],O_SIGN);
write_file(path,"abc\n1\n2\n0\n0\n1\n[1,1]=1");                 /* no final newline */
CHECK_EQ(parse_save(path.c_str(),&st,err,sizeof err),LOAD_OK);
CHECK_EQ(st.board[1][1],X_SIGN);}

/* a failed parse leaves the running game alone */
static void test_failed_load_keeps_game()
{static tomb before;
savestate st;
char err[256];
std::string path=tmp("bad");
clear(ptomb);
ptomb[4][4]=X_SIGN;ptomb[5][5]=O_SIGN;
set_game(2,3,1,1,O_SIGN);
memcpy(before,ptomb,sizeof before);
write_file(path,"zzz\n9\n9\n9\n9\n1\n[1,1]=1\n[1,1]=1\n");
CHECK_EQ(parse_save(path.c_str(),&st,err,sizeof err),LOAD_BAD);
CHECK(!memcmp(before,ptomb,sizeof before));
CHECK_EQ(game_n,2);CHECK_EQ(round_n,3);CHECK_EQ(x_won,1);CHECK_EQ(o_won,1);CHECK_EQ(starting_player,O_SIGN);
clear(ptomb);}

static std::string capture_redraw()
{std::string path=tmp("screen"),text;
fflush(stdout);
if(!freopen(path.c_str(),"w",stdout))exit(2);
redraw_screen();
fflush(stdout);
if(!freopen("/dev/null","w",stdout))exit(2);
read_file(path,&text);
return text;}

/* stones on the last column and the last row are drawn after a save and load round trip */
static void test_redraw_corners()
{savestate st;
char err[256];
std::string path=tmp("corners"),screen;
clear(ptomb);
ptomb[0][0]=X_SIGN;ptomb[23][0]=O_SIGN;ptomb[0][22]=X_SIGN;ptomb[23][22]=O_SIGN;
set_game(1,2,0,0,X_SIGN);
current_state(&st,"corner");
CHECK(write_save(path.c_str(),&st,err,sizeof err));
clear(ptomb);
CHECK_EQ(parse_save(path.c_str(),&st,err,sizeof err),LOAD_OK);
apply_save(&st);
screen=capture_redraw();
CHECK(screen.find("\033[1;2f\033[1;34;40mX")!=std::string::npos);
CHECK(screen.find("\033[1;48f\033[1;31;40mO")!=std::string::npos);
CHECK(screen.find("\033[23;2f\033[1;34;40mX")!=std::string::npos);
CHECK(screen.find("\033[23;48f\033[1;31;40mO")!=std::string::npos);
CHECK(screen.find("\033[24;")==std::string::npos);   /* nothing below the last board row */
clear(ptomb);}

static void remove_tmpdir()
{DIR *d=opendir(tmpdir);
struct dirent *e;
if(!d)return;
while((e=readdir(d))!=NULL)
        if(strcmp(e->d_name,".")&&strcmp(e->d_name,".."))unlink(tmp(e->d_name).c_str());
closedir(d);
rmdir(tmpdir);}

int main(int argc,char **argv)
{const char *base=getenv("TMPDIR");
snprintf(tmpdir,sizeof tmpdir,"%s/amoeba-test-XXXXXX",base&&*base?base:"/tmp");
if(!mkdtemp(tmpdir)){perror("mkdtemp");return 2;}
if(!freopen("/dev/null","w",stdout))return 2;   /* the game draws on stdout */
test_empty_board();
test_patterns_all_axes();
test_edges_and_corners();
test_combination_ranking();
test_combination_moves();
test_lookahead_moves();
test_forced_wins_hold();
test_blocks_win();
test_exhaustive_lines();
test_random_boards();
test_winner();
test_legal_moves();
test_tie_breaking();
test_seed();
test_save_round_trip();
test_legacy_saves(argc>1?argv[1]:".");
test_bad_saves();
test_failed_load_keeps_game();
test_redraw_corners();
remove_tmpdir();
fprintf(stderr,"%d checks, %d failures\n",checks,failures);
return failures?1:0;}

/*      AMOEBA program - Major Marcell
  utolso modositas: 1998.12.08-1999.01.10*/
#include <stdio.h>
#include "conio.h"
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#define MAXPLAYERS 2
#define MAXX 24
#define MAXY 23
#define MAX_SAVES 5
#define MAX_SAVENAME_LEN 8
#define X_SIGN 1
#define O_SIGN 2
#define IN_A_ROW 5

unsigned int P0=1,P1=5,P2=21,P3=85,P4=341,P5=1365,P6=5461,
             P7=21845,PM=UINT_MAX;

typedef unsigned int tombelem;
typedef tombelem tomb[MAXX][MAXY];
tomb wtomb1,wtomb2,ptomb;

int game_n=0,round_n,x_won=0,o_won=0;
int starting_player=X_SIGN;

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
for(i=0;i<24;++i){outc='A'+i;cprintf(" %c",outc);}
textcolor(LIGHTGRAY);}

void putx(int *x,int *y,tomb pt)
{gotoxy(*x*2,*y);
textcolor(LIGHTBLUE);
cprintf("X");
gotoxy(*x,*y);
textcolor(LIGHTGRAY);
pt[*x-1][*y-1]=X_SIGN;}

void puto(int *x,int *y,tomb pt)
{gotoxy(*x*2,*y);
textcolor(LIGHTRED);
cprintf("O");
gotoxy(*x,*y);
textcolor(LIGHTGRAY);
pt[*x-1][*y-1]=O_SIGN;}

void draw_moves(int MaxSor,int MaxOszl)
{
  int i,j;
  
  for(i=0;i<MaxSor;++i)
    for(j=0;j<MaxOszl;++j)
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
{int n=0,i,etype;
if (ntype==X_SIGN)etype=O_SIGN;
else if (ntype==O_SIGN)etype=X_SIGN;
for (i=0;i<IN_A_ROW&&w[posx+dx*i][posy+dy*i]!=etype&&(posx+dx*i)<MAXX&&(posx+dx*i)>=0&&(posy+dy*i)<MAXY&&(posy+dy*i)>=0;++i)++n;
for (i=1;i<IN_A_ROW&&w[posx-dx*i][posy-dy*i]!=etype&&(posx-dx*i)>=0&&(posy-dy*i)<MAXY&&(posy-dy*i)>=0&&(posx-dx*i)<MAXX;++i)++n;
if(n>=6)return 6;
else if(n==5)return n;
else return 0;}

int neighbours(tomb w,int posx,int posy,int ntype,int dx,int dy)
{int n=0,i;
for(i=0;i<IN_A_ROW&&w[posx+dx*i][posy+dy*i]==ntype&&(posx+dx*i)>=0&&(posx+dx*i)<MAXX&&(posy+dy*i)<MAXY&&(posy+dy*i)>=0;++i)++n;
for(i=1;i<IN_A_ROW&&w[posx-dx*i][posy-dy*i]==ntype&&(posx-dx*i)>=0&&(posy-dy*i)<MAXY&&(posy-dy*i)>=0&&(posx-dx*i)<MAXX;++i)++n;
return n;}

int compdirection(tomb w,int posx,int posy,int ntype,int dx,int dy)
{int n,i,flag;
w[posx][posy]=ntype;
n=neighbours(w,posx,posy,ntype,dx,dy);
w[posx][posy]=0;
if(n>=5)return 5;
flag=0;
for(i=1;i<IN_A_ROW&&w[posx+dx*i][posy+dy*i]==ntype&&(posx+dx*i)<MAXX&&(posx+dx*i)>=0&&(posy+dy*i)<MAXY&&(posy+dy*i)>=0;++i)
        {if(!w[posx+dx*i][posy+dy*i])flag++;
        if(flag==1&&w[posx+dx*i][posy+dy*i]==ntype)n=n+6;}
if(n<6) {flag=0;
        for(i=1;i<IN_A_ROW&&w[posx-dx*i][posy-dy*i]==ntype&&(posx-dx*i)>=0&&(posy-dy*i)<MAXY&&(posy-dy*i)>=0&&(posx-dx*i)<MAXX;++i)
                {if(!w[posx-dx*i][posy-dy*i])flag++;
                if(flag==1&&w[posx-dx*i][posy-dy*i]==ntype)n=n+6;}
        }
return n;}

int locks(tomb w,int posx,int posy,int ntype,int dx,int dy)
{int i,locked=0,etype;
if(ntype==X_SIGN)etype=O_SIGN;
else if(ntype==O_SIGN)etype=X_SIGN;
w[posx][posy]=ntype;
i=0;
while(i<IN_A_ROW&&w[posx+dx*i][posy+dy*i]==ntype&&(posx+dx*i)<MAXX&&(posx+dx*i)>=0&&(posy+dy*i)<MAXY&&(posy+dy*i)>=0)++i;
if(w[posx+dx*i][posy+dy*i]==etype)locked++;
i=0;
while(i<IN_A_ROW&&w[posx-dx*i][posy-dy*i]==ntype&&(posx-dx*i)>=0&&(posy-dy*i)<MAXY&&(posy-dy*i)>=0&&(posx-dx*i)<MAXX)++i;
if(w[posx-dx*i][posy-dy*i]==etype)locked++;
w[posx][posy]=0;
return locked;}

tombelem compare(tomb playt,int x_coord,int y_coord,int nt)
{tombelem counter;
int lock,i;
static int directions[4][2]={{0,1},{1,0},{1,1},{1,-1}};
counter=0;
for(i=0;i<4;i++)
{
lock=locks(playt,x_coord,y_coord,nt,directions[i][0],directions[i][1]);
switch (check_space(playt,x_coord,y_coord,nt,directions[i][0],directions[i][1]))
{
case 6:
if(!lock)
        {switch(compdirection(playt,x_coord,y_coord,nt,directions[i][0],directions[i][1]))
                {case 9:counter+=P7-1;break;
                case 8:counter+=P6-1;break;
                case 5:counter=PM;goto compend;
                case 4:counter+=P7;break;
                case 3:counter+=P6;break;
                case 2:counter+=P3;break;}}
else {switch(compdirection(playt,x_coord,y_coord,nt,directions[i][0],directions[i][1]))
        {case 9:counter+=P5-1;break;
        case 8:counter+=P4-1;break;
        case 5:counter=PM;goto compend;
        case 4:counter+=P5;break;
        case 3:counter+=P4;break;
        case 2:counter+=P1;break;}}
break;
case 5:
if(!lock)
        {switch(compdirection(playt,x_coord,y_coord,nt,directions[i][0],directions[i][1]))
                {case 8:counter+=P4-1;break;
                case 5:counter=PM;goto compend;
                case 3:counter+=P4;break;
                case 2:counter+=P2;break;}}
else {switch(compdirection(playt,x_coord,y_coord,nt,directions[i][0],directions[i][1]))
        {case 9:counter+=P5-1;break;
        case 8:counter+=P4-1;break;
        case 5:counter=PM;goto compend;
        case 4:counter+=P5;break;
        case 3:counter+=P4;break;
        case 2:counter+=P1;break;}}
break;
case 0:
break;}}
compend: return counter;}

void minimax(tomb pt,tomb wt,int ntype)
{int i,j;
for(i=0;i<MAXX;i++)for(j=0;j<MAXY;j++)wt[i][j]=0;
for(i=0;i<MAXX;i++)for(j=0;j<MAXY;j++)if(pt[i][j]==0)wt[i][j]=compare(pt,i,j,ntype);
}

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
        if(pt[i][j]==who)
                {
                if(neighbours(ptomb,i,j,who,1,1)>=IN_A_ROW){win=who;goto vege;}
                if(neighbours(ptomb,i,j,who,0,1)>=IN_A_ROW){win=who;goto vege;}
                if(neighbours(ptomb,i,j,who,1,0)>=IN_A_ROW){win=who;goto vege;}
                if(neighbours(ptomb,i,j,who,1,-1)>=IN_A_ROW){win=who;goto vege;}
                }
        }
vege:;
return (win);}

void show_slots()
{
  FILE *ifp;
  char buf[64],savename[64];
  int i;
  
  for(i=0;i<MAX_SAVES;i++)
  {
    strncpy(buf,"save",4);
    buf[4]='1'+i;
    buf[5]=0;
    ifp = fopen(buf, "r");
    gotoxy(20,7+i);
    cprintf("%d.:",i+1);
    if (ifp == NULL)continue;
    fscanf(ifp, "%s", savename);
    fclose(ifp);
    cprintf("%s",savename);
  }
}

void save_game()
{
  char ch;
  char buf[64];
  char filename[64];
  int pos,i,j;
  FILE *ofp;
  
  show_slots();
  textcolor(LIGHTRED);
  gotoxy(0,24);
  cprintf("Save to slot (1-%d):",MAX_SAVES);
  do
  {
    ch=getch();
  }while(ch<'1'||ch>('0'+MAX_SAVES));
  sprintf(filename,"save%c",ch);
  gotoxy(0,24);
  cprintf("Saved state name(allowed characters are [a-z]):");
  pos=0;
  do
  {
    ch=getch();
    if(ch>='a'&&ch<='z')
    {
      buf[pos++]=ch;
      cprintf("%c",ch);
    }else if(ch=='\n')
    {
      break;
    }
  }while(pos<MAX_SAVENAME_LEN);
  buf[pos]=0;
  ofp = fopen(filename, "w");
  if (ofp == NULL)
  {
    fprintf(stderr, "Can't open output file %s!\n",buf);
  }
  else
  {
    fprintf(ofp, "%s\n", buf);
    fprintf(ofp, "%d\n", game_n);
    fprintf(ofp, "%d\n", round_n);
    fprintf(ofp, "%d\n", x_won);
    fprintf(ofp, "%d\n", o_won);
    fprintf(ofp, "%d\n", starting_player);
    for(i=0;i<MAXX;i++)for(j=0;j<MAXY;j++)
    {
      if(ptomb[i][j]==O_SIGN)
      {
        fprintf(ofp, "[%d,%d]=%d\n", i, j, O_SIGN);
      }
      else if(ptomb[i][j]==X_SIGN)
      {
        fprintf(ofp, "[%d,%d]=%d\n", i, j, X_SIGN);
      }
    }
    fclose(ofp);
  }
  tableout(MAXY,MAXX);
  draw_moves(MAXY,MAXX);
  info(game_n, round_n, x_won, o_won);
}

void load_game()
{
  char ch;
  char filename[64];
  char buf[64];
  FILE *ifp;
  int i,j,posx,posy;
  unsigned int player;

  show_slots();
  textcolor(LIGHTRED);
  gotoxy(0,24);
  cprintf("Game to load (1-%d):",MAX_SAVES);
  do
  {
    ch=getch();
  }while((ch<'1')||(ch>('0'+MAX_SAVES)));
  sprintf(filename,"save%c",ch);
  gotoxy(0,24);
  ifp = fopen(filename, "r");
  if(ifp == NULL)
  {
    textcolor(LIGHTRED);
    gotoxy(0,24);
    cprintf("Cannot open %s (press enter)!",filename);
    do
    {
      ch=getch();
    }while(ch!='\n');
  } else
  {
    fscanf(ifp, "%s\n", buf);
    cprintf("buf\n");
    fscanf(ifp, "%d\n", &game_n);
    cprintf("game_n\n");
    fscanf(ifp, "%d\n", &round_n);
    cprintf("round_n\n");
    fscanf(ifp, "%d\n", &x_won);
    cprintf("x_won\n");
    fscanf(ifp, "%d\n", &o_won);
    cprintf("o_won\n");
    fscanf(ifp, "%d\n", &starting_player);
    cprintf("starting_player\n");
    for(i=0;i<MAXX;i++)for(j=0;j<MAXY;j++)
    {
      ptomb[i][j]=0;wtomb1[i][j]=0;wtomb2[i][j]=0;
    }
    while(fscanf(ifp, "[%d,%d]=%d\n", &posx, &posy, &player) != EOF)
    {
      if(posx>=0&&posx<MAXX&&posy>=0&&posy<MAXY)
      {
        ptomb[posx][posy]=player;
        cprintf("[%d,%d]=%d\n", posx, posy, player);
      }
    }
    fclose(ifp);
  }
  tableout(MAXY,MAXX);
  draw_moves(MAXY,MAXX);
  info(game_n, round_n, x_won, o_won);
}

void pointto(int *x,int *y,tomb pt,int *exit)
{char c;
gotoxy(*x*2,*y);
do
{
c=getch();
if(c==27)
  {
    c=getch();
    if(c==91)
    {
      c=getch();
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
if(c=='l')load_game();
gotoxy(*x*2,*y);
}
while ((c!=' '||pt[*x-1][*y-1]!=0)&&c!='q');
if(c=='q')*exit=1;}

void pointto_m(int *x,int *y,tomb wt1,tomb wt2,tomb pt,int rc,int sp)
{int i,j,xx=0,xy=0,ox=0,oy=0;
tombelem max_x=0,max_o=0;
if(!rc&&sp==O_SIGN){*x=13;*y=12;}
else
{
minimax(pt,wt1,X_SIGN);
minimax(pt,wt2,O_SIGN);
for(i=0;i<MAXX;i++)for(j=0;j<MAXY;j++)
        {if(wt1[i][j]>max_x){max_x=wt1[i][j];xx=i;xy=j;}
        else if(wt1[i][j]==max_x&&(rand()%5)==4){max_x=wt1[i][j];xx=i;xy=j;}}
for(i=0;i<MAXX;i++)for(j=0;j<MAXY;j++)
        {if(wt2[i][j]>max_o){max_o=wt2[i][j];ox=i;oy=j;}
        else if(wt2[i][j]==max_o&&(rand()%5)==4){max_o=wt2[i][j];ox=i;oy=j;}}
if(max_x>max_o)
        {*x=xx+1;*y=xy+1;}
        else {*x=ox+1;*y=oy+1;}
}}

int main (void)
{
  int    i,j,x,y,winner,exit=0,quit=0;
  char    ch;
  //randomize();
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
  do
  {switch(starting_player){
  case X_SIGN:
          pointto(&x,&y,ptomb,&quit);
          if(!quit)putx(&x,&y,ptomb);
          if(!(winner=whowins(ptomb))&&!quit)
                  {pointto_m(&x,&y,wtomb1,wtomb2,ptomb,round_n,starting_player);
                  puto(&x,&y,ptomb);}
          info(game_n,round_n,x_won,o_won);
          round_n++;
          break;
  case O_SIGN:
          pointto_m(&x,&y,wtomb1,wtomb2,ptomb,round_n,starting_player);
          puto(&x,&y,ptomb);
          if(!(winner=whowins(ptomb))&&!quit)
                  {pointto(&x,&y,ptomb,&quit);
                  if(!quit)putx(&x,&y,ptomb);}
          info(game_n,round_n,x_won,o_won);
          round_n++;
          break;}
  }
  while (!(winner=whowins(ptomb))&&!quit);
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
  default:cprintf("-");
  }
  textcolor(LIGHTGRAY);
  textbackground(DARKGRAY);
  gotoxy(60,20);
  printf("NEW GAME?(y/n)");
  do
  {
  ch=getch();
  }while(ch!='n'&&ch!='y');
  if (ch=='n')exit=1;
  if (quit){quit=0;o_won++;}
  }while (!exit);
  printf("\e[0;100m");
  clrscr();
  //textcolor(BLACK);
  //system("reset");
}

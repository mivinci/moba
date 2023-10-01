#ifndef MOBA_H
#define MOBA_H

#include "list.h"
#include <lualib.h>

#define moba_lock(M) ((void)0)
#define moba_unlock(M) ((void)0)

#define RANK_BRONZE     0
#define RANK_SILVER     1
#define RANK_GOLD       2
#define RANK_PLATNUM    3
#define RANK_DIAMOND    4
#define RANK_CHALLENGER 5

#define ROLE_TOP 1
#define ROLE_BOT 2
#define ROLE_MID 4
#define ROLE_JG  8
#define ROLE_SUP 16

struct player {
  int id;  // starts from 1
  int rank;
  int score;
  int role;  // role bitmap
};

struct group {
  int len;
  int cap;
  struct player *players;
  struct list_head node;
};

struct match {
  struct group *red;
  struct group *blue;
};

struct moba {
  lua_State *L;
  struct list_head *queues;  // matchmaking queues
  int len;
  int n;
  int k;
};

// opens new moba state.
struct moba *moba_open(int, int);
// closes a moba state.
void moba_close(struct moba *);
// pushes a group into a matchmaking queue.
void moba_push(struct moba *, struct group *);
// pops out a pair of groups.  returns 0 on success or -1 if no pair is matched.
struct group *moba_pop(struct moba *);
// tells if or not the matchmaking queue is empty.
int moba_empty(struct moba *);
// matches a pair of groups as red team and blue team.
int moba_match(struct moba *, struct match *);
// loads a lua file.
void moba_load(struct moba *, const char *);

// calls function `match` in the loaded lua script.
int mobaL_match(lua_State *, struct group *, struct group *);
// calls function `elo` in the loaded lua script.
double mobaL_elo(lua_State *, struct group *, struct group *);
// calls function `score` in the loaded lua script.
double mobaL_score(lua_State *, struct group *);

#endif  // MOBA_H

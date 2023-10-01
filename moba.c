#include "moba.h"
#include <assert.h>
#include <lauxlib.h>
#include <math.h>
#include <stdlib.h>

int mobaL_match(lua_State *L, struct group *g1, struct group *g2) {
  int __ok = 0;
  lua_getglobal(L, "match");
  lua_pushlightuserdata(L, g1);
  lua_pushlightuserdata(L, g2);
  if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
    perror(lua_tostring(L, -1));
    return -1;
  }
  __ok = lua_toboolean(L, -1);
  lua_pop(L, 1);  // pops out `match`
  return __ok;
}

double mobaL_elo(lua_State *L, struct group *g1, struct group *g2) {
  double __elo;
  lua_getglobal(L, "elo");
  lua_pushlightuserdata(L, g1);
  lua_pushlightuserdata(L, g2);
  if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
    perror(lua_tostring(L, -1));
    return -1;
  }
  __elo = lua_tonumber(L, -1);
  lua_pop(L, 1);  // pops out `elo`
  return __elo;
}

double mobaL_score(lua_State *L, struct group *g) {
  double __score;
  lua_getglobal(L, "score");
  lua_pushlightuserdata(L, g);
  if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
    perror(lua_tostring(L, -1));
    return -1;
  }
  __score = lua_tonumber(L, -1);
  lua_pop(L, 1);  // pops out `score`
  return __score;
}

struct moba *moba_open(int n, int k) {
  struct moba *M;
  if ((M = malloc(sizeof(*M))) == NULL)
    goto err;
  if ((M->queues = malloc(n * sizeof(*M->queues))) == NULL)
    goto err;
  if ((M->L = luaL_newstate()) == NULL)
    goto err;
  luaL_openlibs(M->L);
  M->n = n;
  M->k = k;
  M->len = 0;
  for (int i = 0; i < n; i++) {
    list_head_init(M->queues+i);
  }
  return M;
err:
  if (M->L)
    lua_close(M->L);
  if (M->queues)
    free(M->queues);
  if (M)
    free(M);
  return NULL;
}

void moba_close(struct moba *M) {
  if (M->L)
    lua_close(M->L);
  if (M->queues)
    free(M->queues);
  if (M)
    free(M);
}

void moba_load(struct moba *M, const char *name) {
  moba_lock(M);
  luaL_dofile(M->L, name);
  moba_unlock(M);
}

static void merge(struct group *g1, struct group *g2) {
  int i = 0, j = 0;
  while (i < g1->cap && j < g2->cap) {
    if (g1->players[i].id > 0)
      i++;
    else if (g2->players[j].id == 0)
      j++;
    else
      g1->players[i++] = g2->players[j++];
  }
  g1->len += g2->len;
  assert(g1->len == g1->cap);
}

void moba_push(struct moba *M, struct group *g) {
  struct group *g1 = g, *g2;
  struct list_head *head, *q;
  assert(g1->len <= M->n);
  if (g1->len == M->n) {
    list_add(&g1->node, M->queues + M->n - 1);
    M->len++;
    return;
  }
  if (M->len >= M->k) {
    list_add(&g1->node, M->queues + g1->len - 1);
    return;
  }
  int ok;
  head = M->queues + (M->n - g1->len - 1);
  list_foreach(q, head) {
    g2 = container_of(q, struct group, node);
    ok = mobaL_match(M->L, g1, g2);
    assert(ok >= 0);
    if (ok > 0) {
      list_del(&g2->node);
      merge(g2, g1);
      list_add(&g2->node, M->queues + M->n - 1);
      M->len++;
      return;
    }
  }
  list_add(&g1->node, M->queues + g1->len - 1);
}

struct group *moba_pop(struct moba *M) {
  struct group *g;
  struct list_head *head;
  head = M->queues + M->n - 1;
  g = container_of(head->prev, struct group, node);
  list_del(head->prev);
  M->len--;
  return g;
}

int moba_empty(struct moba *M) {
  struct list_head *head;
  head = M->queues + M->n - 1;
  return head->next == head;
}

int moba_match(struct moba *M, struct match *m) {
  struct group *g1, *g2;
  struct list_head *head, *q1, *q2;
  head = M->queues + M->n - 1;
  int ok;
  for (q1 = head->prev; q1 != head; q1 = q1->prev) {
    for (q2 = q1->prev; q2 != head; q2 = q2->prev) {
      g1 = container_of(q1, struct group, node);
      g2 = container_of(q2, struct group, node);
      ok = mobaL_match(M->L, g1, g2);
      assert(ok >= 0);
      if (ok > 0) {
        m->blue = g1;
        m->red = g2;
        list_del(q1);
        list_del(q2);
        M->len -= 2;
        return 0;
      }
    }
  }
  return -1;
}

static int score(lua_State *L) {
  struct group *g = lua_touserdata(L, 1);
  double __score = 0.0, w;
  assert(lua_gettop(L) == g->cap + 1);
  for (int i = 0; i < g->cap; i++) {
    w = lua_tonumber(L, i + 2);
    __score += w * g->players[i].score;
  }
  lua_pushnumber(L, __score);
  return 1;
}

static int role(lua_State *L) {
  struct group *g = lua_touserdata(L, 1);
  unsigned int __role = 0x0;
  assert(lua_gettop(L) == 1);
  for (int i = 0; i < g->cap; i++) {
    __role |= g->players[i].role;
  }
  lua_pushinteger(L, __role);
  return 1;
}

static int rank(lua_State *L) {
  struct group *g = lua_touserdata(L, 1);
  int __min = INT_MAX, __max = INT_MIN, r;
  for (int i = 0; i < g->cap; i++) {
    if (g->players[i].id <= 0)
      continue;
    r = g->players[i].rank;
    __min = (__min > r) ? r : __min;
    __max = (__max < r) ? r : __max;
  }
  lua_pushinteger(L, __min);
  lua_pushinteger(L, __max);
  return 2;
}

static int len(lua_State *L) {
  struct group *g = lua_touserdata(L, 1);
  lua_pushinteger(L, g->len);
  return 1;
}

static int cap(lua_State *L) {
  struct group *g = lua_touserdata(L, 1);
  lua_pushinteger(L, g->cap);
  return 1;
}

int luaopen_moba(lua_State *L) {
  luaL_Reg l[] = {
      {"len", len},      // len
      {"cap", cap},      // cap
      {"score", score},  // score
      {"role", role},    // role
      {"rank", rank},    // rank
      {NULL, NULL},      // sentinel
  };
  luaL_newlib(L, l);
  return 1;
}

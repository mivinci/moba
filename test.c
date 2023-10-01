#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "moba.h"

const char *role_names[] = {"TOP", "BOT", "MID", "JG ", "SUP"};

int main(void) {
  int n, m, k, id = 0;
  int rank, score, role;
  struct group *G, *g;
  struct player *p;

  scanf("%d %d", &n, &m);
  G = malloc(n * sizeof(*G));
  assert(G);
  for (int i = 0; i < n; i++) {
    scanf("%d", &k);
    g = G + i;
    g->cap = m;
    g->len = k;
    g->players = malloc(m * sizeof(struct player));
    memset(g->players, 0, m * sizeof(struct player));
    assert(g->players);
    while (k--) {
      scanf("%d %d %d", &rank, &score, &role);
      p = g->players + role;
      p->rank = rank;
      p->score = score;
      p->role = (1 << role);
      p->id = ++id;
    }
  }

  struct moba *M;
  M = moba_open(5, 32);
  assert(M);
  moba_load(M, "test.lua");
  for (int i = 0; i < n; i++) {
    moba_push(M, G + i);
  }

  struct player *p1, *p2;
  struct match mat;
  double elo;
  int c = 0;
  while (moba_match(M, &mat) == 0) {
    printf("\tBLUE\tRED\n");
    for (int i = 0; i < m; i++) {
      p1 = mat.blue->players + i;
      p2 = mat.red->players + i;
      printf("%s\t%d\t%d\n", role_names[i], p1->id, p2->id);
    }
    printf("SCORE\t%.1lf\t%.1lf\n", mobaL_score(M->L, mat.blue),
           mobaL_score(M->L, mat.red));
    elo = mobaL_elo(M->L, mat.blue, mat.red);
    printf("ELO\t%.2lf\t%.2lf\n", elo, 1 - elo);
    c++;
  }
  printf("%d pair(s) of groups matched\n", c);
  moba_close(M);
  return 0;
}

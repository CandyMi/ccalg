/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Test suite for cctreap (intrusive treap)
**  Build:  gcc -std=c99 -Wall -Wextra -o test_cctreap test_cctreap.c && ./test_cctreap
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../include/cctreap.h"

/* ── test entry ───────────────────────────────────────────────────────── */
struct entry {
  int             key;
  cctreap_node_t  node;
};

static int64_t key_cmp(const cctreap_node_t *a, const cctreap_node_t *b) {
  return (int64_t)(CCTREAP_CONTAINER(a, struct entry, node)->key -
                   CCTREAP_CONTAINER(b, struct entry, node)->key);
}

/* ── simple LCG kept for shuffle / deterministic _rng in stress tests ── */
static uint64_t _rng_state = 0xCAFEBABEDEADBEEFULL;
static uint64_t _rng(void) {
  _rng_state = _rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
  return _rng_state;
}

/* ── test harness ─────────────────────────────────────────────────────── */
static int passed, failed;
#define TEST(name) static void test_##name(void)
#define ASSERT(cond) do { \
  if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failed++; } \
  else passed++; \
} while(0)
#define RUN(name) do { printf("  %s\n", #name); test_##name(); } while(0)

/* ── helpers ──────────────────────────────────────────────────────────── */
static int count_forward(cctreap_t *m) {
  int n = 0;
  for (cctreap_node_t *c = cctreap_begin(m); c != cctreap_end(m); c = cctreap_next(c)) n++;
  return n;
}

static int count_reverse(cctreap_t *m) {
  int n = 0;
  for (cctreap_node_t *c = cctreap_rbegin(m); c; c = cctreap_prev(c)) n++;
  return n;
}

static int verify_bst(cctreap_node_t *x, cctreap_cmp_t cmp) {
  if (!x) return 1;
  if (x->child[CCTREAP_LEFT]) {
    if (cmp(x->child[CCTREAP_LEFT], x) >= 0) return 0;
    if (!verify_bst(x->child[CCTREAP_LEFT], cmp)) return 0;
  }
  if (x->child[CCTREAP_RIGHT]) {
    if (cmp(x, x->child[CCTREAP_RIGHT]) >= 0) return 0;
    if (!verify_bst(x->child[CCTREAP_RIGHT], cmp)) return 0;
  }
  return 1;
}

static int verify_heap(cctreap_node_t *x) {
  if (!x) return 1;
  if (x->child[CCTREAP_LEFT]) {
    if (_TP_PRIO_CMP(x, x->child[CCTREAP_LEFT]) <= 0) return 0;
    if (!verify_heap(x->child[CCTREAP_LEFT])) return 0;
  }
  if (x->child[CCTREAP_RIGHT]) {
    if (_TP_PRIO_CMP(x, x->child[CCTREAP_RIGHT]) <= 0) return 0;
    if (!verify_heap(x->child[CCTREAP_RIGHT])) return 0;
  }
  return 1;
}

static int verify_sizes(cctreap_node_t *x) {
  if (!x) return 1;
  size_t expect = 1;
  if (x->child[CCTREAP_LEFT])  { if (!verify_sizes(x->child[CCTREAP_LEFT])) return 0; expect += x->child[CCTREAP_LEFT]->size; }
  if (x->child[CCTREAP_RIGHT]) { if (!verify_sizes(x->child[CCTREAP_RIGHT])) return 0; expect += x->child[CCTREAP_RIGHT]->size; }
  return x->size == expect;
}

static int real_height(cctreap_node_t *x) {
  if (!x) return 0;
  int l = real_height(x->child[CCTREAP_LEFT]);
  int r = real_height(x->child[CCTREAP_RIGHT]);
  return 1 + (l > r ? l : r);
}

/* ── tests ────────────────────────────────────────────────────────────── */

TEST(init_empty) {
  cctreap_t m;
  cctreap_init(&m, key_cmp);
  ASSERT(cctreap_size(&m) == 0);
  ASSERT(cctreap_begin(&m) == NULL);
  ASSERT(cctreap_end(&m) == NULL);
  ASSERT(cctreap_first(&m) == NULL);
  ASSERT(cctreap_last(&m) == NULL);
  ASSERT(cctreap_rbegin(&m) == NULL);
  ASSERT(cctreap_height(&m) == 0);
  ASSERT(cctreap_kth(&m, 0) == NULL);
}

TEST(insert_and_size) {
  cctreap_t m; cctreap_init(&m, key_cmp);
  struct entry e[5];
  for (int i = 0; i < 5; i++) { e[i].key = i * 10; cctreap_insert(&m, &e[i].node, NULL); }
  ASSERT(cctreap_size(&m) == 5);
  ASSERT(count_forward(&m) == 5);
  ASSERT(count_reverse(&m) == 5);
  ASSERT(verify_bst(m.root, key_cmp));
  ASSERT(verify_heap(m.root));
  ASSERT(verify_sizes(m.root));
}

TEST(insert_duplicate) {
  cctreap_t m; cctreap_init(&m, key_cmp);
  struct entry e1 = {.key = 42}; cctreap_node_t *out = NULL;
  ASSERT(cctreap_insert(&m, &e1.node, &out) == 0);
  ASSERT(out == &e1.node);

  struct entry e2 = {.key = 42};
  ASSERT(cctreap_insert(&m, &e2.node, &out) == -1);
  ASSERT(out == &e1.node); /* out → existing */
  ASSERT(cctreap_size(&m) == 1);
}

TEST(find) {
  cctreap_t m; cctreap_init(&m, key_cmp);
  struct entry e[3] = {{.key=10},{.key=20},{.key=30}};
  for (int i = 0; i < 3; i++) { cctreap_insert(&m, &e[i].node, NULL); }

  struct entry probe;
  probe.key = 10; ASSERT(cctreap_find(&m, &probe.node) == &e[0].node);
  probe.key = 20; ASSERT(cctreap_find(&m, &probe.node) == &e[1].node);
  probe.key = 30; ASSERT(cctreap_find(&m, &probe.node) == &e[2].node);
  probe.key = 99; ASSERT(cctreap_find(&m, &probe.node) == NULL);
}

TEST(erase) {
  cctreap_t m; cctreap_init(&m, key_cmp);
  struct entry e[5];
  for (int i = 0; i < 5; i++) { e[i].key = i; cctreap_insert(&m, &e[i].node, NULL); }
  ASSERT(cctreap_size(&m) == 5);

  cctreap_erase(&m, &e[2].node); /* middle */
  ASSERT(cctreap_size(&m) == 4);
  ASSERT(count_forward(&m) == 4);
  ASSERT(verify_bst(m.root, key_cmp));
  ASSERT(verify_heap(m.root));
  ASSERT(verify_sizes(m.root));

  cctreap_erase(&m, &e[0].node); /* first */
  ASSERT(cctreap_size(&m) == 3);
  ASSERT(verify_bst(m.root, key_cmp));
  ASSERT(verify_heap(m.root));
  ASSERT(verify_sizes(m.root));

  cctreap_erase(&m, &e[4].node); /* last */
  ASSERT(cctreap_size(&m) == 2);
  ASSERT(verify_bst(m.root, key_cmp));
  ASSERT(verify_heap(m.root));
  ASSERT(verify_sizes(m.root));
}

TEST(erase_update_first_last) {
  cctreap_t m; cctreap_init(&m, key_cmp);
  struct entry e[3] = {{.key=30},{.key=10},{.key=20}};
  for (int i = 0; i < 3; i++) { cctreap_insert(&m, &e[i].node, NULL); }
  ASSERT(CCTREAP_CONTAINER(cctreap_first(&m), struct entry, node)->key == 10);
  ASSERT(CCTREAP_CONTAINER(cctreap_last(&m), struct entry, node)->key == 30);

  /* erase min */
  cctreap_erase(&m, &e[1].node); /* key=10 */
  ASSERT(cctreap_size(&m) == 2);
  ASSERT(CCTREAP_CONTAINER(cctreap_first(&m), struct entry, node)->key == 20);
  ASSERT(CCTREAP_CONTAINER(cctreap_last(&m), struct entry, node)->key == 30);

  /* erase max */
  cctreap_erase(&m, &e[0].node); /* key=30 */
  ASSERT(cctreap_size(&m) == 1);
  ASSERT(cctreap_first(&m) == cctreap_last(&m));
  ASSERT(CCTREAP_CONTAINER(cctreap_first(&m), struct entry, node)->key == 20);

  /* erase last node */
  cctreap_erase(&m, &e[2].node); /* key=20 */
  ASSERT(cctreap_size(&m) == 0);
  ASSERT(cctreap_first(&m) == NULL);
  ASSERT(cctreap_last(&m) == NULL);
  ASSERT(cctreap_rbegin(&m) == NULL);
}

TEST(clear) {
  cctreap_t m; cctreap_init(&m, key_cmp);
  struct entry e[3] = {{.key=1},{.key=2},{.key=3}};
  for (int i = 0; i < 3; i++) { cctreap_insert(&m, &e[i].node, NULL); }
  cctreap_clear(&m);
  ASSERT(cctreap_size(&m) == 0);
  ASSERT(cctreap_begin(&m) == NULL);
  ASSERT(cctreap_first(&m) == NULL);
  ASSERT(cctreap_last(&m) == NULL);
  ASSERT(cctreap_height(&m) == 0);
}

TEST(iterate_forward) {
  cctreap_t m; cctreap_init(&m, key_cmp);
  struct entry e[5];
  for (int i = 0; i < 5; i++) { e[i].key = 4 - i; cctreap_insert(&m, &e[i].node, NULL); }
  int prev = -1;
  for (cctreap_node_t *n = cctreap_begin(&m); n != cctreap_end(&m); n = cctreap_next(n)) {
    int k = CCTREAP_CONTAINER(n, struct entry, node)->key;
    ASSERT(k > prev);
    prev = k;
  }
  ASSERT(count_forward(&m) == 5);
}

TEST(iterate_reverse) {
  cctreap_t m; cctreap_init(&m, key_cmp);
  struct entry e[5];
  for (int i = 0; i < 5; i++) { e[i].key = i; cctreap_insert(&m, &e[i].node, NULL); }
  int prev = 999;
  for (cctreap_node_t *n = cctreap_rbegin(&m); n; n = cctreap_prev(n)) {
    int k = CCTREAP_CONTAINER(n, struct entry, node)->key;
    ASSERT(k < prev);
    prev = k;
  }
  ASSERT(count_reverse(&m) == 5);
}

TEST(next_prev_roundtrip) {
  cctreap_t m; cctreap_init(&m, key_cmp);
  struct entry e1 = {.key = 10}, e2 = {.key = 20}, e3 = {.key = 30};
  cctreap_insert(&m, &e1.node, NULL);
  cctreap_insert(&m, &e2.node, NULL);
  cctreap_insert(&m, &e3.node, NULL);

  ASSERT(cctreap_next(&e3.node) == NULL); /* max → no next */
  ASSERT(cctreap_prev(&e1.node) == NULL); /* min → no prev */
  ASSERT(cctreap_prev(cctreap_next(&e2.node)) == &e2.node);
  ASSERT(cctreap_next(cctreap_prev(&e2.node)) == &e2.node);
}

TEST(kth_basics) {
  cctreap_t m; cctreap_init(&m, key_cmp);
  struct entry e[5];
  for (int i = 0; i < 5; i++) { e[i].key = i * 10; cctreap_insert(&m, &e[i].node, NULL); }

  ASSERT(CCTREAP_CONTAINER(cctreap_kth(&m, 0), struct entry, node)->key == 0);   /* min */
  ASSERT(CCTREAP_CONTAINER(cctreap_kth(&m, 1), struct entry, node)->key == 10);
  ASSERT(CCTREAP_CONTAINER(cctreap_kth(&m, 2), struct entry, node)->key == 20);
  ASSERT(CCTREAP_CONTAINER(cctreap_kth(&m, 3), struct entry, node)->key == 30);
  ASSERT(CCTREAP_CONTAINER(cctreap_kth(&m, 4), struct entry, node)->key == 40);  /* max */

  /* bounds */
  ASSERT(cctreap_kth(&m, 5) == NULL);   /* k == size */
  ASSERT(cctreap_kth(&m, 999) == NULL); /* k > size */

  /* empty */
  cctreap_t empty; cctreap_init(&empty, key_cmp);
  ASSERT(cctreap_kth(&empty, 0) == NULL);
}

TEST(rank_basics) {
  cctreap_t m; cctreap_init(&m, key_cmp);
  struct entry e[5];
  for (int i = 0; i < 5; i++) { e[i].key = i * 10; cctreap_insert(&m, &e[i].node, NULL); }

  ASSERT(cctreap_rank(&m, &e[0].node) == 0);  /* key=0  */
  ASSERT(cctreap_rank(&m, &e[1].node) == 1);  /* key=10 */
  ASSERT(cctreap_rank(&m, &e[2].node) == 2);  /* key=20 */
  ASSERT(cctreap_rank(&m, &e[3].node) == 3);  /* key=30 */
  ASSERT(cctreap_rank(&m, &e[4].node) == 4);  /* key=40 */

  /* not found */
  struct entry ghost;
  ghost.key = 15; ASSERT(cctreap_rank(&m, &ghost.node) == -1);
  ghost.key = -5; ASSERT(cctreap_rank(&m, &ghost.node) == -1);
  ghost.key = 99; ASSERT(cctreap_rank(&m, &ghost.node) == -1);

  /* empty */
  cctreap_t empty; cctreap_init(&empty, key_cmp);
  ASSERT(cctreap_rank(&empty, &e[0].node) == -1);
}

TEST(kth_rank_roundtrip) {
  cctreap_t m; cctreap_init(&m, key_cmp);
  enum { N = 100 };
  struct entry *e = calloc(N, sizeof(*e));
  ASSERT(e != NULL);
  for (int i = 0; i < N; i++) {
    e[i].key = (i * 7919) % (N * 10);  /* sparse keys */
    cctreap_insert(&m, &e[i].node, NULL);
  }
  ASSERT(cctreap_size(&m) == N);

  for (size_t k = 0; k < cctreap_size(&m); k++) {
    cctreap_node_t *n = cctreap_kth(&m, k);
    ASSERT(n != NULL);
    ASSERT(cctreap_rank(&m, n) == (int64_t)k);
  }
  free(e);
}

TEST(null_safety) {
  cctreap_t m; cctreap_init(&m, key_cmp);
  cctreap_init(NULL, NULL);
  ASSERT(cctreap_insert(NULL, NULL, NULL) == -1);
  ASSERT(cctreap_find(NULL, NULL) == NULL);
  cctreap_erase(NULL, NULL);
  cctreap_clear(NULL);
  ASSERT(cctreap_size(NULL) == 0);
  ASSERT(cctreap_begin(NULL) == NULL);
  ASSERT(cctreap_end(NULL) == NULL);
  ASSERT(cctreap_first(NULL) == NULL);
  ASSERT(cctreap_last(NULL) == NULL);
  ASSERT(cctreap_rbegin(NULL) == NULL);
  ASSERT(cctreap_kth(NULL, 0) == NULL);
  ASSERT(cctreap_rank(NULL, NULL) == -1);
  ASSERT(cctreap_height(NULL) == 0);
  ASSERT(cctreap_next(NULL) == NULL);
  ASSERT(cctreap_prev(NULL) == NULL);
}

TEST(stress_insert_only) {
  cctreap_t m; cctreap_init(&m, key_cmp);
  enum { N = 5000 };
  struct entry *e = calloc(N, sizeof(*e));
  ASSERT(e != NULL);
  for (int i = 0; i < N; i++) {
    e[i].key = (i * 7919) % N;
    cctreap_insert(&m, &e[i].node, NULL);
  }
  ASSERT(cctreap_size(&m) == N);
  ASSERT(count_forward(&m) == N);
  ASSERT(count_reverse(&m) == N);
  ASSERT(verify_bst(m.root, key_cmp));
  ASSERT(verify_heap(m.root));
  ASSERT(verify_sizes(m.root));

  /* height sanity: treap expected height is O(log n), well within 4x of estimate */
  int rh = real_height(m.root);
  int ub = cctreap_height(&m);
  ASSERT(rh <= ub * 4);  /* generous bound */

  /* find each */
  for (int i = 0; i < N; i++) {
    ASSERT(cctreap_find(&m, &e[i].node) == &e[i].node);
  }
  free(e);
}

TEST(stress_erase_all) {
  cctreap_t m; cctreap_init(&m, key_cmp);
  enum { N = 1000 };
  struct entry *e = calloc(N, sizeof(*e));
  ASSERT(e != NULL);

  /* insert ascending keys (worst-case for BST, treap handles via priority) */
  for (int i = 0; i < N; i++) {
    e[i].key = i;
    cctreap_insert(&m, &e[i].node, NULL);
  }
  ASSERT(cctreap_size(&m) == N);
  ASSERT(verify_bst(m.root, key_cmp));
  ASSERT(verify_heap(m.root));
  ASSERT(verify_sizes(m.root));

  /* erase in random order */
  int *order = calloc(N, sizeof(int));
  ASSERT(order != NULL);
  for (int i = 0; i < N; i++) order[i] = i;
  /* Fisher-Yates shuffle with LCG */
  for (int i = N - 1; i > 0; i--) {
    int j = (int)(_rng() % (uint64_t)(i + 1));
    int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
  }

  for (int i = 0; i < N; i++) {
    int idx = order[i];
    cctreap_erase(&m, &e[idx].node);
    if (i % 200 == 199) {
      ASSERT(verify_bst(m.root, key_cmp));
      ASSERT(verify_heap(m.root));
      ASSERT(verify_sizes(m.root));
    }
  }
  ASSERT(cctreap_size(&m) == 0);
  ASSERT(cctreap_first(&m) == NULL);
  ASSERT(cctreap_last(&m) == NULL);
  free(order);
  free(e);
}

TEST(stress_mixed) {
  cctreap_t m; cctreap_init(&m, key_cmp);
  enum { N = 3000, OPS = 5000 };
  struct entry *pool = calloc(N, sizeof(*pool));
  ASSERT(pool != NULL);
  int *in_tree = calloc(N, sizeof(int));
  ASSERT(in_tree != NULL);

  for (int i = 0; i < N; i++) {
    pool[i].key = i;
  }

  size_t sz = 0;
  for (int op = 0; op < OPS; op++) {
    int idx = (int)(_rng() % N);
    if (in_tree[idx]) {
      cctreap_erase(&m, &pool[idx].node);
      in_tree[idx] = 0;
      sz--;
    } else {
      if (cctreap_insert(&m, &pool[idx].node, NULL) == 0) {
        in_tree[idx] = 1;
        sz++;
      }
    }
    ASSERT(cctreap_size(&m) == sz);
    if (op % 500 == 499) {
      ASSERT(verify_bst(m.root, key_cmp));
      ASSERT(verify_heap(m.root));
      ASSERT(verify_sizes(m.root));
      if (sz > 0) {
        ASSERT(cctreap_first(&m) != NULL);
        ASSERT(cctreap_last(&m) != NULL);
        ASSERT(CCTREAP_CONTAINER(cctreap_first(&m), struct entry, node)->key <=
               CCTREAP_CONTAINER(cctreap_last(&m), struct entry, node)->key);
      }
    }
  }

  free(in_tree);
  free(pool);
}

/* ── main ─────────────────────────────────────────────────────────────── */
int main(void) {
  printf("cctreap tests:\n");
  RUN(init_empty);
  RUN(insert_and_size);
  RUN(insert_duplicate);
  RUN(find);
  RUN(erase);
  RUN(erase_update_first_last);
  RUN(clear);
  RUN(iterate_forward);
  RUN(iterate_reverse);
  RUN(next_prev_roundtrip);
  RUN(kth_basics);
  RUN(rank_basics);
  RUN(kth_rank_roundtrip);
  RUN(null_safety);
  printf("stress tests:\n");
  RUN(stress_insert_only);
  RUN(stress_erase_all);
  RUN(stress_mixed);
  printf("\n  %d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}

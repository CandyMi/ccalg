#include <stdio.h>
#include <assert.h>
#include "include/cctreap.h"

typedef struct mytreap_node
{
  int key;
  cctreap_node_t node;
} mytreap_node_t ;


static int64_t treap_cmp(const cctreap_node_t *a, const cctreap_node_t *b)
{
  return
    CCTREAP_CONTAINER(a, mytreap_node_t, node)->key
    -
    CCTREAP_CONTAINER(b, mytreap_node_t, node)->key;
}


int main(int argc, char const *argv[])
{
  (void)argc; (void)argv;

  cctreap_t tp;
  cctreap_init(&tp, treap_cmp);

  mytreap_node_t n1, n2, n3, n4, n5;
  n1.key = 1;
  n2.key = 2;
  n3.key = 3;
  n4.key = 4;
  n5.key = 5;

  /* insert */
  cctreap_insert(&tp, &n3.node, NULL);
  cctreap_insert(&tp, &n4.node, NULL);
  cctreap_insert(&tp, &n5.node, NULL);
  cctreap_insert(&tp, &n1.node, NULL);
  cctreap_insert(&tp, &n2.node, NULL);

  /* find */
  mytreap_node_t tmp;
  for (int i = 1; i <= 5; i++) {
    tmp.key = i; assert(cctreap_find(&tp, &tmp.node));
  }

  /* iterate. */
  for (cctreap_node_t *p = cctreap_begin(&tp); p != NULL; p = cctreap_next(p))
  {
    printf("make iterate -> key = %d, priority = %llu\n",
      CCTREAP_CONTAINER(p, mytreap_node_t, node)->key,
      (unsigned long long)p->priority
    );
  }

  /* make kth */
  for (cctreap_node_t *p = cctreap_kth(&tp, 2); p != NULL; p = cctreap_next(p))
  {
    printf("make kth -> key = %d, priority = %llu\n",
      CCTREAP_CONTAINER(p, mytreap_node_t, node)->key,
      (unsigned long long)p->priority
    );
  }

  /* make rank */
  // cctreap_rank(&tp, )

  cctreap_clear(&tp);
  return 0;
}

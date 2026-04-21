#include "tree.h"
#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int write_tree(Index *index, ObjectID *tree_id_out) {
    Tree tree;
    tree.count = 0;

    for (int i = 0; i < index->count; i++) {
        IndexEntry *e = &index->entries[i];

        if (strchr(e->path, '/')) continue;

        TreeEntry *te = &tree.entries[tree.count++];
        te->mode = e->mode;
        te->hash = e->hash;
        strcpy(te->name, e->path);
    }

    void *data;
    size_t len;
    if (tree_serialize(&tree, &data, &len) != 0) return -1;

    int rc = object_write(OBJ_TREE, data, len, tree_id_out);
    free(data);
    return rc;
}

int tree_from_index(ObjectID *tree_id_out) {
    Index index;
    if (index_load(&index) != 0) return -1;
    return write_tree(&index, tree_id_out);
}
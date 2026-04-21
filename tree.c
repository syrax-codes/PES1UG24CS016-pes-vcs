#include "tree.h"
#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MODE_DIR 0040000

static int write_tree(Index *index, ObjectID *tree_id_out) {
    Tree tree;
    tree.count = 0;

    for (int i = 0; i < index->count; i++) {
        IndexEntry *e = &index->entries[i];

        if (!strchr(e->path, '/')) {
            TreeEntry *te = &tree.entries[tree.count++];
            te->mode = e->mode;
            te->hash = e->hash;
            strcpy(te->name, e->path);
        }
    }

    for (int i = 0; i < index->count; i++) {
        IndexEntry *e = &index->entries[i];

        char *slash = strchr(e->path, '/');
        if (!slash) continue;

        int len = slash - e->path;

        char dirname[256];
        strncpy(dirname, e->path, len);
        dirname[len] = '\0';

        int exists = 0;
        for (int j = 0; j < tree.count; j++) {
            if (strcmp(tree.entries[j].name, dirname) == 0) {
                exists = 1;
                break;
            }
        }

        if (!exists) {
            TreeEntry *te = &tree.entries[tree.count++];
            te->mode = MODE_DIR;
            memset(&te->hash, 0, sizeof(ObjectID));
            strcpy(te->name, dirname);
        }
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
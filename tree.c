// tree.c — Tree object serialization and construction

#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "index.h"
#include "pes.h"

// ─── Mode Constants ─────────────────────────────────────────────────────────

#define MODE_FILE      0100644
#define MODE_EXEC      0100755
#define MODE_DIR       0040000

// ─── PROVIDED ───────────────────────────────────────────────────────────────
int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);

uint32_t get_file_mode(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return 0;

    if (S_ISDIR(st.st_mode))  return MODE_DIR;
    if (st.st_mode & S_IXUSR) return MODE_EXEC;
    return MODE_FILE;
}

int tree_parse(const void *data, size_t len, Tree *tree_out) {
    tree_out->count = 0;
    const uint8_t *ptr = (const uint8_t *)data;
    const uint8_t *end = ptr + len;

    while (ptr < end && tree_out->count < MAX_TREE_ENTRIES) {
        TreeEntry *entry = &tree_out->entries[tree_out->count];

        const uint8_t *space = memchr(ptr, ' ', end - ptr);
        if (!space) return -1;

        char mode_str[16] = {0};
        size_t mode_len = space - ptr;
        if (mode_len >= sizeof(mode_str)) return -1;
        memcpy(mode_str, ptr, mode_len);
        entry->mode = strtol(mode_str, NULL, 8);

        ptr = space + 1;

        const uint8_t *null_byte = memchr(ptr, '\0', end - ptr);
        if (!null_byte) return -1;

        size_t name_len = null_byte - ptr;
        if (name_len >= sizeof(entry->name)) return -1;
        memcpy(entry->name, ptr, name_len);
        entry->name[name_len] = '\0';

        ptr = null_byte + 1;

        if (ptr + HASH_SIZE > end) return -1; 
        memcpy(entry->hash.hash, ptr, HASH_SIZE);
        ptr += HASH_SIZE;

        tree_out->count++;
    }
    return 0;
}

static int compare_tree_entries(const void *a, const void *b) {
    return strcmp(((const TreeEntry *)a)->name, ((const TreeEntry *)b)->name);
}

int tree_serialize(const Tree *tree, void **data_out, size_t *len_out) {
    size_t max_size = tree->count * 296; 
    uint8_t *buffer = malloc(max_size);
    if (!buffer) return -1;

    Tree sorted_tree = *tree;
    qsort(sorted_tree.entries, sorted_tree.count, sizeof(TreeEntry), compare_tree_entries);

    size_t offset = 0;
    for (int i = 0; i < sorted_tree.count; i++) {
        const TreeEntry *entry = &sorted_tree.entries[i];
        int written = sprintf((char *)buffer + offset, "%o %s", entry->mode, entry->name);
        offset += written + 1;
        memcpy(buffer + offset, entry->hash.hash, HASH_SIZE);
        offset += HASH_SIZE;
    }

    *data_out = buffer;
    *len_out = offset;
    return 0;
}

// ─── IMPLEMENTATION ─────────────────────────────────────────────────────────

// helper for recursion
static int build_tree(IndexEntry *entries, int count, ObjectID *id_out) {
    Tree tree;
    tree.count = 0;

    // files
    for (int i = 0; i < count; i++) {
        if (!strchr(entries[i].path, '/')) {
            TreeEntry *te = &tree.entries[tree.count++];
            te->mode = entries[i].mode;
            memcpy(te->hash.hash, entries[i].hash.hash, HASH_SIZE);
            strcpy(te->name, entries[i].path);
        }
    }

    // directories
    for (int i = 0; i < count; i++) {
        char *slash = strchr(entries[i].path, '/');
        if (!slash) continue;

        int len = slash - entries[i].path;

        char dirname[256];
        strncpy(dirname, entries[i].path, len);
        dirname[len] = '\0';

        int exists = 0;
        for (int j = 0; j < tree.count; j++) {
            if (strcmp(tree.entries[j].name, dirname) == 0) {
                exists = 1;
                break;
            }
        }
        if (exists) continue;

        IndexEntry sub[MAX_INDEX_ENTRIES];
        int sub_count = 0;

        for (int k = 0; k < count; k++) {
            if (strncmp(entries[k].path, dirname, len) == 0 &&
                entries[k].path[len] == '/') {

                sub[sub_count] = entries[k];

                memmove(sub[sub_count].path,
                        entries[k].path + len + 1,
                        strlen(entries[k].path) - len);

                sub_count++;
            }
        }

        ObjectID sub_id;
        if (build_tree(sub, sub_count, &sub_id) != 0) return -1;

        TreeEntry *te = &tree.entries[tree.count++];
        te->mode = MODE_DIR;
        memcpy(te->hash.hash, sub_id.hash, HASH_SIZE);
        strcpy(te->name, dirname);
    }

    void *data;
    size_t len;
    if (tree_serialize(&tree, &data, &len) != 0) return -1;

    int rc = object_write(OBJ_TREE, data, len, id_out);
    free(data);
    return rc;
}

// main function
int tree_from_index(ObjectID *id_out) {
    Index index;
    if (index_load(&index) != 0) return -1;

    return build_tree(index.entries, index.count, id_out);
}
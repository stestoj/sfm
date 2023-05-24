#include <string.h>
#include "sort.h"
#include "file.h"

int dir_cmp(const void *a, const void *b){
    const struct file *arg1 = (const struct file*)a;
    const struct file *arg2 = (const struct file*)b;

    if(arg1->is_dir && !arg2->is_dir) return -1;
    if(!arg1->is_dir && arg2->is_dir) return 1;

    return strcmp(arg1->filename, arg2->filename);
}

int file_cmp(const void *a, const void *b){
    const struct file *arg1 = (const struct file*)a;
    const struct file *arg2 = (const struct file*)b;

    return strcmp(arg1->filename, arg2->filename);
}

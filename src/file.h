#ifndef FILE_H
#define FILE_H

#include <stdbool.h>

struct file {
    char filename[256];
    bool is_dir;
};

#endif

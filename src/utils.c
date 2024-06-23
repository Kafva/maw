#include "maw/log.h"
#include "maw/utils.h"

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/errno.h>


size_t readfile(const char *filepath, char *out, size_t outsize) {
    FILE *fp;
    size_t read_bytes = 0;

    fp = fopen(filepath, "r");
    if (fp == NULL) {
        MAW_PERROR(filepath);
        return 1;
    }

    read_bytes = fread(out, 1, outsize, fp);
    if (read_bytes <= 0) {
        MAW_LOGF(MAW_ERROR, "%s: empty", filepath);
        goto end;
    }
    else if (read_bytes == outsize) {
        MAW_LOGF(MAW_ERROR, "%s: too large", filepath);
        goto end;
    }
end:
    fclose(fp);
    return read_bytes;
}

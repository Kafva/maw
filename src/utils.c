#include "maw/log.h"
#include "maw/maw.h"
#include "maw/utils.h"

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/errno.h>
#include <sys/stat.h>

size_t readfile(const char *filepath, char *out, size_t outsize) {
    FILE *fp = NULL;
    size_t read_bytes = 0;

    fp = fopen(filepath, "r");
    if (fp == NULL) {
        MAW_PERROR(filepath);
        goto end;
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
    if (fp != NULL)
        fclose(fp);
    return read_bytes;
}

int movefile(const char *src, const char *dst) {
    int r = MAW_ERR_INTERNAL;
    size_t read_bytes = 0;
    size_t write_bytes = 0;
    char buffer[BUFSIZ];
    FILE *fp_src = NULL;
    FILE *fp_dst = NULL;

    fp_src = fopen(src, "r");
    if (fp_src == NULL) {
        MAW_PERROR(src);
        goto end;
    }

    fp_dst = fopen(dst, "w");
    if (fp_dst == NULL) {
        MAW_PERROR(dst);
        goto end;
    }

    while ((read_bytes = fread(buffer, 1, BUFSIZ, fp_src)) > 0) {
        write_bytes = fwrite(buffer, 1, read_bytes, fp_dst);
        if (write_bytes != read_bytes) {
            MAW_LOGF(MAW_ERROR, "fwrite short write: %zu byte(s)", write_bytes);
            goto end;
        }
    }

    fclose(fp_src);
    fp_src = NULL;

    r = unlink(src);
    if (r != 0) {
        MAW_PERROR(src);
        goto end;
    }

    r = 0;
end:
    if (fp_src != NULL)
        fclose(fp_src);
    if (fp_dst != NULL)
        fclose(fp_dst);
    return r;
}

bool isdir(const char *path) {
    struct stat s;

    if (stat(path, &s) != 0) {
        return false;
    }

    return S_ISDIR(s.st_mode);
}

bool isfile(const char *path) {
    struct stat s;

    if (stat(path, &s) != 0) {
        return false;
    }

    return S_ISREG(s.st_mode);
}

bool on_same_device(const char *path1, const char *path2) {
    struct stat stat1, stat2;

    if (stat(path1, &stat1) < 0) {
        MAW_PERROR("stat");
        return false;
    }

    if (stat(path2, &stat2) < 0) {
        MAW_PERROR("stat");
        return false;
    }

    return stat1.st_dev == stat2.st_dev;
}

// http://www.isthe.com/chongo/tech/comp/fnv/
uint32_t hash(const char *str) {
    uint32_t digest = 2166136261;

    for (size_t i = 0; i < strlen(str); i++) {
        digest ^= (unsigned char)str[i];
        digest *= 16777619;
    }

    return digest;
}

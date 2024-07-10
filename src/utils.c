#include "maw/utils.h"
#include "maw/log.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
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
        MAW_WRITE(fileno(fp_dst), buffer, read_bytes);
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

// Music/red/red1.m4a -> red1
int basename_no_ext(const char *filepath, char *out, size_t outsize) {
    int r = MAW_ERR_INTERNAL;
    char *slash;
    char *dot;

    slash = strrchr(filepath, '/');
    if (slash == NULL) {
        MAW_STRLCPY_SIZE(out, filepath, outsize);
    }
    else if (strlen(slash) > 1) {
        MAW_STRLCPY_SIZE(out, slash + 1, outsize);
    }
    else {
        MAW_LOGF(MAW_ERROR, "Invalid filename: %s", filepath);
        goto end;
    }

    dot = strrchr(out, '.');
    if (dot != NULL) {
        *dot = '\0';
    }

    r = 0;
end:
    return r;
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

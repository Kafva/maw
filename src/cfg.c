#include <glob.h>

#include "maw/cfg.h"
#include "maw/log.h"
#include "maw/utils.h"

static int maw_cfg_yaml_init(const char *filepath, yaml_parser_t **parser,
                             FILE **fp);
static void maw_cfg_yaml_deinit(yaml_parser_t *parser, FILE *fp);
static const char *maw_cfg_key_tostr(enum YamlKey key);
static void maw_cfg_key_push(YamlContext *ctx, const char *key,
                             enum YamlKey mkey);
static bool maw_cfg_key_pop(YamlContext *ctx);
static enum YamlKey maw_cfg_parse_key_to_enum(YamlContext *ctx,
                                              const char *key);
static int maw_cfg_set_metadata_field(MawConfig *cfg, YamlContext *ctx,
                                      yaml_token_t *token, Metadata *metadata,
                                      const char *value);
static int maw_cfg_add_to_playlist(Playlist *playlist, const char *value);
static int maw_cfg_parse_key(MawConfig *cfg, YamlContext *ctx,
                             yaml_token_t *token);
static int maw_cfg_parse_value(MawConfig *cfg, YamlContext *ctx,
                               yaml_token_t *token);
static void maw_cfg_ctx_dump(YamlContext *ctx);

#define MAW_YAML_UNXEXPECTED(level, ctx, token, type, scalar) \
    do { \
        MAW_LOGF(level, "Unexpected %s '%s' at %s:%zu:%zu", type, scalar, \
                 ctx->filepath, token->start_mark.line + 1, \
                 token->start_mark.column); \
    } while (0)

#define MAW_YAML_WARN(ctx, token, type, scalar) \
    MAW_YAML_UNXEXPECTED(MAW_WARN, ctx, token, type, scalar);

#define MAW_YAML_ERROR(ctx, token, type, scalar) \
    MAW_YAML_UNXEXPECTED(MAW_ERROR, ctx, token, type, scalar);

////////////////////////////////////////////////////////////////////////////////

static int maw_cfg_yaml_init(const char *filepath, yaml_parser_t **parser,
                             FILE **fp) {
    int r = MAW_ERR_INTERNAL;

    *fp = fopen(filepath, "r");
    if (*fp == NULL) {
        MAW_PERRORF("fopen", filepath);
        goto end;
    }

    r = yaml_parser_initialize(*parser);
    if (r != 1) {
        r = MAW_ERR_YAML;
        MAW_LOGF(MAW_ERROR, "%s: failed to initialize parser", filepath);
        goto end;
    }

    yaml_parser_set_input_file(*parser, *fp);

    r = 0;
end:
    return r;
}

static void maw_cfg_yaml_deinit(yaml_parser_t *parser, FILE *fp) {
    yaml_parser_delete(parser);
    free(parser);
    (void)fclose(fp);
}

static const char *maw_cfg_key_tostr(enum YamlKey key) {
    switch (key) {
        CASE_RET(KEY_NONE);
        CASE_RET(KEY_INVALID);
        CASE_RET(KEY_ARBITRARY);
        CASE_RET(KEY_ART);
        CASE_RET(KEY_MUSIC);
        CASE_RET(KEY_PLAYLISTS);
        CASE_RET(KEY_METADATA);
        CASE_RET(KEY_ALBUM);
        CASE_RET(KEY_ARTIST);
        CASE_RET(KEY_COVER);
        CASE_RET(KEY_CLEAN);
    default:
        return NULL;
    }
}

const char *maw_cfg_cover_policy_tostr(CoverPolicy key) {
    switch (key) {
        CASE_RET(COVER_POLICY_NONE);
        CASE_RET(COVER_POLICY_PATH);
        CASE_RET(COVER_POLICY_CLEAR);
        CASE_RET(COVER_POLICY_CROP);
    }
}

static int maw_cfg_glob(const char *instr, char **outstr) {
    int r = MAW_ERR_INTERNAL;
    glob_t glob_result;

    r = glob(instr, GLOB_TILDE, 0, &glob_result);
    if (r != 0) {
        MAW_PERRORF("glob", instr);
        goto end;
    }
    if (glob_result.gl_pathc != 1) {
        MAW_LOGF(MAW_ERROR, "glob(%s): no exact match", instr);
        goto end;
    }

    *outstr = strdup(glob_result.gl_pathv[0]);

    globfree(&glob_result);

    r = 0;
end:
    return r;
}

static void maw_cfg_key_push(YamlContext *ctx, const char *key,
                             enum YamlKey mkey) {
    const char *ctxstr = maw_cfg_key_tostr(ctx->keypath[ctx->key_count - 1]);
    if (ctxstr == NULL) {
        MAW_LOGF(MAW_DEBUG, "Pushing key: %s", key);
    }
    else {
        MAW_LOGF(MAW_DEBUG, "Pushing key onto %s: %s", ctxstr, key);
    }
    ctx->keypath[ctx->key_count++] = mkey;
}

static bool maw_cfg_key_pop(YamlContext *ctx) {
    if (ctx->key_count == 0) {
        return false;
    }
    MAW_LOGF(MAW_DEBUG, "Popping key: %s",
             maw_cfg_key_tostr(ctx->keypath[ctx->key_count - 1]));
    ctx->keypath[ctx->key_count - 1] = KEY_NONE;
    ctx->key_count--;
    return true;
}

static enum YamlKey maw_cfg_parse_key_to_enum(YamlContext *ctx,
                                              const char *key) {
    switch (ctx->key_count) {
    case 0:
        if (STR_EQ(MAW_CFG_KEY_METADATA, key)) {
            return KEY_METADATA;
        }
        else if (STR_EQ(MAW_CFG_KEY_PLAYLISTS, key)) {
            return KEY_PLAYLISTS;
        }
        else if (STR_EQ(MAW_CFG_KEY_MUSIC, key)) {
            return KEY_MUSIC;
        }
        else if (STR_EQ(MAW_CFG_KEY_ART, key)) {
            return KEY_ART;
        }
        break;
    case 1:
        // Name of 'playlists' or 'metadata' entry
        return KEY_ARBITRARY;
    case 2:
        if (STR_EQ(MAW_CFG_KEY_ALBUM, key)) {
            return KEY_ALBUM;
        }
        else if (STR_EQ(MAW_CFG_KEY_ARTIST, key)) {
            return KEY_ARTIST;
        }
        else if (STR_EQ(MAW_CFG_KEY_COVER, key)) {
            return KEY_COVER;
        }
        else if (STR_EQ(MAW_CFG_KEY_CLEAN, key)) {
            return KEY_CLEAN;
        }
        break;
    default:
        break;
    }
    return KEY_INVALID;
}

static int maw_cfg_set_metadata_field(MawConfig *cfg, YamlContext *ctx,
                                      yaml_token_t *token, Metadata *metadata,
                                      const char *value) {
    int r = MAW_ERR_INTERNAL;
    char *cover_path = NULL;

    switch (ctx->keypath[2]) {
    case KEY_ALBUM:
        metadata->album = strdup(value);
        break;
    case KEY_ARTIST:
        metadata->artist = strdup(value);
        break;
    case KEY_COVER:
        if (STR_CASE_EQ("none", value)) {
            metadata->cover_policy = COVER_POLICY_NONE;
        }
        else if (STR_CASE_EQ("crop", value)) {
            metadata->cover_policy = COVER_POLICY_CROP;
        }
        else if (STR_CASE_EQ("clear", value)) {
            metadata->cover_policy = COVER_POLICY_CLEAR;
        }
        else {
            metadata->cover_policy = COVER_POLICY_PATH;
            if (cfg->art_dir == NULL) {
                MAW_LOGF(
                    MAW_ERROR,
                    "%s: an art_dir must be configured before setting a cover",
                    value);
                goto end;
            }

            cover_path = calloc(MAW_PATH_MAX, sizeof(char));
            if (cover_path == NULL) {
                MAW_PERROR("calloc");
                goto end;
            }

            MAW_STRLCPY_SIZE(cover_path, cfg->art_dir, (size_t)MAW_PATH_MAX);
            MAW_STRLCAT_SIZE(cover_path, "/", (size_t)MAW_PATH_MAX);
            MAW_STRLCAT_SIZE(cover_path, value, (size_t)MAW_PATH_MAX);

            metadata->cover_path = cover_path;
        }
        break;
    case KEY_CLEAN:
        metadata->clean = strncasecmp("true", value, sizeof("true") - 1) == 0;
        break;
    default:
        MAW_YAML_ERROR(ctx, token, "value", value);
        break;
    }

    MAW_LOGF(MAW_DEBUG, "Setting: %s", value);
    r = 0;
end:
    return r;
}

static int maw_cfg_add_to_playlist(Playlist *playlist, const char *value) {
    int r = MAW_ERR_INTERNAL;
    PlaylistPath *ppath = NULL;

    ppath = calloc(1, sizeof(PlaylistPath));
    if (ppath == NULL) {
        MAW_PERROR("calloc");
        goto end;
    }

    ppath->path = strdup(value);
    TAILQ_INSERT_TAIL(&playlist->playlist_paths_head, ppath, entry);

    MAW_LOGF(MAW_DEBUG, ".%s.m3u added: %s", playlist->name, ppath->path);
    r = 0;
end:
    return r;
}

// A `YAML_KEY_TOKEN` is not a leaf.
static int maw_cfg_parse_key(MawConfig *cfg, YamlContext *ctx,
                             yaml_token_t *token) {
    int r = MAW_ERR_INTERNAL;
    MetadataEntry *metadata_entry = NULL;
    PlaylistEntry *playlist_entry = NULL;
    const char *key = NULL;
    enum YamlKey mkey;

    key = (const char *)token->data.scalar.value;
    mkey = maw_cfg_parse_key_to_enum(ctx, key);

    switch (mkey) {
    case KEY_INVALID:
        MAW_LOGF(MAW_ERROR, "Unexpected key for current context: '%s'", key);
        goto end;
    case KEY_ARBITRARY:
        switch (ctx->keypath[0]) {
        case KEY_METADATA:
            // New entry under 'metadata'
            metadata_entry = calloc(1, sizeof(MetadataEntry));
            if (metadata_entry == NULL) {
                MAW_PERROR("calloc");
                goto end;
            }
            metadata_entry->value.title = NULL;
            metadata_entry->value.album = NULL;
            metadata_entry->value.artist = NULL;
            metadata_entry->value.cover_path = NULL;
            metadata_entry->value.cover_policy = COVER_POLICY_NONE;
            metadata_entry->value.clean = false;
            metadata_entry->pattern = strdup(key);
            TAILQ_INSERT_TAIL(&cfg->metadata_head, metadata_entry, entry);
            break;
        case KEY_PLAYLISTS:
            // New entry under 'playlists'
            playlist_entry = calloc(1, sizeof(PlaylistEntry));
            if (playlist_entry == NULL) {
                MAW_PERROR("calloc");
                goto end;
            }
            // Initialize the list of paths
            TAILQ_INIT(&playlist_entry->value.playlist_paths_head);
            playlist_entry->value.name = strdup(key);
            TAILQ_INSERT_TAIL(&cfg->playlists_head, playlist_entry, entry);
            break;
        default:
            MAW_LOGF(MAW_ERROR, "Unexpected key at index 0: %s",
                     maw_cfg_key_tostr(ctx->keypath[0]));
            goto end;
        }
        break;
    default:
        break;
    }

    maw_cfg_key_push(ctx, key, mkey);

    r = 0;
end:
    return r;
}

// A `YAML_VALUE_TOKEN` is a leaf.
static int maw_cfg_parse_value(MawConfig *cfg, YamlContext *ctx,
                               yaml_token_t *token) {
    int r = MAW_ERR_INTERNAL;
    const char *value;
    Metadata *metadata = NULL;
    Playlist *playlist = NULL;

    value = (const char *)token->data.scalar.value;

    switch (ctx->key_count) {
    case 0:
        // No leafs should occur under the root
        MAW_YAML_ERROR(ctx, token, "value", value);
        goto end;
    case 1:
        switch (ctx->keypath[0]) {
        case KEY_ART:
            r = maw_cfg_glob(value, &cfg->art_dir);
            if (r != 0)
                goto end;

            (void)maw_cfg_key_pop(ctx);
            break;
        case KEY_MUSIC:
            r = maw_cfg_glob(value, &cfg->music_dir);
            if (r != 0)
                goto end;

            (void)maw_cfg_key_pop(ctx);
            break;
        default:
            MAW_YAML_ERROR(ctx, token, "value", value);
            goto end;
        }
        break;
    case 2:
        switch (ctx->keypath[0]) {
        // playlists.<name>
        case KEY_PLAYLISTS:
            playlist =
                &TAILQ_LAST(&cfg->playlists_head, PlaylistEntryHead)->value;
            r = maw_cfg_add_to_playlist(playlist, value);
            if (r != 0)
                goto end;
            // XXX: Parent key is popped during YAML_BLOCK_END_TOKEN event
            break;
        default:
            MAW_YAML_WARN(ctx, token, "value", value);
        }
        break;
    case 3:
        switch (ctx->keypath[0]) {
        // metadata.<path>.<metadata field>
        case KEY_METADATA:
            metadata =
                &TAILQ_LAST(&cfg->metadata_head, MetadataEntryHead)->value;
            r = maw_cfg_set_metadata_field(cfg, ctx, token, metadata, value);
            if (r != 0)
                goto end;

            (void)maw_cfg_key_pop(ctx);
            break;
        default:
            MAW_YAML_WARN(ctx, token, "value", value);
        }
        break;
    default:
        MAW_YAML_ERROR(ctx, token, "value", value);
        goto end;
    }

    r = 0;
end:
    return r;
}

static void maw_cfg_ctx_dump(YamlContext *ctx) {
    char out[MAW_PATH_MAX];
    out[0] = '\0';

    for (int i = 0; i < MAW_CFG_MAX_DEPTH; i++) {
        (void)strlcat(out, maw_cfg_key_tostr(ctx->keypath[i]), sizeof(out));
        (void)strlcat(out, "|", sizeof(out));
    }

    if (strlen(out) > 0) {
        out[strlen(out) - 1] = '\0';
    }

    MAW_LOGF(MAW_DEBUG, "YAML context: %s", out);
}

void maw_cfg_free(MawConfig *cfg) {
    MetadataEntry *m;
    PlaylistEntry *p;
    PlaylistPath *pp;

    if (cfg == NULL)
        return;

    free(cfg->music_dir);
    free(cfg->art_dir);

    while (!TAILQ_EMPTY(&(cfg->metadata_head))) {
        m = TAILQ_FIRST(&(cfg->metadata_head));
        free((void *)m->pattern);
        free((void *)m->value.title);
        free((void *)m->value.album);
        free((void *)m->value.artist);
        free((void *)m->value.cover_path);

        TAILQ_REMOVE(&(cfg->metadata_head), m, entry);
        free(m);
    }

    while (!TAILQ_EMPTY(&(cfg->playlists_head))) {
        p = TAILQ_FIRST(&(cfg->playlists_head));

        while (!TAILQ_EMPTY(&(p->value.playlist_paths_head))) {
            pp = TAILQ_FIRST(&(p->value.playlist_paths_head));
            free((void *)pp->path);

            TAILQ_REMOVE(&(p->value.playlist_paths_head), pp, entry);
            free(pp);
        }

        TAILQ_REMOVE(&(cfg->playlists_head), p, entry);
        free((void *)p->value.name);
        free(p);
    }

    free(cfg);
}

int maw_cfg_parse(const char *filepath, MawConfig **cfg) {
    int r = MAW_ERR_INTERNAL;
    bool done = false;
    yaml_token_t token;
    yaml_parser_t *parser;
    FILE *fp = NULL;

    YamlContext ctx = {
        .filepath = filepath,
        .next_token_type = 0,
        .keypath = {0},
        .key_count = 0,
    };

    parser = calloc(1, sizeof(yaml_parser_t));
    if (parser == NULL) {
        MAW_PERROR("calloc");
        goto end;
    }

    // Initialize config object
    *cfg = calloc(1, sizeof(MawConfig));
    if (*cfg == NULL) {
        MAW_PERROR("calloc");
        goto end;
    }
    (*cfg)->art_dir = NULL;
    (*cfg)->music_dir = NULL;
    TAILQ_INIT(&(*cfg)->metadata_head);
    TAILQ_INIT(&(*cfg)->playlists_head);

    r = maw_cfg_yaml_init(ctx.filepath, &parser, &fp);
    if (r != 0) {
        goto end;
    }

    while (!done) {
        r = yaml_parser_scan(parser, &token);
        if (r != 1) {
            r = MAW_ERR_YAML;
            MAW_LOGF(MAW_ERROR, "%s: Error parsing yaml", ctx.filepath);
            goto end;
        }

        switch (token.type) {
        case YAML_KEY_TOKEN:
        case YAML_VALUE_TOKEN:
            ctx.next_token_type = token.type;
            break;
        case YAML_SCALAR_TOKEN:
            if (ctx.next_token_type == YAML_KEY_TOKEN) {
                r = maw_cfg_parse_key(*cfg, &ctx, &token);
            }
            else {
                r = maw_cfg_parse_value(*cfg, &ctx, &token);
            }
            if (r != 0)
                goto end;
            break;
        case YAML_BLOCK_SEQUENCE_START_TOKEN:
            MAW_LOG(MAW_DEBUG, "BEGIN sequence");
            break;
        case YAML_BLOCK_ENTRY_TOKEN:
            MAW_LOG(MAW_DEBUG, "BEGIN block");
            break;
        case YAML_BLOCK_MAPPING_START_TOKEN:
            MAW_LOG(MAW_DEBUG, "BEGIN mapping");
            break;
        case YAML_BLOCK_END_TOKEN:
            MAW_LOG(MAW_DEBUG, "END mapping");
            // End of a block mapping, e.g. end of a `Metadata` entry
            // Pop the current key and move up one level
            (void)maw_cfg_key_pop(&ctx);
            break;
        case YAML_STREAM_END_TOKEN:
            done = true;
            break;
        default:
            MAW_LOGF(MAW_DEBUG, "Token event: #%d", token.type);
            break;
        }

        yaml_token_delete(&token);
    }
    r = 0;
    maw_cfg_ctx_dump(&ctx);
end:
    maw_cfg_yaml_deinit(parser, fp);
    return r;
}

#ifdef MAW_TEST
void maw_cfg_dump(MawConfig *cfg) {
    MetadataEntry *m;
    PlaylistEntry *p;
    PlaylistPath *pp;
    MAW_LOG(MAW_DEBUG, "---");
    MAW_LOGF(MAW_DEBUG, MAW_CFG_KEY_ART ": %s", cfg->art_dir);
    MAW_LOGF(MAW_DEBUG, MAW_CFG_KEY_MUSIC ": %s", cfg->music_dir);
    MAW_LOG(MAW_DEBUG, "metadata:");
    TAILQ_FOREACH(m, &(cfg->metadata_head), entry) {
        MAW_LOGF(MAW_DEBUG, "  %s:", m->pattern);
        MAW_LOGF(MAW_DEBUG, "    " MAW_CFG_KEY_TITLE ": %s", m->value.title);
        MAW_LOGF(MAW_DEBUG, "    " MAW_CFG_KEY_ALBUM ": %s", m->value.album);
        MAW_LOGF(MAW_DEBUG, "    " MAW_CFG_KEY_ARTIST ": %s", m->value.artist);
        MAW_LOGF(MAW_DEBUG, "    " MAW_CFG_KEY_COVER ": %s",
                 MAW_COVER_TOSTR((&m->value)));
        MAW_LOGF(MAW_DEBUG, "    " MAW_CFG_KEY_CLEAN ": %d", m->value.clean);
    }
    MAW_LOG(MAW_DEBUG, "playlists:");
    TAILQ_FOREACH(p, &(cfg->playlists_head), entry) {
        MAW_LOGF(MAW_DEBUG, "  %s:", p->value.name);
        TAILQ_FOREACH(pp, &(p->value.playlist_paths_head), entry) {
            MAW_LOGF(MAW_DEBUG, "    - %s", pp->path);
        }
    }
}
#endif

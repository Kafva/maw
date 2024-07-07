
#include "maw/cfg.h"
#include "maw/log.h"
#include "maw/maw.h"

#define KEY_LAST(c) c->keypath[c->key_count - 1]
#define TOSTR(arg) #arg
#define CASE_RET(a) case a: return TOSTR(a)
#define MAW_STRLCPY(dst, src) do {\
    size_t __r; \
    __r = strlcpy(dst, src, sizeof(dst)); \
    if (__r >= sizeof(dst)) { \
        MAW_LOGF(MAW_ERROR, "strlcpy truncation: '%s'", src); \
        goto end; \
    } \
} while (0)

#define MAW_YAML_UNXEXPECTED(level, ctx, token, type, scalar) do { \
    MAW_LOGF(level, "Unexpected %s '%s' at %s:%zu:%zu", \
             type, \
             scalar, \
             ctx->filepath, \
             token->start_mark.line + 1, \
             token->start_mark.column); \
} while (0)

#define MAW_YAML_WARN(ctx, token, type, scalar) \
    MAW_YAML_UNXEXPECTED(MAW_WARN, ctx, token, type, scalar);

#define MAW_YAML_ERROR(ctx, token, type, scalar) \
    MAW_YAML_UNXEXPECTED(MAW_ERROR, ctx, token, type, scalar);

static int maw_cfg_yaml_init(const char *filepath, yaml_parser_t **parser, FILE **fp) {
    // The API for libyaml returns 0 on failure...
    int yr = 0;
    int r = INTERNAL_ERROR;

    *fp = fopen(filepath, "r");
    if (*fp == NULL) {
        MAW_PERROR(filepath);
        goto end;
    }

    yr = yaml_parser_initialize(*parser);
    if (yr != 1) {
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
        CASE_RET(KEY_COVER_POLICY);
        CASE_RET(KEY_CLEAN);
        default:
            return NULL;
    }
}

static void maw_cfg_deinit(MawConfig *cfg) {
    if (cfg == NULL)
        return;

    free(cfg->music_dir);
    free(cfg->art_dir);

    // TODO

    free(cfg);
}

static void maw_cfg_key_push(YamlContext *ctx, const char *key, enum YamlKey mkey) {
    const char *ctxstr = maw_cfg_key_tostr(KEY_LAST(ctx));
    if (ctxstr == NULL) {
        MAW_LOGF(MAW_DEBUG, "Pushing key: %s", key);
    }
    else {
        MAW_LOGF(MAW_DEBUG, "Pushing key onto %s: %s",  ctxstr,  key);
    }
    ctx->keypath[ctx->key_count++] = mkey;
}

static bool maw_cfg_key_pop(YamlContext *ctx) {
    if (ctx->key_count == 0) {
        return false;
    }
    MAW_LOGF(MAW_DEBUG, "Popping key: %s",  maw_cfg_key_tostr(KEY_LAST(ctx)));
    KEY_LAST(ctx) = KEY_NONE;
    ctx->key_count--;
    return true;
}

static enum YamlKey maw_cfg_parse_key_to_enum(YamlContext *ctx, const char *key) {
    switch (ctx->key_count) {
    case 0:
        if (STR_MATCH(MAW_CFG_KEY_METADATA, key)) {
            return KEY_METADATA;
        }
        else if (STR_MATCH(MAW_CFG_KEY_PLAYLISTS, key)) {
            return KEY_PLAYLISTS;
        }
        else if (STR_MATCH(MAW_CFG_KEY_MUSIC, key)) {
            return KEY_MUSIC;
        }
        else if (STR_MATCH(MAW_CFG_KEY_ART, key)) {
            return KEY_ART;
        }
        break;
    case 1:
        // Name of 'playlists' or 'metadata' entry
        return KEY_ARBITRARY;
    case 2:
        if (STR_MATCH(MAW_CFG_KEY_ALBUM, key)) {
            return KEY_ALBUM;
        }
        else if (STR_MATCH(MAW_CFG_KEY_ARTIST, key)) {
            return KEY_ARTIST;
        }
        else if (STR_MATCH(MAW_CFG_KEY_COVER, key)) {
            return KEY_COVER;
        }
        else if (STR_MATCH(MAW_CFG_KEY_COVER_POLICY, key)) {
            return KEY_COVER_POLICY;
        }
        else if (STR_MATCH(MAW_CFG_KEY_CLEAN, key)) {
            return KEY_CLEAN;
        }
        break;
    default:
        break;
    }
    return KEY_INVALID;
}

static int maw_cfg_set_metadata_field(YamlContext *ctx,
                                      yaml_token_t *token,
                                      Metadata *metadata,
                                      const char *value) {
    int r = INTERNAL_ERROR;
    switch (ctx->keypath[2]) {
        case KEY_ALBUM:
            metadata->album = strdup(value);
            break;
        case KEY_ARTIST:
            metadata->artist = strdup(value);
            break;
        case KEY_COVER:
            // TODO prepend art_dir
            metadata->cover_path = strdup(value);
            break;
        case KEY_COVER_POLICY:
            if (STR_CASE_MATCH("keep", value)) {
                metadata->cover_policy = COVER_KEEP;
            }
            else if (STR_CASE_MATCH("crop", value)) {
                metadata->cover_policy = COVER_CROP;
            }
            else if (STR_CASE_MATCH("clear", value)) {
                metadata->cover_policy = COVER_CLEAR;
            }
            else {
                MAW_YAML_ERROR(ctx, token, "value", value);
                goto end;
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

static int maw_cfg_add_to_playlist(YamlContext *ctx,
                                   yaml_token_t *token,
                                   Playlist *playlist,
                                   const char *value) {
    int r = INTERNAL_ERROR;
    PlaylistPath *ppath = NULL;

    ppath = calloc(1, sizeof(PlaylistPath));
    if (ppath == NULL) {
        MAW_PERROR("calloc");
        goto end;
    }

    ppath->path = strdup(value);
    SLIST_INSERT_HEAD(&playlist->playlist_paths_head, ppath, entry);

    MAW_LOGF(MAW_DEBUG, ".%s.m3u added: %s", playlist->name, ppath->path);
    r = 0;
end:
    return r;
}

// A `YAML_KEY_TOKEN` is not a leaf.
static int maw_parse_key(MawConfig *cfg, YamlContext *ctx, yaml_token_t *token) {
    int r = INTERNAL_ERROR;
    MetadataEntry *metadata_entry = NULL;
    PlaylistEntry *playlist_entry = NULL;
    const char *key = NULL;
    enum YamlKey mkey;

    key = (const char*)token->data.scalar.value;
    mkey = maw_cfg_parse_key_to_enum(ctx, key);

    switch (mkey) {
        case KEY_INVALID:
            goto end;
        case KEY_ARBITRARY:
            // All keys are valid under 'metadata' and 'playlists'
            switch (ctx->keypath[0]) {
                case KEY_METADATA:
                    // New entry under 'metadata'
                    metadata_entry = calloc(1, sizeof(MetadataEntry));
                    if (metadata_entry == NULL) {
                        MAW_PERROR("calloc");
                        goto end;
                    }
                    metadata_entry->value.filepath = strdup(key);
                    SLIST_INSERT_HEAD(&cfg->metadata_head, metadata_entry, entry);
                    break;
                case KEY_PLAYLISTS:
                    // New entry under 'playlists'
                    playlist_entry = calloc(1, sizeof(PlaylistEntry));
                    if (playlist_entry == NULL) {
                        MAW_PERROR("calloc");
                        goto end;
                    }
                    // Intialize the list of paths
                    SLIST_INIT(&playlist_entry->value.playlist_paths_head);
                    playlist_entry->value.name = strdup(key);
                    SLIST_INSERT_HEAD(&cfg->playlists_head, playlist_entry, entry);
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

    maw_cfg_key_push(ctx,  key, mkey);

    r = 0;
end:
    return r;
}

// A `YAML_VALUE_TOKEN` is a leaf.
static int maw_parse_value(MawConfig *cfg,
                           YamlContext *ctx,
                           yaml_token_t *token) {
    int r = INTERNAL_ERROR;
    const char *value;
    Metadata *metadata = NULL;
    Playlist *playlist = NULL;

    value = (const char*)token->data.scalar.value;

    switch (ctx->key_count) {
    case 0:
        // No leafs should occur under the root
        MAW_YAML_ERROR(ctx, token, "value", value);
        goto end;
    case 1:
        switch (ctx->keypath[0]) {
        case KEY_ART:
            cfg->art_dir = strdup(value);
            (void)maw_cfg_key_pop(ctx);
            break;
        case KEY_MUSIC:
            cfg->music_dir = strdup(value);
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
                playlist = &SLIST_FIRST(&cfg->playlists_head)->value;
                (void)maw_cfg_add_to_playlist(ctx, token, playlist, value);
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
                metadata = &SLIST_FIRST(&cfg->metadata_head)->value;
                (void)maw_cfg_set_metadata_field(ctx, token, metadata, value);
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
    char out[1024];
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

void maw_cfg_dump(MawConfig *cfg) {
    MetadataEntry *m;
    PlaylistEntry *p;
    PlaylistPath *pp;
    MAW_LOG(MAW_DEBUG, "---");
    MAW_LOGF(MAW_DEBUG, MAW_CFG_KEY_ART": %s", cfg->art_dir);
    MAW_LOGF(MAW_DEBUG, MAW_CFG_KEY_MUSIC": %s", cfg->music_dir);
    MAW_LOG(MAW_DEBUG, "metadata:");
    SLIST_FOREACH(m, &(cfg->metadata_head), entry) {
        MAW_LOGF(MAW_DEBUG, "  %s:", m->value.filepath);
        MAW_LOGF(MAW_DEBUG, "    "MAW_CFG_KEY_ALBUM": %s", m->value.album);
        MAW_LOGF(MAW_DEBUG, "    "MAW_CFG_KEY_ARTIST": %s", m->value.artist);
        MAW_LOGF(MAW_DEBUG, "    "MAW_CFG_KEY_COVER": %s", m->value.cover_path);
        MAW_LOGF(MAW_DEBUG, "    "MAW_CFG_KEY_COVER_POLICY": %d", m->value.cover_policy);
        MAW_LOGF(MAW_DEBUG, "    "MAW_CFG_KEY_CLEAN": %d", m->value.clean);
    }
    MAW_LOG(MAW_DEBUG, "playlists:");
    SLIST_FOREACH(p, &(cfg->playlists_head), entry) {
        MAW_LOGF(MAW_DEBUG, "  %s:", p->value.name);
        SLIST_FOREACH(pp, &(p->value.playlist_paths_head), entry) {
            MAW_LOGF(MAW_DEBUG, "    - %s", pp->path);
        }
    }
}

/**
 *
 * @param [in] filepath
 * @param [in,out] cfg
 *
 * @return 0 on success,
 */
int maw_cfg_parse(const char *filepath, MawConfig **cfg) {
    int yr = 0;
    int r = INTERNAL_ERROR;
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
    SLIST_INIT(&(*cfg)->metadata_head);
    SLIST_INIT(&(*cfg)->playlists_head);

    r = maw_cfg_yaml_init(ctx.filepath, &parser, &fp);
    if (r != 0) {
        goto end;
    }

    while (!done) {
        yr = yaml_parser_scan(parser, &token);
        if (yr != 1) {
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
                    (void)maw_parse_key(*cfg, &ctx, &token);
                }
                else {
                    (void)maw_parse_value(*cfg, &ctx, &token);
                }
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
    maw_cfg_dump(*cfg);
end:
    maw_cfg_yaml_deinit(parser, fp);
    if (r != 0) {
        maw_cfg_deinit(*cfg);
    }
    return r;
}

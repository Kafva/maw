
#include "maw/cfg.h"
#include "maw/log.h"
#include "maw/maw.h"

static int maw_cfg_yaml_init(const char *filepath, yaml_parser_t **parser, FILE **fp)
                 __attribute__((warn_unused_result));
static void maw_cfg_yaml_deinit(yaml_parser_t *parser, FILE *fp);

#define TOSTR(arg) #arg
#define MAW_STRLCPY(dst, src) do {\
    size_t __r; \
    __r = strlcpy(dst, src, sizeof(dst)); \
    if (__r >= sizeof(dst)) { \
        MAW_LOGF(MAW_ERROR, "strlcpy truncation: '%s'", src); \
        goto end; \
    } \
} while (0)

#define MAW_YAML_WARN(ctx, token, type, scalar) do { \
    MAW_LOGF(MAW_WARN, "Unexpected %s '%s' at %s:%zu:%zu", \
             type, \
             scalar, \
             ctx->filepath, \
             token->start_mark.line + 1, \
             token->start_mark.column); \
} while (0)

////////////////////////////////////////////////////////////////////////////////

static int maw_cfg_yaml_init(const char *filepath, yaml_parser_t **parser, FILE **fp) {
    int r = INTERNAL_ERROR;

    *fp = fopen(filepath, "r");
    if (*fp == NULL) {
        MAW_PERROR(filepath);
        goto end;
    }

    r = yaml_parser_initialize(*parser);
    if (r != 1) {
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

static void maw_cfg_deinit(MawConfig *cfg) {
    if (cfg == NULL)
        return;

    free(cfg->music_dir);
    free(cfg->art_dir);

    // TODO

    free(cfg);
}

static void maw_cfg_set_metadata_field(YamlContext *ctx,
                                       yaml_token_t *token,
                                       const char *key,
                                       Metadata *metadata,
                                       const char *value) {
    if (STR_MATCH("filepath", key)) {
        metadata->filepath = strdup(value);
    }
    else if (STR_MATCH("title", key)) {
        metadata->title = strdup(value);
    }
    else if (STR_MATCH("album", key)) {
        metadata->album = strdup(value);
    }
    else if (STR_MATCH("artist", key)) {
        metadata->artist = strdup(value);
    }
    else if (STR_MATCH("cover", key)) {
        // TODO prepend art_dir
        metadata->cover_path = strdup(value);
    }
    else if (STR_MATCH("cover_policy", key)) {
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
            MAW_YAML_WARN(ctx, token, "value", value);
            goto end;
        }
    }
    else if (STR_MATCH("clean", key)) {
        metadata->clean = strncasecmp("true", value, sizeof("true") - 1) == 0;
    }
    else {
        MAW_YAML_WARN(ctx, token, "key", key);
        goto end;
    }

    MAW_LOGF(MAW_DEBUG, "Setting %s: %s", key, value);
end:
    return;
}

// A `YAML_KEY_TOKEN` is not a leaf.
static int maw_parse_key(MawConfig *cfg, YamlContext *ctx, yaml_token_t *token) {
    int r = INTERNAL_ERROR;
    MetadataEntry *metadata_entry = NULL;
    YamlKey *key = NULL;

    key = calloc(1, sizeof(YamlKey));
    if (key == NULL) {
        goto end;
    }
    key->value = strdup((char*)token->data.scalar.value);

    switch (ctx->current_section) {
        case MAW_CFG_SECTION_TOP:
            if (STR_MATCH("playlists", key->value)) {
                ctx->current_section = MAW_CFG_SECTION_PLAYLISTS;
            }
            else if (STR_MATCH("metadata", key->value)) {
                ctx->current_section = MAW_CFG_SECTION_METADATA;
            }
            else if (!STR_MATCH("art_dir", key->value) &&
                     !STR_MATCH("music_dir", key->value)) {
                MAW_YAML_WARN(ctx, token, "key", key->value);
            }
            break;
        case MAW_CFG_SECTION_METADATA:
            // Create a new metadata entry with the current key as the filepath
            metadata_entry = calloc(1, sizeof(MetadataEntry));
            if (metadata_entry == NULL) {
                MAW_PERROR("calloc");
                goto end;
            }
            metadata_entry->value.filepath = strdup(key->value);
            SLIST_INSERT_HEAD(&cfg->metadata_head, metadata_entry, entry);
            // Move one level deeper
            ctx->current_section = MAW_CFG_SECTION_METADATA_ENTRY;
            break;
        case MAW_CFG_SECTION_METADATA_ENTRY:
            // OK: the next Value should go into the latest Metadata item
            break;
        case MAW_CFG_SECTION_PLAYLISTS:
        case MAW_CFG_SECTION_PLAYLISTS_ENTRY:
            // TODO
            break;
    }

    // Add the current key to the context list
    MAW_LOGF(MAW_DEBUG, "Pushing key: %s", key->value);
    SLIST_INSERT_HEAD(&ctx->keys_head, key, entry);

    r = 0;
end:
    return r;
}

// A `YAML_VALUE_TOKEN` is a leaf.
static int maw_parse_value(MawConfig *cfg,
                           YamlContext *ctx,
                           yaml_token_t *token) {
    int r = INTERNAL_ERROR;
    const char *key;
    const char *value;
    MetadataEntry *metadata_entry = NULL;

    if (SLIST_EMPTY(&ctx->keys_head)) {
        r = 0;
        goto end;
    }
    key = SLIST_FIRST(&ctx->keys_head)->value;
    value = (const char*)token->data.scalar.value;

    switch (ctx->current_section) {
        case MAW_CFG_SECTION_TOP:
            if (STR_MATCH("art_dir", key)) {
                MAW_LOGF(MAW_DEBUG, "Setting %s: %s", key, value);
                cfg->art_dir = strdup(value);
            }
            else if (STR_MATCH("music_dir", key)) {
                MAW_LOGF(MAW_DEBUG, "Setting %s: %s", key, value);
                cfg->music_dir = strdup(value);
            }
            else {
                MAW_YAML_WARN(ctx, token, "key", key);
            }
            // Pop the key
            MAW_LOGF(MAW_DEBUG, "Popping key: %s", key);
            SLIST_REMOVE_HEAD(&ctx->keys_head, entry);
            break;
        case MAW_CFG_SECTION_METADATA:
        case MAW_CFG_SECTION_PLAYLISTS:
            // Should never happen
            MAW_YAML_WARN(ctx, token, "key", key);
            goto end;
        case MAW_CFG_SECTION_METADATA_ENTRY:
            // Assign the value to the latest Metadata item
            if (SLIST_EMPTY(&cfg->metadata_head)) {
                goto end;
            }
            metadata_entry = SLIST_FIRST(&cfg->metadata_head);
            maw_cfg_set_metadata_field(ctx, token, key, &metadata_entry->value, value);
            // Pop the key
            MAW_LOGF(MAW_DEBUG, "Popping key: %s", key);
            SLIST_REMOVE_HEAD(&ctx->keys_head, entry);
            break;
        case MAW_CFG_SECTION_PLAYLISTS_ENTRY:
            break;
    }

    r = 0;
end:
    return r;
}

int maw_cfg_yaml_parse(const char *filepath, MawConfig **cfg) {
    int r = INTERNAL_ERROR;
    bool done = false;
    yaml_token_t token;
    yaml_parser_t *parser;
    FILE *fp = NULL;

    YamlContext ctx = {
        .filepath = filepath,
        .next_token_type = 0,
        .current_section = MAW_CFG_SECTION_TOP
    };
    SLIST_INIT(&ctx.keys_head);

    parser = calloc(1, sizeof(yaml_parser_t));
    if (parser == NULL) {
        MAW_PERROR("calloc");
        goto end;
    }

    *cfg = calloc(1, sizeof(MawConfig));
    if (*cfg == NULL) {
        MAW_PERROR("calloc");
        goto end;
    }
    (*cfg)->art_dir = NULL;
    (*cfg)->music_dir = NULL;

    // Initialize linked lists
    SLIST_INIT(&(*cfg)->metadata_head);
    SLIST_INIT(&(*cfg)->playlists_head);

    r = maw_cfg_yaml_init(ctx.filepath, &parser, &fp);
    if (r != 0) {
        goto end;
    }

    while (!done) {
        r = yaml_parser_scan(parser, &token);
        if (r != 1) {
            MAW_LOGF(MAW_ERROR, "%s: Error parsing yaml", ctx.filepath);
            r = INTERNAL_ERROR;
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
            case YAML_STREAM_END_TOKEN:
                done = true;
                break;
            default:
                break;
        }

        yaml_token_delete(&token);


        // NOTE the order:
        // Scalar event --> Mapping event
        //
        // There is no mapping event for scalar->scalar mappings, only for
        // scalar->map mappings.
        //
        // There are a set of keys that are valid at each level
        // * top
        //  - metadata
        //  - playlists
        //  - art_dir (no mapping event)
        //  - music_dir  (no mapping event)
        //
        // * metadata_entry (key)
        //  - title (no mapping event)
        //  ...
        //
        // * playlist_entry (key)
        //    [sequence]
        //
        //
        // onScalar( current_level )
        //
        // On scalar event: save the value and match against it in the Mapping
        // event
        // On mapping event: change the section state
    }
    r = 0;
    maw_cfg_dump(*cfg);
end:
    maw_cfg_yaml_deinit(parser, fp);
    if (r != 0) {
        maw_cfg_deinit(*cfg);
    }
    return r;
}

void maw_cfg_dump(MawConfig *cfg) {
    MetadataEntry *m;
    MAW_LOG(MAW_DEBUG, "=== Configuration");
    MAW_LOGF(MAW_DEBUG, "  "MAW_CFG_ART_KEY": %s", cfg->art_dir);
    MAW_LOGF(MAW_DEBUG, "  "MAW_CFG_MUSIC_KEY": %s", cfg->music_dir);

    SLIST_FOREACH(m, &(cfg->metadata_head), entry) {
        MAW_LOGF(MAW_DEBUG, "%s:", m->value.filepath);
        MAW_LOGF(MAW_DEBUG, "  album: %s", m->value.album);
        MAW_LOGF(MAW_DEBUG, "  artist: %s", m->value.artist);
        MAW_LOGF(MAW_DEBUG, "  cover: %s", m->value.cover_path);
        MAW_LOGF(MAW_DEBUG, "  cover_policy: %d", m->value.cover_policy);
        MAW_LOGF(MAW_DEBUG, "  clean: %d", m->value.clean);
    }
    MAW_LOG(MAW_DEBUG, "===");
}

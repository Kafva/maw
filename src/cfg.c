
#include "maw/cfg.h"
#include "maw/log.h"
#include "maw/maw.h"

static int maw_cfg_yaml_init(const char *filepath, yaml_parser_t **parser, FILE **fp)
                 __attribute__((warn_unused_result));
static void maw_cfg_yaml_deinit(yaml_parser_t *parser, FILE *fp);

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

////////////////////////////////////////////////////////////////////////////////

static const char *maw_cfg_section_tostr(MawConfigSection section) {
    switch (section) {
        CASE_RET(MAW_CFG_SECTION_TOP);
        CASE_RET(MAW_CFG_SECTION_ART_DIR);
        CASE_RET(MAW_CFG_SECTION_MUSIC_DIR);
        CASE_RET(MAW_CFG_SECTION_PLAYLISTS);
        CASE_RET(MAW_CFG_SECTION_PLAYLISTS_ENTRY);
        CASE_RET(MAW_CFG_SECTION_METADATA);
        CASE_RET(MAW_CFG_SECTION_METADATA_ENTRY);
    }
}

static void maw_cfg_set_metadata_field(const char *key, 
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
    else if (STR_MATCH("cover_path", key)) {
        metadata->cover_path = strdup(value);
    }
    else if (STR_MATCH("cover_policy", key)) {
        // TODO convert string to enum
        metadata->cover_policy = 1;
    }
    else if (STR_MATCH("clean", key)) {
        metadata->clean = strncasecmp("true", value, sizeof("true") - 1) == 0;
    }
    else {
        MAW_LOGF(MAW_WARN, "Skipping unknown key: '%s'", key);
    }
}

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

int maw_cfg_yaml_parse(const char *filepath, MawConfig **cfg) {
    int r = INTERNAL_ERROR;
    bool done = false;
    yaml_event_t event;
    yaml_parser_t *parser;
    FILE *fp = NULL;
    char *value = NULL;
    char prev_value[1024] = {0};
    char current_metadata_key[256] = {0};

    PlaylistEntry *playlist_entry = NULL;
    MetadataEntry *metadata_entry = NULL;
    MawConfigSection current_section = MAW_CFG_SECTION_TOP;
    
    bool new_mapping = false;
    bool used_as_key = false;

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

    r = maw_cfg_yaml_init(filepath, &parser, &fp);
    if (r != 0) {
        goto end;
    }

    while (!done) {
        r = yaml_parser_parse(parser, &event);
        if (r != 1) {
            MAW_LOGF(MAW_ERROR, "%s: Error parsing yaml", filepath);
            r = INTERNAL_ERROR;
            goto end;
        }

        // NOTE the order:
        // Scalar event --> Mapping event

        switch (event.type) {
        case YAML_MAPPING_START_EVENT:
            MAW_LOGF(MAW_DEBUG, "== BEGIN mapping %s", maw_cfg_section_tostr(current_section));
            // Move down one level
            switch (current_section) {
            case MAW_CFG_SECTION_TOP:
                if (STR_MATCH(MAW_CFG_MUSIC_KEY, prev_value)) {
                    current_section = MAW_CFG_SECTION_MUSIC_DIR;
                }
                else if (STR_MATCH(MAW_CFG_ART_KEY, prev_value)) {
                    current_section = MAW_CFG_SECTION_ART_DIR;
                }
                else if (STR_MATCH(MAW_CFG_PLAYLISTS_KEY, prev_value)) {
                    current_section = MAW_CFG_SECTION_PLAYLISTS;
                }
                else if (STR_MATCH(MAW_CFG_METADATA_KEY, prev_value)) {
                    current_section = MAW_CFG_SECTION_METADATA;
                }
                else {
                    MAW_LOGF(MAW_WARN, "Unexpected scalar '%s' at %s:%zu:%zu",
                             value, filepath, event.start_mark.line, event.start_mark.column);
                }
                break;
            case MAW_CFG_SECTION_METADATA:
            case MAW_CFG_SECTION_PLAYLISTS:
                // OK: encountered the metadata or playlists array

                break;
            case MAW_CFG_SECTION_METADATA_ENTRY:
                // The previous scalar value is the path
                if (prev_value[0] == '\0') {
                    MAW_LOGF(MAW_ERROR, "Unexpected mapping start at %s:%zu:%zu",
                             filepath, event.start_mark.line, event.start_mark.column);
                    goto end;
                }

                metadata_entry = calloc(1, sizeof(PlaylistEntry));
                if (metadata_entry == NULL) {
                    MAW_PERROR("calloc");
                    goto end;
                }

                metadata_entry->value.filepath = strdup(prev_value);
                SLIST_INSERT_HEAD(&(*cfg)->metadata_head, metadata_entry, entry);
                metadata_entry = NULL;

                break;
                break;
            default:
                MAW_LOGF(MAW_ERROR, "Unexpected mapping start at %s:%zu:%zu",
                         filepath, event.start_mark.line, event.start_mark.column);
                goto end;
            }            
            break;
        case YAML_MAPPING_END_EVENT:
            // Move up one level
            MAW_LOGF(MAW_DEBUG, "== END mapping %s", maw_cfg_section_tostr(current_section));
            switch (current_section) {
            case MAW_CFG_SECTION_TOP:
                // OK: end of document
                break;
            case MAW_CFG_SECTION_PLAYLISTS:
            case MAW_CFG_SECTION_METADATA:
                // OK: end of metadata or playlists section 
                current_section = MAW_CFG_SECTION_TOP;
                break;
            case MAW_CFG_SECTION_METADATA_ENTRY:
                // OK: end of metadata entry
                current_section = MAW_CFG_SECTION_METADATA;
                break;
            case MAW_CFG_SECTION_PLAYLISTS_ENTRY:
            case MAW_CFG_SECTION_ART_DIR:
            case MAW_CFG_SECTION_MUSIC_DIR:
                // No mapping events should occur under 'playlists', 'art_dir' or 'music_dir'
                MAW_LOGF(MAW_ERROR, "Unexpected mapping end at %s:%zu:%zu",
                         filepath, event.start_mark.line, event.start_mark.column);
                goto end;
            }            
            break;
        case YAML_SEQUENCE_START_EVENT:
            // Move down one level
            MAW_LOGF(MAW_DEBUG, "== BEGIN sequence %s", maw_cfg_section_tostr(current_section));
            switch (current_section) {
            case MAW_CFG_SECTION_PLAYLISTS:
                // TODO

                // playlist_entry = calloc(1, sizeof(PlaylistEntry));
                // if (playlist_entry == NULL) {
                //     MAW_PERROR("calloc");
                //     goto end;
                // }

                // SLIST_INSERT_HEAD(&(*cfg)->playlists_head, playlist_entry, entry);

                current_section = MAW_CFG_SECTION_PLAYLISTS_ENTRY;
                break;
            default:
                // Sequence start events should only occur under 'playlists'
                MAW_LOGF(MAW_ERROR, "Unexpected sequence start at %s:%zu:%zu",
                         filepath, event.start_mark.line, event.start_mark.column);
                goto end;
            }
            break;
        case YAML_SEQUENCE_END_EVENT:
            // Move up one level
            MAW_LOGF(MAW_DEBUG, "== END sequence %s", maw_cfg_section_tostr(current_section));
            switch (current_section) {
            case MAW_CFG_SECTION_PLAYLISTS_ENTRY:
                current_section = MAW_CFG_SECTION_PLAYLISTS;
                break;
            default:
                // Sequence end events should only occur under a 'playlists' entry
                MAW_LOGF(MAW_ERROR, "Unexpected sequence end at %s:%zu:%zu",
                         filepath, event.start_mark.line, event.start_mark.column);
                goto end;
            }
            break;
        case YAML_SCALAR_EVENT:
            value = (char*)event.data.scalar.value;

            switch (current_section) {
            case MAW_CFG_SECTION_TOP:
                MAW_LOGF(MAW_DEBUG, "Scalar: %s", value);
                break;
            case MAW_CFG_SECTION_ART_DIR:
                MAW_LOGF(MAW_DEBUG, "Scalar: %s", value);
                (*cfg)->art_dir = strdup(value);
                current_section = MAW_CFG_SECTION_TOP;
                break;
            case MAW_CFG_SECTION_MUSIC_DIR:
                MAW_LOGF(MAW_DEBUG, "Scalar: %s", value);
                (*cfg)->music_dir = strdup(value);
                current_section = MAW_CFG_SECTION_TOP;
                break;
            case MAW_CFG_SECTION_METADATA_ENTRY:
                if (current_metadata_key[0] == '\0') {
                    MAW_STRLCPY(current_metadata_key, value);
                    MAW_LOGF(MAW_DEBUG, "Scalar key: %s", value);
                    break;
                }

                if (SLIST_EMPTY(&(*cfg)->metadata_head)) {
                    MAW_LOG(MAW_ERROR, "Unexpected empty metadata list");
                    goto end;
                }

                MAW_LOGF(MAW_DEBUG, "Scalar value: %s", value);
                maw_cfg_set_metadata_field(current_metadata_key,
                                           &SLIST_FIRST(&(*cfg)->metadata_head)->value,
                                           value);
                current_metadata_key[0] = '\0';
                break;
            default:
                break;
            // case MAW_CFG_SECTION_PLAYLISTS:
            //     current_section = MAW_CFG_SECTION_PLAYLISTS_ENTRY;
            //     break;
            // case MAW_CFG_SECTION_METADATA:
            //     break;
            // case MAW_CFG_SECTION_METADATA_ENTRY:
            //     break;
            // case MAW_CFG_SECTION_PLAYLISTS_ENTRY:
            //     break;
            }
            
            // XXX: save previous value for use in BEGIN events
            MAW_STRLCPY(prev_value, value);

            break;
        default:
            break;
        }

        done = event.type == YAML_STREAM_END_EVENT;
        yaml_event_delete(&event);
    }

    r = 0;
    maw_cfg_dump(*cfg);
end:
    maw_cfg_yaml_deinit(parser, fp);
    if (r != 0) {
        maw_cfg_deinit(*cfg);
    }
    free(metadata_entry);
    free(playlist_entry);
    return r;
}

void maw_cfg_dump(MawConfig *cfg) {
    MetadataEntry *m;
    MAW_LOG(MAW_DEBUG, "===");
    MAW_LOGF(MAW_DEBUG, MAW_CFG_ART_KEY": %s", cfg->art_dir);
    MAW_LOGF(MAW_DEBUG, MAW_CFG_MUSIC_KEY": %s", cfg->music_dir);

    SLIST_FOREACH(m, &(cfg->metadata_head), entry) {
        MAW_LOGF(MAW_DEBUG, "Metadata: %s", m->value.title);
    }
}

#include "maw/cfg.h"
#include "maw/log.h"
#include "maw/maw.h"

static int maw_cfg_yaml_init(const char *filepath, yaml_parser_t **parser, FILE **fp)
                 __attribute__((warn_unused_result));
static void maw_cfg_yaml_deinit(yaml_parser_t *parser, FILE *fp);

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

static void free_metadata(MetadataEntry *entry) {
    if (entry == NULL)
        return;
    free_metadata(entry->next);
    free(entry);
}

static void free_playlists(PlaylistEntry *entry) {
    if (entry == NULL)
        return;
    free_playlists(entry->next);
    free(entry);
}

static PlaylistEntry* alloc_playlist_entry(void) {
    PlaylistEntry *entry;
    return entry;
}

static void maw_cfg_deinit(MawConfig *cfg) {
    if (cfg == NULL)
        return;

    free(cfg->music_dir);
    free(cfg->art_dir);

    free_metadata(cfg->metadata);
    free_playlists(cfg->playlists);

    free(cfg);
}


int maw_cfg_yaml_parse(const char *filepath, MawConfig **cfg) {
    int r = INTERNAL_ERROR;
    bool done = false;
    yaml_event_t event;
    yaml_parser_t *parser;
    FILE *fp = NULL;
    char *value = NULL;
    char *prev_value = NULL;
    char *current_key = NULL;

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

        switch (event.type) {
        case YAML_MAPPING_START_EVENT:
            // Move down one level
            MAW_LOG(MAW_DEBUG, "== BEGIN Mapping");
            switch (current_section) {
            case MAW_CFG_SECTION_TOP:
                // The first mapping event should be for an unnamed root object
                if (prev_value != NULL) {
                    MAW_LOGF(MAW_ERROR, "Unexpected mapping start at %s:%zu:%zu",
                             filepath, event.start_mark.line, event.start_mark.column);
                    goto end;
                }
                break;
            case MAW_CFG_SECTION_METADATA:
                // OK: encountered a new metadata entry
                current_section = MAW_CFG_SECTION_METADATA_ENTRY;
                current_key = strdup(prev_value);
                break;
            default:
                // No mapping events should occur under 'playlists'
                // No mapping events should occur under a metadata entry
                MAW_LOGF(MAW_ERROR, "Unexpected mapping start at %s:%zu:%zu",
                         filepath, event.start_mark.line, event.start_mark.column);
                goto end;
            }            
            break;
        case YAML_MAPPING_END_EVENT:
            // Move up one level
            MAW_LOG(MAW_DEBUG, "== END Mapping");
            switch (current_section) {
            case MAW_CFG_SECTION_TOP:
                break;
            case MAW_CFG_SECTION_PLAYLISTS:
            case MAW_CFG_SECTION_METADATA:
                current_section = MAW_CFG_SECTION_TOP;
                break;
            case MAW_CFG_SECTION_METADATA_ENTRY:
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
            MAW_LOG(MAW_DEBUG, "== BEGIN Sequence");
            switch (current_section) {
            case MAW_CFG_SECTION_PLAYLISTS:
                playlist_entry = calloc(1, sizeof(PlaylistEntry));
                if (playlist_entry == NULL) {
                    MAW_PERROR("calloc");
                    goto end;
                }

                if ((*cfg)->playlists == NULL) {
                    (*cfg)->playlists = playlist_entry;
                }

                (*cfg)->playlist_count++;

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
            MAW_LOG(MAW_DEBUG, "== END Sequence");
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
            MAW_LOGF(MAW_DEBUG, "Scalar: %s", value);

            switch (current_section) {
            case MAW_CFG_SECTION_TOP:
                if (STR_MATCH(MAW_CFG_MUSIC_KEY, value)) {
                    current_section = MAW_CFG_SECTION_MUSIC_DIR;
                }
                else if (STR_MATCH(MAW_CFG_ART_KEY, value)) {
                    current_section = MAW_CFG_SECTION_ART_DIR;
                }
                else {
                    MAW_LOGF(MAW_WARN, "Unexpected scalar at %s:%zu:%zu",
                             filepath, event.start_mark.line, event.start_mark.column);
                }
            case MAW_CFG_SECTION_ART_DIR:
                (*cfg)->art_dir = strdup(value);
                current_section = MAW_CFG_SECTION_TOP;
                break;
            case MAW_CFG_SECTION_MUSIC_DIR:
                (*cfg)->music_dir = strdup(value);
                current_section = MAW_CFG_SECTION_TOP;
                break;
            case MAW_CFG_SECTION_PLAYLISTS:

                current_section = MAW_CFG_SECTION_PLAYLISTS_ENTRY;
                break;
            case MAW_CFG_SECTION_METADATA:
                break;
            case MAW_CFG_SECTION_METADATA_ENTRY:
                break;
            case MAW_CFG_SECTION_PLAYLISTS_ENTRY:
                break;
            }
            














            free(prev_value);
            prev_value = strdup(value);

            // Bookeep the current key
            // if (new_mapping) {
            //     free(current_top_key);
            //     current_top_key = strdup(value);
            //     new_mapping = false;
            //     break;
            // }

            break;
        default:
            break;
        }

        done = event.type == YAML_STREAM_END_EVENT;
        yaml_event_delete(&event);
    }

    r = 0;
end:
    maw_cfg_yaml_deinit(parser, fp);
    if (r != 0) {
        maw_cfg_deinit(*cfg);
    }
    return r;
}

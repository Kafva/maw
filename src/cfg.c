#include "maw/cfg.h"
#include "maw/log.h"
#include "maw/maw.h"

static int maw_cfg_init(const char *filepath, yaml_parser_t **parser, FILE **fp)
                 __attribute__((warn_unused_result));
static void maw_cfg_deinit(yaml_parser_t *parser, FILE *fp);


////////////////////////////////////////////////////////////////////////////////

static int maw_cfg_init(const char *filepath, yaml_parser_t **parser, FILE **fp) {
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

static void maw_cfg_deinit(yaml_parser_t *parser, FILE *fp) {
    yaml_parser_delete(parser);
    free(parser);
    (void)fclose(fp);
}

int maw_cfg_parse(const char *filepath) {
    int r = INTERNAL_ERROR;
    bool done = false;
    yaml_event_t event;
    yaml_parser_t *parser;
    FILE *fp = NULL;

    parser = calloc(1, sizeof(yaml_parser_t));
    if (parser == NULL) {
        MAW_PERROR("calloc");
        goto end;
    }

    r = maw_cfg_init(filepath, &parser, &fp);
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
            MAW_LOG(MAW_DEBUG, "== BEGIN Mapping");
            break;
        case YAML_MAPPING_END_EVENT:
            MAW_LOG(MAW_DEBUG, "== END Mapping");
            break;
        case YAML_SEQUENCE_START_EVENT:
            MAW_LOG(MAW_DEBUG, "== BEGIN Sequence");
            break;
        case YAML_SEQUENCE_END_EVENT:
            MAW_LOG(MAW_DEBUG, "== END Sequence");
            break;
        case YAML_SCALAR_EVENT:
            MAW_LOGF(MAW_DEBUG, "Scalar: %s", event.data.scalar.value);
            break;
        default:
            break;
        }

        done = event.type == YAML_STREAM_END_EVENT;
        yaml_event_delete(&event);
    }

    r = 0;
end:
    maw_cfg_deinit(parser, fp);
    return r;
}

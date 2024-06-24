#include "maw/cfg.h"
#include "maw/log.h"
#include "maw/maw.h"

int maw_cfg_init(const char *filepath, yaml_parser_t *parser) {
    int r = INTERNAL_ERROR;
    FILE *fp = NULL;

    fp = fopen(filepath, "r");
    if (fp == NULL) {
        MAW_PERROR(filepath);
        goto end;
    }

    r = yaml_parser_initialize(parser);
    if (r != 1) {
        MAW_LOGF(MAW_ERROR, "%s: failed to initialize parser", filepath);
        goto end;
    }

    yaml_parser_set_input_file(parser, fp);

    r = 0;
end:
    (void)fclose(fp);
    return r;
}

void maw_cfg_deinit(yaml_parser_t *parser) {
    yaml_parser_delete(parser);
}


int maw_cfg_parse(const char *filepath) {
    int r = INTERNAL_ERROR;
    yaml_event_t event;
    yaml_parser_t parser;

    r = maw_cfg_init(filepath, &parser);
    if (r != 0) {
        goto end;
    }

    // do {
    //     // Get the next event
    //     if (!yaml_parser_parse(&parser, &event)) {
    //         fprintf(stderr, "Failed to parse YAML.\n");
    //         return 1;
    //     }

    //     // Check the type of the event
    //     switch (event.type) {
    //     case YAML_STREAM_START_EVENT:
    //     case YAML_STREAM_END_EVENT:
    //     case YAML_DOCUMENT_START_EVENT:
    //     case YAML_DOCUMENT_END_EVENT:
    //     case YAML_MAPPING_START_EVENT:
    //     case YAML_MAPPING_END_EVENT:
    //     case YAML_SEQUENCE_START_EVENT:
    //     case YAML_SEQUENCE_END_EVENT:
    //         // Ignore these events for now
    //         break;
    //     case YAML_SCALAR_EVENT:
    //         // Process scalar value
    //         printf("Scalar: %s\n", event.data.scalar.value);
    //         break;
    //     default:
    //         fprintf(stderr, "Unknown YAML event.\n");
    //         return 1;
    //     }

    //     // Check if parsing is done
    //     done = (event.type == YAML_STREAM_END_EVENT);

    //     // Free event resources
    //     yaml_event_delete(&event);
    // } while (!done);


end:
    maw_cfg_deinit(&parser);
    return r;
}

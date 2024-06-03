#include "cfg.h"

#include <yaml.h>

int maw_cfg_parse(const char *filepath) {
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open yaml file: %s\n", filepath);
        return 1;
    }

    // Initialize YAML parser
    yaml_parser_t parser;
    yaml_parser_initialize(&parser);

    // Set input file for the parser
    yaml_parser_set_input_file(&parser, file);

    // Start parsing YAML
    yaml_event_t event;
    int done = 0;

    do {
        // Get the next event
        if (!yaml_parser_parse(&parser, &event)) {
            fprintf(stderr, "Failed to parse YAML.\n");
            return 1;
        }

        // Check the type of the event
        switch (event.type) {
        case YAML_STREAM_START_EVENT:
        case YAML_STREAM_END_EVENT:
        case YAML_DOCUMENT_START_EVENT:
        case YAML_DOCUMENT_END_EVENT:
        case YAML_MAPPING_START_EVENT:
        case YAML_MAPPING_END_EVENT:
        case YAML_SEQUENCE_START_EVENT:
        case YAML_SEQUENCE_END_EVENT:
            // Ignore these events for now
            break;
        case YAML_SCALAR_EVENT:
            // Process scalar value
            printf("Scalar: %s\n", event.data.scalar.value);
            break;
        default:
            fprintf(stderr, "Unknown YAML event.\n");
            return 1;
        }

        // Check if parsing is done
        done = (event.type == YAML_STREAM_END_EVENT);

        // Free event resources
        yaml_event_delete(&event);
    } while (!done);

    // Clean up
    yaml_parser_delete(&parser);
    fclose(file);

    return 0;
}

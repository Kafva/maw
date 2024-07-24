#ifndef TESTS_MAW_VERIFY_H
#define TESTS_MAW_VERIFY_H

#include "maw/maw.h"

bool maw_verify(const MediaFile *mediafile);
bool maw_verify_file(const char *path, const char *expected_content);

#endif

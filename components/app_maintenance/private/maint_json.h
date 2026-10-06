#pragma once
#include <stdbool.h>
#include <stddef.h>
/* Shape guard before cJSON: one flat object, no decoded NUL strings.
 * Syntax and field types remain the JSON parser / endpoint's responsibility. */
bool maint_json_flat(const char *text,size_t size);

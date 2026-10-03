#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef uint32_t (*maint_random_fn)(void *context);
typedef struct {
    char pin[7];
    uint8_t token[16], failures;
    bool session, locked;
    uint32_t locked_until_ms;
} maint_auth_t;
typedef enum { MAINT_AUTH_OK, MAINT_AUTH_BAD_PIN, MAINT_AUTH_LOCKED } maint_auth_result_t;
void maint_auth_reset(maint_auth_t *auth, maint_random_fn random, void *context);
maint_auth_result_t maint_auth_login(maint_auth_t *auth, const char *pin, uint32_t now,
                                     maint_random_fn random, void *context, char token[33]);
bool maint_auth_check(const maint_auth_t *auth, const char *token);
void maint_auth_logout(maint_auth_t *auth);
unsigned maint_auth_retry_after(const maint_auth_t *auth, uint32_t now);

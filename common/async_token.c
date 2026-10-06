#include "async_token.h"
#include <stdatomic.h>
static atomic_uint token;
uint32_t async_token_next(void)
{
    uint32_t value=atomic_fetch_add(&token,1)+1;
    if (!value) value=atomic_fetch_add(&token,1)+1;
    return value;
}

#include "atom_slave_tx.h"
#include "i2c_private.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int critical;
int xSemaphoreTake(bool *mutex, unsigned timeout)
{ assert(timeout == 5 && !critical); if (*mutex) return 0; *mutex = true; return 1; }
int xSemaphoreGive(bool *mutex) { assert(*mutex && critical); *mutex = false; return 1; }
void fake_enter(int *lock) { assert(!critical && !*lock); critical = *lock = 1; }
void fake_exit(int *lock) { assert(critical && *lock); critical = *lock = 0; }
void i2c_ll_slave_disable_tx_it(fake_fifo_t *f) { assert(critical); f->enabled = false; }
void i2c_ll_slave_enable_tx_it(fake_fifo_t *f) { assert(critical); f->enabled = true; }
void i2c_ll_slave_clear_stretch(fake_fifo_t *f) { (void)f; assert(critical); }
void i2c_ll_txfifo_rst(fake_fifo_t *f) { assert(critical); f->count = 0; }
void i2c_ll_write_txfifo(fake_fifo_t *f, uint8_t *bytes, size_t size)
{ assert(critical && size <= 32 - f->count); memcpy(f->bytes + f->count, bytes, size); f->count += size; }
void *xRingbufferReceive(fake_ring_t *r, size_t *size, unsigned timeout)
{ assert(critical && timeout == 0); if (!r->count) return NULL; *size = r->count; return r->bytes; }
void vRingbufferReturnItem(fake_ring_t *r, void *item)
{ assert(critical && item == r->bytes); r->count = 0; }
int xRingbufferSend(fake_ring_t *r, const void *bytes, size_t size, unsigned timeout)
{
    assert(critical && timeout == 0 && size <= sizeof(r->bytes) - r->count);
    if (r->fail_send) return 0;
    memcpy(r->bytes + r->count, bytes, size); r->count += size; return 1;
}
/* Consume a master's read independently of the replacement implementation. */
static size_t master_read(fake_fifo_t *fifo, fake_ring_t *ring, uint8_t *out, size_t size)
{
    size_t read = 0;
    while (read < size) {
        if (!fifo->count) {
            size_t fill = ring->count < 32 ? ring->count : 32;
            if (!fill) break;
            memcpy(fifo->bytes, ring->bytes, fill); fifo->count = fill;
            memmove(ring->bytes, ring->bytes + fill, ring->count - fill); ring->count -= fill;
        }
        out[read++] = fifo->bytes[0];
        memmove(fifo->bytes, fifo->bytes + 1, --fifo->count);
    }
    return read;
}
int main(void)
{
    const unsigned partial_reads[] = {0, 1, 7, 19, 31, 32, 34, 35};
    const unsigned reply_sizes[] = {7, 19, 35};
    for (unsigned test = 0; test < sizeof(partial_reads) / sizeof(partial_reads[0]); ++test) {
        for (unsigned next = 0; next < sizeof(reply_sizes) / sizeof(reply_sizes[0]); ++next) {
            unsigned next_size = reply_sizes[next];
            fake_fifo_t fifo = {.count = 32, .enabled = true};
            fake_ring_t ring = {.count = 3}; bool mutex = false;
            fake_bus_t bus = {.hal = {.dev = &fifo}};
            struct fake_slave slave = {&bus, &mutex, &ring};
            memset(fifo.bytes, 0x55, sizeof(fifo.bytes)); memset(ring.bytes, 0x55, 3);
            uint8_t discarded[35]; assert(master_read(&fifo, &ring, discarded, partial_reads[test]) == partial_reads[test]);
            uint8_t fresh[35], result[36]; for (unsigned i = 0; i < 35; ++i) fresh[i] = (uint8_t)(i + 0x80);
            assert(atom_slave_replace_reply(&slave, fresh, next_size) == ESP_OK);
            assert(!mutex && !critical && fifo.enabled);
            assert(master_read(&fifo, &ring, result, sizeof(result)) == next_size);
            assert(!memcmp(result, fresh, next_size)); assert(!fifo.count && !ring.count);
        }
    }
    fake_fifo_t fifo = {.count = 8}; fake_ring_t ring = {.count = 3}; bool mutex = true;
    fake_bus_t bus = {.hal = {.dev = &fifo}}; struct fake_slave slave = {&bus, &mutex, &ring};
    uint8_t fresh[35] = {0};
    assert(atom_slave_replace_reply(&slave, fresh, sizeof(fresh)) == ESP_ERR_TIMEOUT);
    assert(fifo.count == 8 && ring.count == 3);
    mutex = false; ring.fail_send = true;
    assert(atom_slave_replace_reply(&slave, fresh, sizeof(fresh)) == ESP_ERR_NO_MEM);
    assert(!fifo.count && !ring.count && !fifo.enabled && !mutex);
    ring.fail_send = false;
    assert(atom_slave_replace_reply(&slave, fresh, sizeof(fresh)) == ESP_OK);
    assert(atom_slave_replace_reply(&slave, fresh, 36) == ESP_ERR_INVALID_ARG);
    assert(atom_slave_replace_reply(&slave, NULL, 0) == ESP_OK);
    assert(!fifo.count && !ring.count && !fifo.enabled && !mutex);
    uint8_t empty[35]; assert(master_read(&fifo,&ring,empty,sizeof(empty))==0);
    assert(atom_slave_replace_reply(&slave,NULL,1)==ESP_ERR_INVALID_ARG);
    assert(atom_slave_replace_reply(&slave,fresh,7)==ESP_OK && fifo.enabled);
    puts("Partial reply replacement and bounded critical-section tests passed");
}

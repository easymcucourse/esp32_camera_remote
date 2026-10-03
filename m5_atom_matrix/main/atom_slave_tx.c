#include "atom_slave_tx.h"
#include "esp_idf_version.h"
#include "i2c_private.h"
#include "hal/i2c_ll.h"
#include "freertos/ringbuf.h"

/* IDF 5.5.1's v2 i2c_slave_write() appends queued bytes from an earlier
 * response. The protocol requires replacement, including a partially read
 * response. Keep this narrow adapter version-pinned instead of modifying the
 * installed SDK. Review against the driver source when upgrading IDF. */
#if ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(5, 5, 1) || !CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2
#error "Review atom_slave_tx for this ESP-IDF version/slave driver"
#endif

esp_err_t atom_slave_replace_reply(i2c_slave_dev_handle_t slave, const uint8_t *bytes, size_t size)
{
    if (!slave || (size && !bytes) || size > 35) return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(slave->operation_mux, pdMS_TO_TICKS(5)) != pdTRUE) return ESP_ERR_TIMEOUT;
    /* This task and the slave ISR run on core 0. The critical section excludes
     * the ISR while draining the software ring and replacing the FIFO. All
     * ring operations use zero wait; no allocation or blocking occurs here. */
    portENTER_CRITICAL(&slave->base->spinlock);
    i2c_ll_slave_disable_tx_it(slave->base->hal.dev);
    size_t old_size;
    uint8_t *old;
    while ((old = xRingbufferReceive(slave->tx_ring_buf, &old_size, 0)) != NULL)
        vRingbufferReturnItem(slave->tx_ring_buf, old);
    i2c_ll_txfifo_rst(slave->base->hal.dev);
    size_t first = size < SOC_I2C_FIFO_LEN ? size : SOC_I2C_FIFO_LEN;
    if (first) i2c_ll_write_txfifo(slave->base->hal.dev, (uint8_t *)bytes, first);
    bool queued = size == first || xRingbufferSend(slave->tx_ring_buf, bytes + first, size - first, 0) == pdTRUE;
    if (queued) {
        if (size) i2c_ll_slave_enable_tx_it(slave->base->hal.dev);
        i2c_ll_slave_clear_stretch(slave->base->hal.dev);
    } else i2c_ll_txfifo_rst(slave->base->hal.dev);
    xSemaphoreGive(slave->operation_mux);
    portEXIT_CRITICAL(&slave->base->spinlock);
    return queued ? ESP_OK : ESP_ERR_NO_MEM;
}

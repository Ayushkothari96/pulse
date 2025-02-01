#include "accelerometer.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>

LOG_MODULE_REGISTER(accel, CONFIG_LOG_DEFAULT_LEVEL);

#define ACCEL_STACK_SIZE 1024
#define ACCEL_PRIORITY 5

static const struct device *i2c_dev;
static struct accel_config driver_config;
static struct accel_data buffer_storage[2];  // Static buffer allocation
static struct accel_data *active_buffer;      // Currently being filled
static struct accel_data *process_buffer;     // Being processed
static uint16_t buffer_index;
static atomic_t running;
K_MUTEX_DEFINE(buffer_mutex);  // Single declaration using Zephyr macro

K_THREAD_STACK_DEFINE(accel_stack, ACCEL_STACK_SIZE);
static struct k_thread accel_thread_data;

static void swap_buffers(void)
{
    k_mutex_lock(&buffer_mutex, K_FOREVER);
    struct accel_data *temp = active_buffer;
    active_buffer = process_buffer;
    process_buffer = temp;
    k_mutex_unlock(&buffer_mutex);
}

static int read_lis2dh12_data(struct accel_data *data)
{
    struct sensor_value accel[3];
    int ret;

    ret = sensor_sample_fetch(i2c_dev);
    if (ret < 0) {
        return ret;
    }

    ret = sensor_channel_get(i2c_dev, SENSOR_CHAN_ACCEL_XYZ, accel);
    if (ret < 0) {
        return ret;
    }

    data->x[buffer_index] = sensor_value_to_double(&accel[0]);
    data->y[buffer_index] = sensor_value_to_double(&accel[1]);
    data->z[buffer_index] = sensor_value_to_double(&accel[2]);

    return 0;
}

static void accel_thread(void *p1, void *p2, void *p3)
{
    while (1) {
        if (atomic_get(&running)) {
            k_mutex_lock(&buffer_mutex, K_FOREVER);
            if (0 == read_lis2dh12_data(active_buffer)) {
                buffer_index++;
            }

            if (buffer_index >= ACCEL_BUFFER_SIZE) {
                active_buffer->samples = buffer_index;
                swap_buffers();
                buffer_index = 0;
                k_mutex_unlock(&buffer_mutex);
                driver_config.data_ready_cb(process_buffer);
            } else {
                k_mutex_unlock(&buffer_mutex);
            }
            k_msleep(1000 / driver_config.sample_rate_hz);
        } else {
            k_sleep(K_MSEC(100));
        }
    }
}

int accelerometer_init(const struct accel_config *config)
{
    if (!config || !config->data_ready_cb) {
        return -EINVAL;
    }

    i2c_dev = DEVICE_DT_GET(DT_ALIAS(accel0));
    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("Accelerometer device not ready");
        return -ENODEV;
    }

    driver_config = *config;
    
    // Initialize static buffers
    active_buffer = &buffer_storage[0];
    process_buffer = &buffer_storage[1];
    buffer_index = 0;
    atomic_set(&running, false);

    k_thread_create(&accel_thread_data, accel_stack,
                    K_THREAD_STACK_SIZEOF(accel_stack),
                    accel_thread,
                    NULL, NULL, NULL,
                    K_PRIO_PREEMPT(10), 0, K_NO_WAIT);

    LOG_INF("LIS2DH12 accelerometer initialized");
    return 0;
}

void accelerometer_start(void)
{
    atomic_set(&running, true);
}

void accelerometer_stop(void)
{
    atomic_set(&running, false);
}
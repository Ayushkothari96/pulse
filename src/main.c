#include <zephyr/kernel.h>
#include <zephyr/drivers/usb/usb_dc.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include "accelerometer.h"
#include "moving_average.h"
#include "NanoEdgeAI.h"


LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

#define ML_BUFFER_SIZE (256u)
#define LEARNING_ITERATIONS (40u)

#define LED_PORT "GPIOB"  // Make sure the label is correct
#define LED_PIN 4         // Ensure this pin exists on your board

#define BUZZER_WORKING_FREQ (2700u)
#define BUZZER_STOP_FREQ (0u)

enum ml_state {
    ML_STATE_INIT,
    ML_STATE_TRAINING,
    ML_STATE_INFERENCING,
    ML_STATE_IDLE
};

enum ml_events {
    EVT_DATA_READY = 0x1,
    EVT_TRAINING_COMPLETE = 0x2,
    EVT_ERROR = 0x4,
    EVT_STOP = 0x8
};

static struct accel_sample *ml_buffer;
static struct k_event ml_event;
static struct k_mutex ml_mutex;
static enum ml_state current_state = ML_STATE_INIT;
static struct moving_average similarity_ma;


void ml_process_data(struct accel_data *buffer, size_t size);
enum ml_state ml_get_state(void);
void ml_init(void);

static void state_machine_step(uint32_t events)
{
    static uint8_t trainigIteration = 0;
    uint8_t similarity = 0;
    enum neai_state error_code = NEAI_OK;

    switch (current_state) {
        case ML_STATE_INIT:
                LOG_INF("Entering training state");
                error_code = neai_anomalydetection_init();
                if (error_code != NEAI_OK) {
                    /* This happens if the library works into a not supported board. */
                    LOG_ERR("The library failed to initialize, error code: %d", error_code);
                    current_state = ML_STATE_IDLE;
                } else {
                    current_state = ML_STATE_TRAINING;
                    LOG_INF("Engine analyzer started in IDLE state");
                }
            break;

        case ML_STATE_TRAINING:
            if (events & EVT_DATA_READY) {
                k_mutex_lock(&ml_mutex, K_FOREVER);
                neai_anomalydetection_learn((float*)ml_buffer);
                trainigIteration++;
                LOG_INF("Training iteration %d", trainigIteration);
                if (trainigIteration >= LEARNING_ITERATIONS) {
                    current_state = ML_STATE_INFERENCING;
                    LOG_INF("Training complete, entering inferencing");
                }
                k_mutex_unlock(&ml_mutex);
            }
            break;

        case ML_STATE_INFERENCING:
            if (events & EVT_DATA_READY) {
                static uint32_t inference_count = 0;
                k_mutex_lock(&ml_mutex, K_FOREVER);
                error_code = neai_anomalydetection_detect((float*)ml_buffer, &similarity);
                if (error_code != NEAI_OK) {
                    LOG_ERR("Error in inferencing, error code: %d", error_code);
                } else {
                    float avg_similarity = moving_average_update(&similarity_ma, similarity);
                    // LOG_INF("Inference #%u, Similarity: %d, Moving Average: %.2f", 
                    //     ++inference_count, similarity, avg_similarity);

                    if (avg_similarity < 80.0f) {
                        LOG_INF("Anomaly detected, similarity: %.2f", avg_similarity);
                       // buzzer_set_frequency(BUZZER_WORKING_FREQ);
                    } else {
                        //buzzer_set_frequency(BUZZER_STOP_FREQ);
                    }
                }
                k_mutex_unlock(&ml_mutex);
            }
            if (events & EVT_ERROR) {
                current_state = ML_STATE_IDLE;
                LOG_ERR("Error in inferencing, entering idle state");
            }
            break;

        case ML_STATE_IDLE:
            if (events & EVT_DATA_READY) {
                LOG_DBG("Ignoring data in idle state");
            }
            break;
    }
}

static void data_ready_handler(const struct accel_data *buffer)
{
    static uint32_t callback_count = 0;
    LOG_INF("Data ready callback called #%u", ++callback_count);
    k_mutex_lock(&ml_mutex, K_FOREVER);
    ml_buffer = (struct accel_sample*)buffer;
    k_event_post(&ml_event, EVT_DATA_READY);
    k_mutex_unlock(&ml_mutex);
}

#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static void led_init(void)
{
    int ret;

    if (!gpio_is_ready_dt(&led)) {
        LOG_ERR("GPIO device not ready, check your devicetree");
        return;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        LOG_ERR("Error %d: failed to configure LED pin\n", ret);
        return;
    }
    gpio_pin_set_dt(&led, 1);

    LOG_INF("Init done");
}

void main(void)
{
    k_event_init(&ml_event);
    k_mutex_init(&ml_mutex);

    struct accel_config config = {
        .sample_rate_hz = 100,
        .data_ready_cb = data_ready_handler
    };
    int ret;
    
    led_init();

    if (buzzer_init() != 0) {
        LOG_ERR("Failed to initialize buzzer");
        return;
    }

    if (accelerometer_init(&config) != 0) {
        LOG_ERR("Failed to initialize accelerometer");
        return;
    }

    accelerometer_start();
    LOG_INF("Engine analyzer started in INIT state");

    moving_average_init(&similarity_ma);

    buzzer_set_frequency(BUZZER_WORKING_FREQ);
      k_sleep(K_MSEC(750));
    buzzer_set_frequency(BUZZER_STOP_FREQ);
    k_sleep(K_MSEC(250));
    buzzer_set_frequency(BUZZER_WORKING_FREQ);
    k_sleep(K_MSEC(750));
    buzzer_set_frequency(BUZZER_STOP_FREQ);
    k_sleep(K_MSEC(250));
    buzzer_set_frequency(BUZZER_WORKING_FREQ);
    k_sleep(K_MSEC(750));
    buzzer_set_frequency(BUZZER_STOP_FREQ);
    k_sleep(K_MSEC(250));

    while (1) {
        uint32_t events = k_event_wait(&ml_event, 
            EVT_DATA_READY | EVT_TRAINING_COMPLETE | EVT_ERROR | EVT_STOP,
            false, K_FOREVER);
        // Clear the events before processing
        k_event_set_masked(&ml_event, 0, events);
        state_machine_step(events);
    }
}
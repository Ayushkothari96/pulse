#include <zephyr/kernel.h>
#include <zephyr/drivers/usb/usb_dc.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/debug/thread_analyzer.h>
#include <zephyr/fatal.h>
#include <zephyr/sys/printk.h>
#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include "accelerometer.h"
#include "NanoEdgeAI.h"


LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

// Early boot marker - if this doesn't print, panic is during C static init
static int __attribute__((constructor)) early_boot_test(void)
{
    // This runs BEFORE main() during C++ static initialization
    // If we don't see this, panic is even earlier
    return 0;
}

#define ML_BUFFER_SIZE (256u)
#define LEARNING_ITERATIONS (40u)

#define LED_PORT "GPIOB"  // Make sure the label is correct
#define LED_PIN 4         // Ensure this pin exists on your board


enum ml_state {
    ML_STATE_IDLE,
    ML_STATE_TRAINING,
    ML_STATE_INFERENCING
};

enum ml_events {
    EVT_DATA_READY = 0x1,
    EVT_TRAINING_COMPLETE = 0x2,
    EVT_ERROR = 0x4,
    EVT_STOP = 0x8
};



// Move ml_buffer to static memory to avoid stack overflow (256 floats = 1024 bytes)
static float ml_buffer[DATA_INPUT_USER*AXIS_NUMBER] __attribute__((aligned(4)));
static struct k_event ml_event;
static struct k_mutex ml_mutex;
static enum ml_state current_state = ML_STATE_IDLE;
static const struct accel_data *pending_buffer = NULL; // Pointer to accelerometer buffer

// Statistics for shell access
static uint8_t training_iteration = 0;
static uint8_t last_similarity = 0;
static bool similarity_valid = false;
static uint16_t accel_sample_rate = 500;

// Deferred work for ML reinitialization
static struct k_work ml_reinit_work;
static volatile bool ml_reinit_success = false;
static volatile bool ml_reinit_done = false;

void ml_process_data(struct accel_data *buffer, size_t size);
enum ml_state ml_get_state(void);
void ml_init(void);

/**
 * @brief Deferred work handler to reinitialize NanoEdge AI library
 * 
 * This runs in the system work queue context, not in the shell thread,
 * so it won't block the USB interface.
 */
static void ml_reinit_work_handler(struct k_work *work)
{
    LOG_INF("Starting NanoEdge AI reinitialization...");
    
    enum neai_state error_code = neai_anomalydetection_init();
    
    if (error_code == NEAI_OK) {
        k_mutex_lock(&ml_mutex, K_FOREVER);
        training_iteration = 0;
        similarity_valid = false;
        current_state = ML_STATE_IDLE;
        k_mutex_unlock(&ml_mutex);
        
        ml_reinit_success = true;
        LOG_INF("NanoEdge AI reinitialized successfully");
    } else {
        ml_reinit_success = false;
        LOG_ERR("NanoEdge AI reinitialization failed: %d", error_code);
    }
    
    ml_reinit_done = true;
}

/**
 * @brief Copy and interleave accelerometer data into ML buffer
 * 
 * Takes data from the accelerometer buffer (separate X, Y, Z arrays) and
 * interleaves it into the ML buffer in the format expected by NanoEdge AI:
 * x1,y1,z1, x2,y2,z2, ..., x256,y256,z256
 * 
 * @note This function does NOT lock the mutex. Caller must ensure thread safety.
 * @return true if data was copied successfully, false if buffer is NULL
 */
static bool copy_accel_to_ml_buffer(void)
{
    // Capture pointer locally to avoid race condition with callback
    const struct accel_data *local_buffer = pending_buffer;
    
    if (local_buffer == NULL) {
        return false;
    }
    
    // Interleave the axes: for each sample, write x, y, z
    for (uint16_t i = 0; i < DATA_INPUT_USER; i++) {
        ml_buffer[i * 3 + 0] = local_buffer->x[i];
        ml_buffer[i * 3 + 1] = local_buffer->y[i];
        ml_buffer[i * 3 + 2] = local_buffer->z[i];
    }
    
    return true;
}

static void state_machine_step(uint32_t events)
{
    uint8_t similarity = 0;
    enum neai_state error_code = NEAI_OK;

    switch (current_state) {
        case ML_STATE_TRAINING:
            if (events & EVT_DATA_READY) {
                // Lock mutex for entire critical section (copy + ML processing)
                k_mutex_lock(&ml_mutex, K_FOREVER);
                
                // Copy data from accelerometer buffer to ML buffer
                if (!copy_accel_to_ml_buffer()) {
                    LOG_ERR("Failed to copy accelerometer data (NULL buffer)");
                    k_mutex_unlock(&ml_mutex);
                    break;
                }
                
                if (training_iteration < MINIMUM_ITERATION_CALLS_FOR_EFFICIENT_LEARNING) {
                    error_code = neai_anomalydetection_learn(ml_buffer);
                    
                    if (error_code == NEAI_MINIMAL_RECOMMENDED_LEARNING_DONE) {
                        // Training complete early - model already trained
                        LOG_WRN("Model already trained! Power cycle to retrain from scratch.");
                        current_state = ML_STATE_INFERENCING;
                        LOG_INF("Training complete (AI satisfied), entering inferencing");
                    } else if (error_code == NEAI_NOT_ENOUGH_CALL_TO_LEARNING) {
                        // Continue training
                        training_iteration++;
                        LOG_INF("Training iteration %d", training_iteration);
                    } else {
                        // Error occurred
                        LOG_ERR("Training error: %d", error_code);
                        current_state = ML_STATE_IDLE;
                    }
                } else {
                    current_state = ML_STATE_INFERENCING;
                    LOG_INF("Training complete (max iterations), entering inferencing");
                }
                
                k_mutex_unlock(&ml_mutex);
            }
            break;

        case ML_STATE_INFERENCING:
            if (events & EVT_DATA_READY) {
                // Lock mutex for entire critical section (copy + ML processing)
                k_mutex_lock(&ml_mutex, K_FOREVER);
                
                // Copy data from accelerometer buffer to ML buffer
                if (!copy_accel_to_ml_buffer()) {
                    LOG_ERR("Failed to copy accelerometer data (NULL buffer)");
                    k_mutex_unlock(&ml_mutex);
                    break;
                }
                
                error_code = neai_anomalydetection_detect(ml_buffer, &similarity);
                if (error_code != NEAI_OK) {
                    LOG_ERR("Error in inferencing, error code: %d", error_code);
                } else {
                    last_similarity = similarity;
                    similarity_valid = true;
                    LOG_INF("Similarity: %d%%", similarity);
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
    // Fast, non-blocking callback - just signal that data is ready
    // The buffer pointer remains valid until next callback (double buffering in accelerometer.c)
    
    if (buffer == NULL) {
        LOG_ERR("Invalid buffer: NULL pointer");
        k_event_post(&ml_event, EVT_ERROR);
        return;
    }
    
    if (buffer->samples != ACCEL_BUFFER_SIZE) {
        LOG_ERR("Invalid buffer samples: %u (expected %u)", buffer->samples, ACCEL_BUFFER_SIZE);
        k_event_post(&ml_event, EVT_ERROR);
        return;
    }
    
    // Store pointer and signal main thread - no mutex, no copy, just atomic pointer write
    pending_buffer = buffer;
    
    // Signal event - main thread will do the heavy lifting
    k_event_post(&ml_event, EVT_DATA_READY);
}

// Custom fault handler to capture detailed fault information
void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf)
{
    // Use printk for early boot panics (before logging is ready)
    printk("\n\n======================================\n");
    printk("FATAL ERROR OCCURRED!\n");
    printk("======================================\n");
    printk("Reason code: %u\n", reason);
    
    LOG_ERR("======================================");
    LOG_ERR("FATAL ERROR OCCURRED!");
    LOG_ERR("======================================");
    
    // Decode the reason
    LOG_ERR("Reason code: %u", reason);
    switch (reason) {
        case K_ERR_KERNEL_OOPS:
            LOG_ERR("Reason: K_ERR_KERNEL_OOPS");
            break;
        case K_ERR_KERNEL_PANIC:
            LOG_ERR("Reason: K_ERR_KERNEL_PANIC");
            break;
        case K_ERR_STACK_CHK_FAIL:
            LOG_ERR("Reason: STACK CHECK FAIL - Stack overflow detected!");
            break;
        case K_ERR_CPU_EXCEPTION:
            LOG_ERR("Reason: K_ERR_CPU_EXCEPTION - Hardware fault!");
            break;
        default:
            LOG_ERR("Reason: Unknown (%u)", reason);
            break;
    }
    
    // Print thread information
    struct k_thread *current = k_current_get();
    if (current) {
        LOG_ERR("Current thread: %p", current);
        #ifdef CONFIG_THREAD_NAME
        const char *name = k_thread_name_get(current);
        if (name) {
            LOG_ERR("Thread name: %s", name);
        } else {
            LOG_ERR("Thread name: (unnamed)");
        }
        #endif
    }
    
    LOG_ERR("======================================");
    
    // Flush logs to ensure they're sent over USB
    k_msleep(200);
    
    // Call default handler
    CODE_UNREACHABLE;
}

void main(void)
{
    /* Ensure main thread has FP context */
    arch_float_enable(true);
    enum neai_state error_code;
    

    k_event_init(&ml_event);
    k_mutex_init(&ml_mutex);
    
    // Initialize deferred work for ML reinitialization
    k_work_init(&ml_reinit_work, ml_reinit_work_handler);
   
    k_msleep(100);

    // Wait for USB and shell to initialize
    LOG_INF("Waiting for system initialization...");
    k_msleep(2000);

    //Initialize NanoEdge AI first, before starting accelerometer
    error_code = neai_anomalydetection_init();
        
    if (error_code != NEAI_OK) {
        LOG_ERR("NanoEdge AI initialization failed, error code: %d", error_code);
        LOG_ERR("System will continue in IDLE state without ML functionality");
        current_state = ML_STATE_IDLE;
    } else {
        LOG_INF("NanoEdge AI initialized successfully");
        current_state = ML_STATE_TRAINING;
    }

    LOG_INF("Current state: %d", current_state);

    // Initialize accelerometer
    LOG_INF("Initializing accelerometer...");
    struct accel_config config = {
        .sample_rate_hz = 500,
        .data_ready_cb = data_ready_handler
    };

    if (accelerometer_init(&config) != 0) {
        LOG_ERR("Failed to initialize accelerometer");
        LOG_ERR("System will continue without accelerometer data");
        // Don't return - allow shell to work for debugging
        current_state = ML_STATE_IDLE;
    } else {
        accelerometer_start();
        accel_sample_rate = config.sample_rate_hz;
        LOG_INF("Accelerometer started successfully");
    }

    LOG_INF("Pulse device ready, current_state: %d", current_state);
    LOG_INF("Type 'help' for available commands");

    while (1) {
        uint32_t events = k_event_wait(&ml_event, 
            EVT_DATA_READY | EVT_TRAINING_COMPLETE | EVT_ERROR | EVT_STOP,
            true, K_FOREVER);
        
        state_machine_step(events);
    }
}

/* ============================================================================
 * Shell Command Handlers
 * ============================================================================ */

static const char *ml_state_to_string(enum ml_state state)
{
    switch (state) {
        case ML_STATE_IDLE: return "idle";
        case ML_STATE_TRAINING: return "training";
        case ML_STATE_INFERENCING: return "inferencing";
        default: return "unknown";
    }
}

/* ml state - Get current ML state */
static int cmd_ml_state(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "ML State: %s (%d)", 
                ml_state_to_string(current_state), current_state);
    
    if (current_state == ML_STATE_TRAINING) {
        shell_print(sh, "Training iteration: %d/%d",
                    training_iteration, MINIMUM_ITERATION_CALLS_FOR_EFFICIENT_LEARNING);
    } else if (current_state == ML_STATE_INFERENCING && similarity_valid) {
        shell_print(sh, "Last similarity: %d%%", last_similarity);
    }
    
    return 0;
}

/* ml set - Set ML state */
static int cmd_ml_set(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(sh, "Usage: ml set <state>");
        shell_print(sh, "States: idle(0), training(1), inferencing(2)");
        return -EINVAL;
    }
    
    int new_state = atoi(argv[1]);
    
    if (new_state < ML_STATE_IDLE || new_state > ML_STATE_INFERENCING) {
        shell_error(sh, "Invalid state. Use: 0(idle), 1(training), 2(inferencing)");
        return -EINVAL;
    }
    
    k_mutex_lock(&ml_mutex, K_FOREVER);
    current_state = (enum ml_state)new_state;
    
    // Reset training iteration when entering training mode
    if (new_state == ML_STATE_TRAINING) {
        training_iteration = 0;
        similarity_valid = false;  // Clear old similarity data
        shell_print(sh, "Note: If model is already trained, use 'ml reset' first");
        shell_print(sh, "      to reinitialize for fresh training.");
    }
    
    k_mutex_unlock(&ml_mutex);
    
    shell_print(sh, "ML state set to: %s", ml_state_to_string(current_state));
    return 0;
}

/* ml info - Get detailed ML information */
static int cmd_ml_info(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "=== ML Information ===");
    shell_print(sh, "Current state:      %s", ml_state_to_string(current_state));
    shell_print(sh, "Training iteration: %d/%d", 
                training_iteration, MINIMUM_ITERATION_CALLS_FOR_EFFICIENT_LEARNING);
    
    if (similarity_valid) {
        shell_print(sh, "Last similarity:    %d%%", last_similarity);
        if (last_similarity >= 90) {
            shell_print(sh, "Status:             NORMAL");
        } else if (last_similarity >= 70) {
            shell_print(sh, "Status:             WARNING");
        } else {
            shell_print(sh, "Status:             ANOMALY!");
        }
    } else {
        shell_print(sh, "Last similarity:    (no data)");
    }
    
    return 0;
}

/* ml reset - Reinitialize NanoEdge AI library using deferred work */
static int cmd_ml_reset(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "Submitting NanoEdge AI reinitialization request...");
    shell_print(sh, "This will run in background to avoid blocking USB.");
    
    // Reset flags
    ml_reinit_done = false;
    ml_reinit_success = false;
    
    // Submit work to system work queue
    k_work_submit(&ml_reinit_work);
    
    // Wait for completion (with timeout)
    int timeout_ms = 5000;
    int elapsed_ms = 0;
    
    while (!ml_reinit_done && elapsed_ms < timeout_ms) {
        k_msleep(100);
        elapsed_ms += 100;
    }
    
    if (!ml_reinit_done) {
        shell_error(sh, "Timeout waiting for reinitialization!");
        return -ETIMEDOUT;
    }
    
    if (ml_reinit_success) {
        shell_print(sh, "NanoEdge AI reinitialized successfully!");
        shell_print(sh, "ML state reset to: IDLE");
        shell_print(sh, "Use 'ml set 1' to start fresh training.");
        return 0;
    } else {
        shell_error(sh, "Reinitialization failed - check logs above");
        return -1;
    }
}

/* device info - Get device information */
static int cmd_device_info(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "=== Device Information ===");
    shell_print(sh, "Name:         Pulse");
    shell_print(sh, "Manufacturer: Kothari");
    shell_print(sh, "Version:      1.0.0");
    shell_print(sh, "Board:        nucleo_l412rb_p");
    shell_print(sh, "Uptime:       %lld ms", k_uptime_get());
    
    return 0;
}

/* accel info - Get accelerometer information */
static int cmd_accel_info(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "=== Accelerometer Information ===");
    shell_print(sh, "Sample rate:  %d Hz", accel_sample_rate);
    shell_print(sh, "Buffer size:  %d samples", ACCEL_BUFFER_SIZE);
    shell_print(sh, "Status:       running");
    
    return 0;
}

/* accel test - Test accelerometer reading */
static int cmd_accel_test(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "Testing accelerometer initialization...");
    
    struct accel_config config = {
        .sample_rate_hz = 500,
        .data_ready_cb = data_ready_handler
    };

    if (accelerometer_init(&config) != 0) {
        shell_error(sh, "Accelerometer initialization failed!");
        shell_print(sh, "Check logs above for detailed error");
        return -1;
    }
    
    accelerometer_start();
    accel_sample_rate = config.sample_rate_hz;
    shell_print(sh, "Accelerometer started successfully");
    
    return 0;
}

/* Register shell commands */
SHELL_STATIC_SUBCMD_SET_CREATE(ml_cmds,
    SHELL_CMD(state, NULL, "Get current ML state", cmd_ml_state),
    SHELL_CMD(set, NULL, "Set ML state (0=idle, 1=training, 2=inferencing)", cmd_ml_set),
    SHELL_CMD(info, NULL, "Get detailed ML information", cmd_ml_info),
    SHELL_CMD(reset, NULL, "Reinitialize NanoEdge AI (for retraining)", cmd_ml_reset),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(accel_cmds,
    SHELL_CMD(info, NULL, "Get accelerometer information", cmd_accel_info),
    SHELL_CMD(test, NULL, "Test/restart accelerometer", cmd_accel_test),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(ml, &ml_cmds, "Machine Learning commands", NULL);
SHELL_CMD_REGISTER(device, NULL, "Get device information", cmd_device_info);
SHELL_CMD_REGISTER(accel, &accel_cmds, "Accelerometer commands", NULL);

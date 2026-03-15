/*
MIT License

Copyright (c) 2026 Ayush Kothari

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <zephyr/kernel.h>
#include <zephyr/drivers/usb/usb_dc.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/debug/thread_analyzer.h>
#include <zephyr/fatal.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/sys/crc.h>
#include <string.h>
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

/* Similarity thresholds for anomaly detection */
#define SIMILARITY_NORMAL_THRESHOLD  90  /* >= 90% = Normal operation */
#define SIMILARITY_WARNING_THRESHOLD 70  /* 70-89% = Warning */
                                         /* < 70% = Anomaly */

#define LED_PORT "GPIOB"  // Make sure the label is correct
#define LED_PIN 4         // Ensure this pin exists on your board

/* ============================================================================
 * Simple ML State Machine
 * ============================================================================ */

enum ml_state {
    ML_STATE_IDLE,
    ML_STATE_TRAINING,
    ML_STATE_INFERENCING,
    ML_STATE_ERROR
};

/* Event flags for state machine */
#define EVT_DATA_READY    BIT(0)
#define EVT_ERROR         BIT(1)

/* Future: Enhanced State Machine with Persistent Training Data
 * Uncomment when implementing full flash-based training data management
 */
#if 0
enum ml_device_state {
    STATE_INIT,                    // Initial boot, checking flash
    STATE_LOAD_TRAINING_DATA,      // Loading stored training data from flash
    STATE_TRAIN_FROM_STORED_DATA,  // Training model with stored data
    STATE_COLLECT_FRESH_DATA,      // Collecting fresh accelerometer data
    STATE_TRAIN_FROM_FRESH_DATA,   // Training with fresh data
    STATE_SAVE_TRAINING_DATA,      // Saving training data to flash
    STATE_INFERENCING,             // Normal operation - detecting anomalies
    STATE_ERROR                    // Error state
};

enum device_events {
    EVT_DATA_READY = BIT(0),           // Accelerometer data available
    EVT_TRAINING_COMPLETE = BIT(1),    // ML training finished
    EVT_FLASH_VALID = BIT(2),          // Valid training data in flash
    EVT_FLASH_INVALID = BIT(3),        // No valid data in flash
    EVT_SAVE_COMPLETE = BIT(4),        // Data saved to flash
    EVT_ERROR = BIT(5),                // Error occurred
    EVT_USER_RESET = BIT(6)            // User requested reset via USB
};
#endif

/**
 * Training data storage structure in flash
 * Approach: Store samples in flash, load one at a time to RAM during training
 */
#define TRAINING_DATA_MAGIC 0x4E454149  // "NEAI" in hex
#define MAX_TRAINING_SAMPLES 10         // Store up to 10 training samples
#define TRAINING_DATA_VERSION 1

struct training_sample {
    float data[DATA_INPUT_USER * AXIS_NUMBER];  // One sample: 256 * 3 floats
    uint32_t timestamp;                          // When collected
};

struct training_data_header {
    uint32_t magic;              // Magic number for validation
    uint32_t version;            // Data structure version
    uint32_t num_samples;        // Number of valid samples
    uint32_t crc32;              // CRC32 of header + sample count only
    uint32_t reserved[4];        // Reserved for future use
};

/* Only keep header and current sample in RAM - samples stored individually in NVS */
struct persistent_training_data {
    struct training_data_header header;
    /* No samples array - samples stored separately in NVS to save RAM */
};

/* ============================================================================
 * Global Variables
 * ============================================================================ */

// Move ml_buffer to static memory to avoid stack overflow (256 floats = 1024 bytes)
static float ml_buffer[DATA_INPUT_USER*AXIS_NUMBER] __attribute__((aligned(4)));
static struct k_event device_event;
static struct k_mutex ml_mutex;
static enum ml_state current_state = ML_STATE_IDLE;
static const struct accel_data *pending_buffer = NULL; // Pointer to accelerometer buffer

// Training data collection - only header in RAM, samples in flash
static struct persistent_training_data training_data __attribute__((aligned(4)));
static struct training_sample current_sample __attribute__((aligned(4)));  // Single sample buffer
static uint32_t current_sample_index = 0;
static bool collecting_training_data = false;

// Statistics for shell access
static uint8_t training_iteration = 0;
static uint8_t last_similarity = 0;
static bool similarity_valid = false;
static uint16_t accel_sample_rate = 500;
static bool verbose_logs_enabled = true;  // Control verbose logging

// NVS storage
static struct nvs_fs nvs;
static bool nvs_initialized = false;  // Track NVS initialization status
#define NVS_PARTITION_DEVICE FIXED_PARTITION_DEVICE(storage_partition)
#define NVS_PARTITION_OFFSET FIXED_PARTITION_OFFSET(storage_partition)
#define NVS_TRAINING_DATA_ID 1

// Deferred work for ML reinitialization
static struct k_work ml_reinit_work;
static volatile bool ml_reinit_success = false;
static volatile bool ml_reinit_done = false;

// USB Console thread
#define CONSOLE_STACK_SIZE 512
K_THREAD_STACK_DEFINE(console_stack, CONSOLE_STACK_SIZE);
static struct k_thread console_thread;

// Forward declarations
void ml_process_data(struct accel_data *buffer, size_t size);
enum ml_state ml_get_state(void);
void ml_init(void);
static void usb_console_thread(void *p1, void *p2, void *p3);

/* ============================================================================
 * NVS Flash Storage Functions
 * ============================================================================ */

/**
 * @brief Initialize the NVS file system
 * @return 0 on success, negative errno on failure
 */
static int nvs_storage_init(void)
{
    int rc = 0;
    struct flash_pages_info info;
    
    nvs.flash_device = NVS_PARTITION_DEVICE;
    if (!device_is_ready(nvs.flash_device)) {
        LOG_ERR("Flash device %s is not ready", nvs.flash_device->name);
        return -ENODEV;
    }
    
    nvs.offset = NVS_PARTITION_OFFSET;
    
    /* Get flash page info to determine sector size */
    rc = flash_get_page_info_by_offs(nvs.flash_device, nvs.offset, &info);
    if (rc) {
        LOG_ERR("Unable to get page info: %d", rc);
        return rc;
    }
    
    /* STM32L412 flash: Use detected sector size or default to 2KB */
    nvs.sector_size = info.size;  /* Use detected sector size (typically 2KB for STM32L4) */
    nvs.sector_count = 24;        /* 24 sectors = 48KB total partition (for 10 samples + overhead) */
    
    LOG_INF("NVS config: offset=0x%x, sector_size=%d, sector_count=%d, partition_size=%dKB",
            nvs.offset, nvs.sector_size, nvs.sector_count, 
            (nvs.sector_size * nvs.sector_count) / 1024);
    
    rc = nvs_mount(&nvs);
    if (rc) {
        LOG_ERR("Flash Init failed: %d", rc);
        nvs_initialized = false;
        return rc;
    }
    
    nvs_initialized = true;  // Mark NVS as ready
    LOG_INF("NVS initialized: offset=0x%x, sector_size=%d, sector_count=%d",
            nvs.offset, nvs.sector_size, nvs.sector_count);
    
    return 0;
}

/**
 * @brief Calculate CRC32 of training header
 * @param data Pointer to training data structure
 * @return CRC32 value
 * 
 * CRC covers: magic (4), version (4), num_samples (4) = 12 bytes
 * Does NOT cover: crc32 field itself, or reserved fields
 */
static uint32_t calculate_training_data_crc(struct persistent_training_data *data)
{
    /* CRC covers magic, version, and num_samples fields (12 bytes total) */
    return crc32_ieee((uint8_t *)&data->header.magic, 
                      offsetof(struct training_data_header, crc32));
}

/**
 * @brief Save a single training sample to NVS flash (chunked for large data)
 * @param sample_index Index of sample to save (0-9)
 * @param sample Pointer to sample data
 * @return 0 on success, negative errno on failure
 * 
 * Each sample is 3,076 bytes, split into 2 chunks to fit in NVS:
 * - Chunk 0: First 1,536 bytes (384 floats)
 * - Chunk 1: Remaining 1,540 bytes (384 floats + timestamp)
 */
static int save_training_sample(uint32_t sample_index, struct training_sample *sample)
{
    int rc;
    uint16_t nvs_id_base = NVS_TRAINING_DATA_ID + 1 + (sample_index * 2);  // 2 chunks per sample
    const size_t chunk_size = 1536;  // Half of sample data (384 floats)
    
    if (!nvs_initialized) {
        LOG_ERR("NVS not initialized");
        return -EACCES;
    }
    
    /* Write first chunk (first 1,536 bytes) */
    rc = nvs_write(&nvs, nvs_id_base, sample->data, chunk_size);
    if (rc < 0) {
        LOG_ERR("Failed to write sample %d chunk 0 to NVS: %d", sample_index, rc);
        return rc;
    }
    
    /* Write second chunk (remaining 1,540 bytes: 384 floats + timestamp) */
    rc = nvs_write(&nvs, nvs_id_base + 1, 
                   ((uint8_t *)sample->data) + chunk_size, 
                   sizeof(struct training_sample) - chunk_size);
    if (rc < 0) {
        LOG_ERR("Failed to write sample %d chunk 1 to NVS: %d", sample_index, rc);
        return rc;
    }
    
    LOG_DBG("Saved training sample %d to flash (NVS IDs=%d,%d)", 
            sample_index, nvs_id_base, nvs_id_base + 1);
    return 0;
}

/**
 * @brief Load a single training sample from NVS flash (chunked read)
 * @param sample_index Index of sample to load (0-9)
 * @param sample Pointer to buffer for sample data
 * @return 0 on success, negative errno on failure
 */
static int load_training_sample(uint32_t sample_index, struct training_sample *sample)
{
    int rc;
    uint16_t nvs_id_base = NVS_TRAINING_DATA_ID + 1 + (sample_index * 2);  // 2 chunks per sample
    const size_t chunk_size = 1536;  // First chunk size
    
    if (!nvs_initialized) {
        LOG_ERR("NVS not initialized");
        return -EACCES;
    }
    
    /* Read first chunk */
    rc = nvs_read(&nvs, nvs_id_base, sample->data, chunk_size);
    if (rc < 0) {
        LOG_ERR("Failed to read sample %d chunk 0 from NVS: %d", sample_index, rc);
        return rc;
    }
    
    /* Read second chunk */
    rc = nvs_read(&nvs, nvs_id_base + 1,
                  ((uint8_t *)sample->data) + chunk_size,
                  sizeof(struct training_sample) - chunk_size);
    if (rc < 0) {
        LOG_ERR("Failed to read sample %d chunk 1 from NVS: %d", sample_index, rc);
        return rc;
    }
    
    LOG_DBG("Loaded training sample %d from flash (NVS IDs=%d,%d)", 
            sample_index, nvs_id_base, nvs_id_base + 1);
    return 0;
}

/**
 * @brief Save training header to NVS flash
 * @return 0 on success, negative errno on failure
 */
static int save_training_header(void)
{
    int rc;
    
    if (!nvs_initialized) {
        LOG_ERR("NVS not initialized");
        return -EACCES;
    }
    
    /* Calculate CRC before saving */
    training_data.header.crc32 = calculate_training_data_crc(&training_data);
    
    /* Write header to NVS (ID=1) */
    rc = nvs_write(&nvs, NVS_TRAINING_DATA_ID, &training_data, sizeof(training_data));
    if (rc < 0) {
        LOG_ERR("Failed to write training header to NVS: %d", rc);
        return rc;
    }
    
    LOG_INF("Saved training header: %d samples (CRC=0x%08x)",
            training_data.header.num_samples,
            training_data.header.crc32);
    
    return 0;
}

/**
 * @brief Load training header from NVS flash and validate
 * @return 0 on success (data valid), negative errno on failure or invalid data
 */
static int load_training_header(void)
{
    int rc;
    uint32_t calculated_crc;
    
    if (!nvs_initialized) {
        LOG_ERR("NVS not initialized");
        return -EACCES;
    }
    
    /* Read header from NVS (ID=1) */
    rc = nvs_read(&nvs, NVS_TRAINING_DATA_ID, &training_data, sizeof(training_data));
    if (rc < 0) {
        if (rc == -ENOENT) {
            LOG_INF("No training data found in flash (first boot)");
        } else {
            LOG_ERR("Failed to read training header from NVS: %d", rc);
        }
        return rc;
    }
    
    /* Validate magic number */
    if (training_data.header.magic != TRAINING_DATA_MAGIC) {
        LOG_WRN("Invalid magic number in flash: 0x%08x (expected 0x%08x)",
                training_data.header.magic, TRAINING_DATA_MAGIC);
        return -EINVAL;
    }
    
    /* Validate version */
    if (training_data.header.version != TRAINING_DATA_VERSION) {
        LOG_WRN("Incompatible training data version: %d (expected %d)",
                training_data.header.version, TRAINING_DATA_VERSION);
        return -EINVAL;
    }
    
    /* Validate sample count */
    if (training_data.header.num_samples > MAX_TRAINING_SAMPLES) {
        LOG_WRN("Invalid sample count: %d (max %d)",
                training_data.header.num_samples, MAX_TRAINING_SAMPLES);
        return -EINVAL;
    }
    
    /* Validate CRC */
    calculated_crc = calculate_training_data_crc(&training_data);
    if (calculated_crc != training_data.header.crc32) {
        LOG_WRN("CRC mismatch: calculated=0x%08x, stored=0x%08x",
                calculated_crc, training_data.header.crc32);
        return -EINVAL;
    }
    
    LOG_INF("Loaded training header: %d samples (CRC=0x%08x)",
            training_data.header.num_samples,
            training_data.header.crc32);
    
    return 0;
}

/**
 * @brief Add current ML buffer to training data collection in flash
 * @note Must be called after ml_buffer is filled with valid data
 */
static void add_training_sample(void)
{
    if (current_sample_index >= MAX_TRAINING_SAMPLES) {
        LOG_WRN("Training data buffer full, cannot add more samples");
        return;
    }
    
    /* Copy ML buffer to current sample buffer */
    memcpy(current_sample.data, ml_buffer, sizeof(current_sample.data));
    current_sample.timestamp = k_uptime_get_32();
    
    /* Save sample to flash immediately */
    if (save_training_sample(current_sample_index, &current_sample) == 0) {
        current_sample_index++;
        training_data.header.num_samples = current_sample_index;
        LOG_DBG("Added training sample %d/%d", current_sample_index, MAX_TRAINING_SAMPLES);
    }
}

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
        current_state = ML_STATE_TRAINING;  // Automatically start training
        k_mutex_unlock(&ml_mutex);
        
        ml_reinit_success = true;
        LOG_INF("NanoEdge AI reinitialized successfully - training started");
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
                
                // Add sample to training data collection for flash storage
                if (current_sample_index < MAX_TRAINING_SAMPLES) {
                    add_training_sample();
                }
                
                if (training_iteration < MINIMUM_ITERATION_CALLS_FOR_EFFICIENT_LEARNING) {
                    error_code = neai_anomalydetection_learn(ml_buffer);
                    
                    if (error_code == NEAI_MINIMAL_RECOMMENDED_LEARNING_DONE) {
                        // Training complete early - model already trained
                        LOG_WRN("Model already trained! Use RESET command or power cycle to retrain from scratch.");
                        current_state = ML_STATE_INFERENCING;
                        LOG_INF("Training complete (AI satisfied), entering inferencing");
                        
                        // Save training header to flash (samples already saved individually)
                        if (save_training_header() == 0) {
                            LOG_INF("Training header saved to flash");
                        }
                    } else if (error_code == NEAI_NOT_ENOUGH_CALL_TO_LEARNING) {
                        // Continue training
                        training_iteration++;
                        if (verbose_logs_enabled) {
                            LOG_INF("Training iteration %d", training_iteration);
                        }
                    } else {
                        // Error occurred
                        LOG_ERR("Training error: %d", error_code);
                        current_state = ML_STATE_ERROR;
                    }
                } else {
                    current_state = ML_STATE_INFERENCING;
                    LOG_INF("Training complete (max iterations), entering inferencing");
                    
                    // Save training header to flash (samples already saved individually)
                    if (save_training_header() == 0) {
                        LOG_INF("Training header saved to flash");
                    }
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
                    if (verbose_logs_enabled) {
                        LOG_INF("Similarity: %d%%", similarity);
                    }
                }
                
                k_mutex_unlock(&ml_mutex);
            }
            if (events & EVT_ERROR) {
                current_state = ML_STATE_ERROR;
                LOG_ERR("Error in inferencing, entering error state");
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
        k_event_post(&device_event, EVT_ERROR);
        return;
    }
    
    if (buffer->samples != ACCEL_BUFFER_SIZE) {
        LOG_ERR("Invalid buffer samples: %u (expected %u)", buffer->samples, ACCEL_BUFFER_SIZE);
        k_event_post(&device_event, EVT_ERROR);
        return;
    }
    
    // Store pointer and signal main thread - no mutex, no copy, just atomic pointer write
    pending_buffer = buffer;
    
    // Signal event - main thread will do the heavy lifting
    k_event_post(&device_event, EVT_DATA_READY);
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
    

    k_event_init(&device_event);
    k_mutex_init(&ml_mutex);
    
    // Initialize deferred work for ML reinitialization
    k_work_init(&ml_reinit_work, ml_reinit_work_handler);
   
    k_msleep(100);

    // Wait for USB to initialize
    LOG_INF("Waiting for system initialization...");
    k_msleep(2000);

    // Initialize NVS storage for persistent training data
    memset(&training_data, 0, sizeof(training_data));
    training_data.header.magic = TRAINING_DATA_MAGIC;
    training_data.header.version = TRAINING_DATA_VERSION;
    training_data.header.num_samples = 0;
    
    int nvs_rc = nvs_storage_init();
    if (nvs_rc != 0) {
        LOG_ERR("NVS initialization failed: %d", nvs_rc);
        LOG_WRN("Flash storage disabled - training data will not persist");
    } else {
        // Try to load existing training header (samples stay in flash until needed)
        nvs_rc = load_training_header();
        if (nvs_rc == 0) {
            LOG_INF("Found %d training samples in flash", training_data.header.num_samples);
            current_sample_index = training_data.header.num_samples;
        } else {
            LOG_INF("No valid training data in flash, starting fresh");
            current_sample_index = 0;
        }
    }

    //Initialize NanoEdge AI first, before starting accelerometer
    error_code = neai_anomalydetection_init();
        
    if (error_code != NEAI_OK) {
        LOG_ERR("NanoEdge AI initialization failed, error code: %d", error_code);
        LOG_ERR("System will continue in ERROR state without ML functionality");
        current_state = ML_STATE_ERROR;
    } else {
        LOG_INF("NanoEdge AI initialized successfully");
        
        // If we have stored training samples, train the model with them
        if (training_data.header.num_samples > 0) {
            LOG_INF("Training ML model with %d stored samples from flash...", 
                    training_data.header.num_samples);
            
            for (uint32_t i = 0; i < training_data.header.num_samples; i++) {
                // Load sample from flash into current_sample buffer
                if (load_training_sample(i, &current_sample) == 0) {
                    // Copy to ml_buffer for training
                    memcpy(ml_buffer, current_sample.data, sizeof(ml_buffer));
                    
                    // Train with this sample
                    error_code = neai_anomalydetection_learn(ml_buffer);
                    
                    if (error_code == NEAI_MINIMAL_RECOMMENDED_LEARNING_DONE) {
                        LOG_INF("Training complete (AI satisfied) after %d stored samples", i + 1);
                        current_state = ML_STATE_INFERENCING;
                        training_iteration = i + 1;
                        break;
                    } else if (error_code == NEAI_NOT_ENOUGH_CALL_TO_LEARNING) {
                        training_iteration = i + 1;
                        LOG_INF("Trained with stored sample %d/%d", i + 1, training_data.header.num_samples);
                    } else {
                        LOG_ERR("Error training with stored sample %d: %d", i, error_code);
                        break;
                    }
                } else {
                    LOG_ERR("Failed to load training sample %d from flash", i);
                    break;
                }
            }
            
            // Check final state after training with stored samples
            if (current_state == ML_STATE_INFERENCING) {
                LOG_INF("ML model ready for inferencing (trained from flash)");
            } else if (training_iteration >= MINIMUM_ITERATION_CALLS_FOR_EFFICIENT_LEARNING) {
                current_state = ML_STATE_INFERENCING;
                LOG_INF("ML model ready for inferencing (%d iterations)", training_iteration);
            } else {
                current_state = ML_STATE_TRAINING;
                LOG_INF("ML model needs more training (%d/%d iterations)", 
                        training_iteration, MINIMUM_ITERATION_CALLS_FOR_EFFICIENT_LEARNING);
            }
        } else {
            // No stored samples, start fresh training
            current_state = ML_STATE_TRAINING;
            LOG_INF("No stored samples, starting fresh training");
        }
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
        current_state = ML_STATE_ERROR;
    } else {
        accelerometer_start();
        accel_sample_rate = config.sample_rate_hz;
        LOG_INF("Accelerometer started successfully");
    }

    LOG_INF("Pulse device ready, current_state: %d", current_state);
    
    // Start USB console command thread
    // Priority 10 (lower than accelerometer priority 10, runs when idle)
    k_tid_t console_tid = k_thread_create(&console_thread, console_stack,
                                          K_THREAD_STACK_SIZEOF(console_stack),
                                          usb_console_thread,
                                          NULL, NULL, NULL,
                                          K_PRIO_PREEMPT(12), 0, K_NO_WAIT);
    k_thread_name_set(console_tid, "usb_console");

    while (1) {
        uint32_t events = k_event_wait(&device_event, 
            EVT_DATA_READY | EVT_ERROR,
            true, K_FOREVER);
        
        state_machine_step(events);
    }
}

/* ============================================================================
 * Simple USB Command Handler (Minimal Memory Footprint)
 * ============================================================================ */

static const char *ml_state_to_string(enum ml_state state)
{
    switch (state) {
        case ML_STATE_IDLE: return "IDLE";
        case ML_STATE_TRAINING: return "TRAINING";
        case ML_STATE_INFERENCING: return "INFERENCING";
        case ML_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

/**
 * Command: STATUS
 * Returns all device information and current state
 */
static void cmd_status(void)
{
    printk("\n=== PULSE DEVICE STATUS ===\n");
    printk("Device:      Pulse v1.0.0\n");
    printk("Board:       nucleo_l412rb_p\n");
    printk("Uptime:      %lld ms\n", k_uptime_get());
    printk("\n");
    printk("ML State:    %s\n", ml_state_to_string(current_state));
    printk("Training:    %d/%d iterations\n", 
           training_iteration, MINIMUM_ITERATION_CALLS_FOR_EFFICIENT_LEARNING);
    
    if (similarity_valid) {
        printk("Similarity:  %d%%", last_similarity);
        if (last_similarity >= SIMILARITY_NORMAL_THRESHOLD) {
            printk(" (NORMAL)\n");
        } else if (last_similarity >= SIMILARITY_WARNING_THRESHOLD) {
            printk(" (WARNING)\n");
        } else {
            printk(" (ANOMALY!)\n");
        }
    } else {
        printk("Similarity:  N/A\n");
    }
    
    printk("\n");
    printk("Accel Rate:  %d Hz\n", accel_sample_rate);
    printk("Accel Buf:   %d samples\n", ACCEL_BUFFER_SIZE);
    printk("\n");
    printk("Flash Store: %d/%d samples stored\n", 
           training_data.header.num_samples, MAX_TRAINING_SAMPLES);
    if (training_data.header.num_samples > 0) {
        printk("Flash CRC:   0x%08X\n", training_data.header.crc32);
    }
    printk("===========================\n\n");
}

/**
 * Command: LOGS
 * Toggle verbose logging on/off
 */
static void cmd_logs(void)
{
    verbose_logs_enabled = !verbose_logs_enabled;
    
    if (verbose_logs_enabled) {
        printk("\nVerbose logs ENABLED\n\n");
    } else {
        printk("\nVerbose logs DISABLED\n\n");
    }
}

/**
 * Command: RESET
 * Reinitializes the ML model for fresh training and clears stored training data
 */
static void cmd_reset(void)
{
    printk("\nRESETTING: ML model + training data...\n");
    
    // Clear training data counters
    k_mutex_lock(&ml_mutex, K_FOREVER);
    current_sample_index = 0;
    training_data.header.num_samples = 0;
    training_data.header.crc32 = 0;
    k_mutex_unlock(&ml_mutex);
    
    printk("Cleared training data from RAM\n");
    
    // Clear training data from NVS flash
    // Note: We could delete individual samples, but easier to just reset header
    // Samples will be overwritten during next training session
    training_data.header.magic = TRAINING_DATA_MAGIC;
    training_data.header.version = TRAINING_DATA_VERSION;
    training_data.header.num_samples = 0;
    
    int nvs_rc = save_training_header();
    if (nvs_rc == 0) {
        printk("Cleared training data from flash\n");
    } else {
        printk("WARNING: Failed to clear flash data (error %d)\n", nvs_rc);
    }
    
    // Reset ML model
    printk("Reinitializing ML model...\n");
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
        printk("ERROR: Timeout waiting for reinitialization!\n\n");
        return;
    }
    
    if (ml_reinit_success) {
        printk("SUCCESS: ML model reinitialized\n");
        printk("Training started automatically\n");
        printk("New training data will be collected and saved to flash\n");
        printk("Monitor progress with STATUS command\n\n");
    } else {
        printk("ERROR: Reinitialization failed\n\n");
    }
}

/**
 * USB Console Command Parser Thread
 * Handles simple text commands from USB console
 */
static void usb_console_thread(void *p1, void *p2, void *p3)
{
    const struct device *uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    char cmd_buf[32];
    int idx = 0;
    uint8_t c;
    
    if (!device_is_ready(uart_dev)) {
        printk("Console device not ready!\n");
        return;
    }
    
    // Wait a bit for USB to enumerate
    k_msleep(1000);
    
    printk("\n\n");
    printk("=======================================\n");
    printk("  PULSE DEVICE - USB CONSOLE\n");
    printk("=======================================\n");
    printk("Commands:\n");
    printk("  STATUS - Show device status\n");
    printk("  RESET  - Reset ML model for retraining\n");
    printk("  LOGS   - Toggle verbose logging\n");
    printk("=======================================\n\n");
    
    while (1) {
        // Poll for character
        if (uart_poll_in(uart_dev, &c) == 0) {
            // Handle backspace
            if (c == '\b' || c == 0x7F) {
                if (idx > 0) {
                    idx--;
                    printk("\b \b");  // Erase character on screen
                }
                continue;
            }
            
            // Echo character
            if (c >= 32 && c < 127) {
                printk("%c", c);
                if (idx < sizeof(cmd_buf) - 1) {
                    cmd_buf[idx++] = c;
                }
            }
            
            // Handle enter
            if (c == '\r' || c == '\n') {
                printk("\n");
                cmd_buf[idx] = '\0';
                
                // Skip empty commands
                if (idx == 0) {
                    continue;
                }
                
                // Convert to uppercase for case-insensitive matching
                for (int i = 0; i < idx; i++) {
                    if (cmd_buf[i] >= 'a' && cmd_buf[i] <= 'z') {
                        cmd_buf[i] -= 32;
                    }
                }
                
                // Parse and execute command
                if (strcmp(cmd_buf, "STATUS") == 0) {
                    cmd_status();
                } else if (strcmp(cmd_buf, "RESET") == 0) {
                    cmd_reset();
                } else if (strcmp(cmd_buf, "LOGS") == 0) {
                    cmd_logs();
                } else {
                    printk("Unknown command: %s\n", cmd_buf);
                    printk("Type STATUS, RESET, or LOGS\n\n");
                }
                
                // Reset for next command
                idx = 0;
            }
        } else {
            // No data, sleep briefly to avoid busy loop
            k_msleep(10);
        }
    }
}

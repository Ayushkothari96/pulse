# Flash Storage Implementation for Persistent Training Data

## Overview

The Pulse device stores training data samples in non-volatile flash memory to prevent loss of training progress during power cycles or resets. This ensures that the ML model can quickly retrain using previously collected "healthy" data instead of potentially training on faulty data if the machine fails overnight.

## Boot Behavior

When the device powers on, it follows this sequence:

1. **Initialize NVS**: Mount flash storage and check for existing data
2. **Load Training Header**: Read metadata (magic, version, sample count, CRC)
3. **Validate Data**: Check magic number, version, and CRC32 checksum
4. **If Valid Data Found** (e.g., 10 samples stored):
   - Load each sample one-by-one from flash into RAM
   - Train ML model with loaded sample using `neai_anomalydetection_learn()`
   - Repeat for all stored samples
   - If training completes (≥40 iterations or AI satisfied), enter INFERENCING mode
   - If more training needed, continue collecting fresh samples from accelerometer
5. **If No Data Found** (fresh boot or after RESET):
   - Start fresh training from scratch
   - Collect new samples from accelerometer
   - Save each sample to flash as collected during training

This approach ensures:
- ✅ **No data loss**: Stored samples survive power cycles
- ✅ **Fast recovery**: Model trains immediately on boot from stored data
- ✅ **Continuous improvement**: Can collect more samples if needed
- ✅ **RAM efficient**: Only loads one sample at a time (3KB RAM vs 31KB for all samples)

## Features

1. **Persistent Storage**: Training samples are saved to flash during training
2. **Data Validation**: CRC32 checksum ensures data integrity
3. **Auto-Recovery**: On boot, device automatically loads and retrains from stored data
4. **Storage Capacity**: Stores up to 10 training samples (each ~3KB) in 48KB flash partition
5. **USB Status**: `STATUS` command shows number of samples stored and CRC32 value
6. **USB Reset**: `RESET` command clears flash data and restarts fresh training

**Note**: 48KB partition provides sufficient space for 10 samples (~31KB) plus NVS overhead (~17KB for wear leveling and metadata).

## Architecture

### Flash Partition Layout

```
Flash Memory (128KB total)
├─ Application Code (0x00000 - 0x13FFF): ~80KB
└─ Storage Partition (0x14000 - 0x1FFFF): 48KB
   └─ NVS File System
      └─ Training Data (ID=1)
```

### Data Structure

```c
struct training_data_header {
    uint32_t magic;         // 0x4E454149 ("NEAI")
    uint32_t version;       // Data format version (1)
    uint32_t num_samples;   // Number of valid samples (0-10)
    uint32_t crc32;         // CRC32 of header
    uint32_t reserved[4];   // Future expansion
};

struct training_sample {
    float data[768];        // 256 samples * 3 axes
    uint32_t timestamp;     // Collection time (ms)
};

struct persistent_training_data {
    struct training_data_header header;
    /* Samples NOT in struct - stored individually in NVS to save RAM */
};
```

### NVS Storage Layout (Chunked)

```
NVS Partition (48KB, 24 sectors × 2KB)
├─ NVS ID 1:   Header (64 bytes)
├─ NVS ID 2:   Sample[0] Chunk 0 (1,536 bytes) - First 384 floats
├─ NVS ID 3:   Sample[0] Chunk 1 (1,540 bytes) - Last 384 floats + timestamp
├─ NVS ID 4:   Sample[1] Chunk 0 (1,536 bytes)
├─ NVS ID 5:   Sample[1] Chunk 1 (1,540 bytes)
├─ ... (2 chunks per sample)
├─ NVS ID 20:  Sample[9] Chunk 0 (1,536 bytes)
├─ NVS ID 21:  Sample[9] Chunk 1 (1,540 bytes)
└─ [NVS Overhead: ~17KB for wear leveling, metadata, and free space]

Total: 1 header + 20 chunks (10 samples × 2 chunks) + overhead
```

**Key Optimizations**: 
- Samples split into 2 chunks (~1.5KB each) to fit NVS sector size (2KB)
- Only one sample buffer (~3KB) in RAM at any time
- Chunked write/read for efficient flash storage

### Storage Size

- **Header**: 64 bytes (NVS ID 1)
- **Single Sample**: 3,076 bytes (768 floats × 4 bytes + 4 bytes timestamp)
- **10 Samples**: ~30KB total in flash (NVS IDs 2-11)
- **RAM Usage**: Only 1 sample buffer (~3KB) + header (64 bytes) in RAM at a time
- **Total NVS**: 16KB (8 sectors × 2KB each)

## Boot Sequence

1. **Initialize NVS**: Mount NVS file system on storage partition
2. **Load Training Header**: Attempt to read stored header (NVS ID 1)
3. **Validate Header**: Check magic number, version, and CRC32
4. **Decision**:
   - If valid header exists → Report stored samples count in STATUS
   - If no/invalid header → Start with empty collection
   - **Samples remain in flash** until needed for training/retraining

## Training Flow

```
Training Iteration
    ↓
1. Copy accel data to ML buffer
    ↓
2. Copy ML buffer to current_sample buffer (RAM)
    ↓
3. Save current_sample to flash immediately (NVS ID 2-11)
    ↓
4. Call neai_anomalydetection_learn()
    ↓
5. If training complete:
    ↓
6. Save header to flash (NVS ID 1, with CRC32)
    ↓
7. Transition to INFERENCING state
```

**RAM Benefit**: Only one sample (~3KB) in RAM at any time, vs 10 samples (~31KB) if all kept in RAM.

## API Functions

### `nvs_storage_init()`
Initialize NVS file system and mount storage partition.

**Returns**: 0 on success, negative errno on failure

### `load_training_header()`
Load and validate training header from flash (NVS ID 1).

**Returns**: 
- 0 = Valid header loaded
- -ENOENT = No header in flash (first boot)
- -EINVAL = Invalid magic/version/CRC

### `save_training_header()`
Calculate CRC32 and save training header to flash (NVS ID 1).

**Returns**: 0 on success, negative errno on failure

### `load_training_sample(uint32_t index, struct training_sample *sample)`
Load a single training sample from flash into RAM buffer.

**Parameters**:
- index: Sample index (0-9)
- sample: Pointer to RAM buffer for sample data

**Returns**: 0 on success, negative errno on failure

### `save_training_sample(uint32_t index, struct training_sample *sample)`
Save a single training sample from RAM to flash.

**Parameters**:
- index: Sample index (0-9)
- sample: Pointer to sample data in RAM

**Returns**: 0 on success, negative errno on failure

### `add_training_sample()`
Copy current ML buffer to sample buffer and save to flash immediately.

**Note**: Called during each training iteration (up to 10 samples max). Sample saved to NVS ID (2 + sample_index).

## USB Commands

### STATUS Command Output

```
=== PULSE DEVICE STATUS ===
Device:      Pulse v1.0.0
Board:       nucleo_l412rb_p
Uptime:      12345 ms

ML State:    TRAINING
Training:    25/40 iterations
Similarity:  N/A

Accel Rate:  500 Hz
Accel Buf:   256 samples

Flash Store: 10/10 samples stored
Flash CRC:   0x12AB34CD
===========================
```

### RESET Command

Reinitializes the ML model. Training data in flash is **not erased** but will be overwritten during next training cycle.

## Memory Usage

- **Flash Code**: ~112KB (application + libraries)
- **Flash Storage**: 16KB (NVS partition for 10 samples)
- **RAM**: ~3.8KB (training_data header + current_sample buffer + ml_buffer)
  - training_data struct: ~64 bytes (header only, no sample array)
  - current_sample buffer: ~3,076 bytes (single sample)
  - ml_buffer: ~768 bytes
- **Stack**: 2KB main + 512 bytes console

**Key Optimization**: Samples stored individually in flash (NVS IDs 2-11), loaded one at a time to RAM during training. This reduces RAM usage from ~31KB to ~3.8KB.

## Configuration (prj.conf)

```ini
# Flash and NVS
CONFIG_FLASH=y
CONFIG_FLASH_MAP=y
CONFIG_NVS=y
CONFIG_MPU_ALLOW_FLASH_WRITE=y

# CRC for data validation
CONFIG_CRC=y
```

## Limitations

1. **Sample Count**: Only 10 samples stored (vs 40 training iterations)
   - Device collects samples from iterations 0-9
   - Remaining iterations (10-39) reuse ML model training (samples only for potential retraining)
   
2. **No Erase Command**: Must reflash device to clear stored data

3. **Single Storage Slot**: Only one training data set (no versioning/history)

4. **No Compression**: Raw float arrays stored as-is

5. **Sequential Write**: Samples written one at a time during training (10 NVS writes vs 1 bulk write)

## Future Enhancements

1. **Erase Command**: Add USB command to clear flash storage
2. **More Samples**: Optimize storage to fit 15-20 samples
3. **Compression**: Use quantization to reduce sample size (float32 → int16)
4. **Multiple Profiles**: Store training data for different machine states
5. **Flash Wear Leveling**: Track write cycles and warn on excessive writes
6. **Auto-Retrain**: Use stored samples to automatically train ML on boot
7. **External Flash**: Move to external SPI flash for more storage capacity

## Testing

1. **First Boot**: Device trains from scratch, saves samples to flash
2. **Power Cycle**: Device loads samples, STATUS shows stored count
3. **RESET**: Clears ML model but keeps flash data intact
4. **Train Again**: New samples overwrite old ones in flash

## Troubleshooting

### "NVS initialization failed"
- Check flash partition in device tree overlay
- Verify CONFIG_FLASH=y in prj.conf
- Ensure partition address doesn't overlap with code

### "CRC mismatch"
- Flash corruption detected
- Device will ignore stored data and train fresh
- Check power supply stability

### "No valid training data in flash"
- Normal on first boot
- Device will collect and save new data during training

### "Training data buffer full"
- Max 10 samples collected
- Additional iterations use existing 10 samples
- This is normal behavior

## References

- [Zephyr NVS Documentation](https://docs.zephyrproject.org/latest/services/storage/nvs/nvs.html)
- [STM32L412 Reference Manual](https://www.st.com/resource/en/reference_manual/rm0394-stm32l41xxx42xxx43xxx44xxx45xxx46xxx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- Device Tree Overlay: `boards/nucleo_l412rb_p.overlay`
- Main Code: `src/main.c` (lines 133-289: NVS functions)

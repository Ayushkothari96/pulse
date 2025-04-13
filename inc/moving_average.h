#ifndef MOVING_AVERAGE_H
#define MOVING_AVERAGE_H

#include <stdint.h>
#include <stdbool.h>

#define MA_WINDOW_SIZE 15u

struct moving_average {
    uint8_t buffer[MA_WINDOW_SIZE];
    uint32_t sum;
    uint8_t index;
    bool is_filled;
};

void moving_average_init(struct moving_average *ma);
float moving_average_update(struct moving_average *ma, uint8_t new_value);
float moving_average_get(const struct moving_average *ma);

#endif /* MOVING_AVERAGE_H */
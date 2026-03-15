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

#include "buzzer.h"
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(buzzer, CONFIG_LOG_DEFAULT_LEVEL);

#define BUZZER_NODE DT_ALIAS(buzzer0)

static const struct pwm_dt_spec buzzer = PWM_DT_SPEC_GET(BUZZER_NODE);
static uint32_t period_ns;

int buzzer_init(void)
{
    if (!device_is_ready(buzzer.dev)) {
        LOG_ERR("PWM device not ready");
        return -ENODEV;
    }

    LOG_INF("Buzzer initialized");
    return 0;
}

int buzzer_set_frequency(uint32_t freq_hz)
{
    if (freq_hz == 0) {
        buzzer_stop();
        return 0;
    }

    // Calculate period in nanoseconds
    period_ns = (uint32_t)(1000000000 / freq_hz);
    
    // Set 30% duty cycle
    uint32_t pulse_ns = (period_ns * 3) / 100;

    return pwm_set_dt(&buzzer, period_ns, pulse_ns);
}

void buzzer_stop(void)
{
    pwm_set_dt(&buzzer, 0, 0);
}
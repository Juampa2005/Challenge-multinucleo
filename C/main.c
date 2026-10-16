    for (uint8_t i = 0; i < buffer_count; i++) {
        sum += sample_buffer[i];
    }

    return (uint16_t)(sum / buffer_count);
}

static system_state_t classify(float percent) {
    if (percent < 50.0f) {
        return STATE_NORMAL;
    } else if (percent < 75.0f) {
        return STATE_WARNING;
    }
    return STATE_ALARM;
}

static void sensor_task(void) {
    raw_value = adc_read(); // ADC de 12 bits: 0..4095
    filtered_value = moving_average(raw_value);
    percent_value = ((float)filtered_value * 100.0f) / ADC_MAX_VALUE;

    system_state_t new_state = classify(percent_value);
    sample_count++;

    // Si deja de existir la alarma, borrar el reconocimiento (ACK).
    if (new_state != STATE_ALARM) {
        alarm_ack = false;
    }

    state = new_state;
}

static void ui_task(void) {
    gpio_xor_mask(1u << HEARTBEAT_GPIO);

    switch (state) {
        case STATE_NORMAL:
            gpio_put(LED_NORMAL, 1);
            gpio_put(LED_WARNING, 0);
            gpio_put(LED_ALARM, 0);
            break;

        case STATE_WARNING:
            gpio_put(LED_NORMAL, 0);
            gpio_put(LED_WARNING, 1);
            gpio_put(LED_ALARM, 0);
            break;

        case STATE_ALARM:
            gpio_put(LED_NORMAL, 0);
            gpio_put(LED_WARNING, 0);

            if (alarm_ack) {
                gpio_put(LED_ALARM, 1);
            } else {
                alarm_blink = !alarm_blink;
                gpio_put(LED_ALARM, alarm_blink);
            }
            break;

        case STATE_INIT:
        default:
            gpio_put(LED_NORMAL, 0);
            gpio_put(LED_WARNING, 0);
            gpio_put(LED_ALARM, 0);
            break;
    }
}

static void log_task(void) {
    printf("raw=%u filtered=%u percent=%.1f state=%s samples=%lu ack=%s\n",
           raw_value,
           filtered_value,
           (double)percent_value,
           state_name(state),
           (unsigned long)sample_count,
           alarm_ack ? "True" : "False");
}

static void button_task(void) {
    bool current_button = gpio_get(BUTTON_GPIO);

    // Flanco de bajada: el botón pasa de 1 (suelto) a 0 (presionado).
    if (last_button && !current_button) {
        if (state == STATE_ALARM) {
            alarm_ack = true;
            printf(">>> ALARMA RECONOCIDA | ACK=True\n");
        } else {
            printf(">>> Boton presionado, pero no hay ALARM\n");
        }
    }

    last_button = current_button;
}

static void init_gpio(void) {
    const uint pins[] = {
        LED_NORMAL, LED_WARNING, LED_ALARM, HEARTBEAT_GPIO
    };

    for (unsigned int i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_OUT);
        gpio_put(pins[i], 0);
    }

    gpio_init(BUTTON_GPIO);
    gpio_set_dir(BUTTON_GPIO, GPIO_IN);
    gpio_pull_up(BUTTON_GPIO);
}

int main(void) {
    stdio_init_all();

    adc_init();
    adc_gpio_init(ADC_GPIO);
    adc_select_input(0); // GPIO26 = ADC0

    init_gpio();

    printf("DO 08: sistema multitarea cooperativo completo (C)\n");

    absolute_time_t next_sensor = get_absolute_time();
    absolute_time_t next_ui = next_sensor;
    absolute_time_t next_log = next_sensor;
    absolute_time_t next_button = next_sensor;

    while (true) {
        absolute_time_t now = get_absolute_time();

        if (absolute_time_diff_us(next_sensor, now) >= 0) {
            sensor_task();
            next_sensor = delayed_by_ms(next_sensor, SENSOR_PERIOD_MS);
            // Evita acumular ejecuciones atrasadas si una tarea tardó demasiado.
            if (absolute_time_diff_us(next_sensor, now) < 0) {
                next_sensor = delayed_by_ms(now, SENSOR_PERIOD_MS);
            }
        }

        if (absolute_time_diff_us(next_ui, now) >= 0) {
            ui_task();
            next_ui = delayed_by_ms(next_ui, UI_PERIOD_MS);
            if (absolute_time_diff_us(next_ui, now) < 0) {
                next_ui = delayed_by_ms(now, UI_PERIOD_MS);
            }
        }

        if (absolute_time_diff_us(next_log, now) >= 0) {
            log_task();
            next_log = delayed_by_ms(next_log, LOG_PERIOD_MS);
            if (absolute_time_diff_us(next_log, now) < 0) {
                next_log = delayed_by_ms(now, LOG_PERIOD_MS);
            }
        }

        if (absolute_time_diff_us(next_button, now) >= 0) {
            button_task();
            next_button = delayed_by_ms(next_button, BUTTON_PERIOD_MS);
            if (absolute_time_diff_us(next_button, now) < 0) {
                next_button = delayed_by_ms(now, BUTTON_PERIOD_MS);
            }
        }

        // Pausa muy breve para no saturar el procesador; las tareas siguen siendo cooperativas.
        sleep_ms(1);
    }

    return 0;
}

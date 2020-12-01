#include <application.h>

#define UPDATE_THERMOMETER_SECOND 20
#define UPDATE_ACCELEROMETER_SECOND 20
#define PUBLISH_INTERVAL_SECOND 60
#define MAX_TEMPERATURE 35.f
#define MAX_G_X 1.5f
#define MAX_G_Y 1.5f
#define MAX_G_Z 1.5f

// LED
hio_led_t led;

// Button
hio_button_t button;
uint16_t button_press_count = 0;

// Exgternal thermometer
hio_ds18b20_sensor_t ds18b20_sensors[1];
hio_ds18b20_t ds18b20;
float ext_temperature = NAN;

// External Accelerometer
hio_lis2dh12_t lis2dh12;
hio_lis2dh12_result_g_t result_g;

// Magnet sensor
hio_switch_t magnet;
uint32_t counter = 0;

void pub_ext_temperature(void) {
    hio_radio_pub_float("thermometer/-/temperature", &ext_temperature);
}

void pub_ext_acceleration(void) {
    hio_radio_pub_acceleration(&result_g.x_axis, &result_g.y_axis, &result_g.z_axis);
}

void pub_counter(void) {
    hio_radio_pub_uint32("magnet/-/count", &counter);
}

void button_event_handler(hio_button_t *self, hio_button_event_t event, void *event_param)
{
    if (event == HIO_BUTTON_EVENT_PRESS) {
        hio_led_pulse(&led, 100);
        button_press_count++;
        hio_log_info("APP: Button press_count = %u", button_press_count);
        hio_radio_pub_push_button(&button_press_count);
    }
}

void ds18b20_event_handler(hio_ds18b20_t *self, uint64_t device_address, hio_ds18b20_event_t event, void *param)
{
    (void) param;

    if (event == HIO_DS18B20_EVENT_UPDATE) {
        ext_temperature = NAN;
        if (hio_ds18b20_get_temperature_celsius(self, device_address, &ext_temperature)) {
            hio_log_info("APP: External Temperature %" PRIx64 " = %0.2f", device_address, ext_temperature);
            if (ext_temperature >= MAX_TEMPERATURE) {
                pub_ext_temperature();
            }
        }
    } else {
        ext_temperature = NAN;
        hio_log_error("APP: External Temperature");
    }
}

void lis2dh12_event_handler(hio_lis2dh12_t *self, hio_lis2dh12_event_t event, void *event_param)
{
    if (event == HIO_LIS2DH12_EVENT_UPDATE)
    {
        result_g.x_axis = NAN;
        result_g.y_axis = NAN;
        result_g.z_axis = NAN;
        if (hio_lis2dh12_get_result_g(self, &result_g))
        {
            hio_log_info("APP: External Acceleration = [%.2f,%.2f,%.2f]", result_g.x_axis, result_g.y_axis, result_g.z_axis);

            if ((fabs(result_g.x_axis) >= MAX_G_X) || (fabs(result_g.y_axis) >= MAX_G_Y) || (fabs(result_g.z_axis) >= MAX_G_Z)) {
                pub_ext_acceleration();
            }
        }
    } else if (event == HIO_LIS2DH12_EVENT_ERROR) {
        result_g.x_axis = NAN;
        result_g.y_axis = NAN;
        result_g.z_axis = NAN;
        hio_log_error("APP: External Accelerometer error");
    }
}

void counter_handler(hio_switch_t *self, hio_switch_event_t event, void *event_param)
{
    if (event == HIO_SWITCH_EVENT_OPENED) {
        hio_led_pulse(&led, 100);
        hio_log_info("APP: Counter = %lu", ++counter);
    }
}

void application_init(void)
{
    // Initialize log
    hio_log_init(HIO_LOG_LEVEL_DUMP, HIO_LOG_TIMESTAMP_ABS);
    hio_log_info("APP: Reset");

    // Initialize LED
    hio_led_init(&led, HIO_GPIO_LED, false, false);
    hio_led_set_mode(&led, HIO_LED_MODE_OFF);

    // Initialize button
    hio_button_init(&button, HIO_GPIO_BUTTON, HIO_GPIO_PULL_DOWN, false);
    hio_button_set_event_handler(&button, button_event_handler, NULL);

    hio_module_x1_init();

    // Initialize Exgternal thermometer
    hio_ds18b20_init(&ds18b20, hio_module_x1_get_onewire(), ds18b20_sensors, 1, HIO_DS18B20_RESOLUTION_BITS_12);
    hio_ds18b20_set_event_handler(&ds18b20, ds18b20_event_handler, NULL);
    hio_ds18b20_set_update_interval(&ds18b20, (UPDATE_THERMOMETER_SECOND) * 1000);

    // Initialize External accelerometer
    hio_lis2dh12_init(&lis2dh12, HIO_I2C_I2C_1W, 0x19);
    hio_lis2dh12_set_event_handler(&lis2dh12, lis2dh12_event_handler, NULL);
    hio_lis2dh12_set_update_interval(&lis2dh12, (UPDATE_ACCELEROMETER_SECOND) * 1000);

    // Initialize Magnet Sensor
    hio_switch_init(&magnet, HIO_GPIO_P7, HIO_SWITCH_TYPE_NC, HIO_SWITCH_PULL_UP_DYNAMIC);
    hio_switch_set_scan_interval(&magnet, 20);
    hio_switch_set_event_handler(&magnet, counter_handler, NULL);

    hio_radio_init(HIO_RADIO_MODE_NODE_SLEEPING);
    hio_radio_pairing_request("okim-bps-hugo", VERSION);

    hio_led_pulse(&led, 2000);

    hio_scheduler_plan_current_from_now(10000);
}

void application_task(void)
{
    hio_log_info("APP: Task");

    pub_ext_temperature();
    pub_ext_acceleration();
    pub_counter();

    hio_scheduler_plan_current_from_now((PUBLISH_INTERVAL_SECOND) * 1000);
}

#include <application.h>

#define UPDATE_THERMOMETER_SECOND 20
#define UPDATE_ACCELEROMETER_SECOND 20
#define PUBLISH_INTERVAL_SECOND 60
#define MAX_TEMPERATURE 35.f
#define MAX_G_X 1.5f
#define MAX_G_Y 1.5f
#define MAX_G_Z 1.5f

// LED
twr_led_t led;

// Button
twr_button_t button;
uint16_t button_press_count = 0;

// Exgternal thermometer
twr_ds18b20_sensor_t ds18b20_sensors[1];
twr_ds18b20_t ds18b20;
float ext_temperature = NAN;

// External Accelerometer
twr_lis2dh12_t lis2dh12;
twr_lis2dh12_result_g_t result_g;

// Magnet sensor
twr_switch_t magnet;
uint32_t counter = 0;

void pub_ext_temperature(void) {
    twr_radio_pub_float("thermometer/-/temperature", &ext_temperature);
}

void pub_ext_acceleration(void) {
    twr_radio_pub_acceleration(&result_g.x_axis, &result_g.y_axis, &result_g.z_axis);
}

void pub_counter(void) {
    twr_radio_pub_uint32("magnet/-/count", &counter);
}

void button_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param)
{
    if (event == TWR_BUTTON_EVENT_PRESS) {
        twr_led_pulse(&led, 100);
        button_press_count++;
        twr_log_info("APP: Button press_count = %u", button_press_count);
        twr_radio_pub_push_button(&button_press_count);
    }
}

void ds18b20_event_handler(twr_ds18b20_t *self, uint64_t device_address, twr_ds18b20_event_t event, void *param)
{
    (void) param;

    if (event == TWR_DS18B20_EVENT_UPDATE) {
        ext_temperature = NAN;
        if (twr_ds18b20_get_temperature_celsius(self, device_address, &ext_temperature)) {
            twr_log_info("APP: External Temperature %" PRIx64 " = %0.2f", device_address, ext_temperature);
            if (ext_temperature >= MAX_TEMPERATURE) {
                pub_ext_temperature();
            }
        }
    } else {
        ext_temperature = NAN;
        twr_log_error("APP: External Temperature");
    }
}

void lis2dh12_event_handler(twr_lis2dh12_t *self, twr_lis2dh12_event_t event, void *event_param)
{
    if (event == TWR_LIS2DH12_EVENT_UPDATE)
    {
        result_g.x_axis = NAN;
        result_g.y_axis = NAN;
        result_g.z_axis = NAN;
        if (twr_lis2dh12_get_result_g(self, &result_g))
        {
            twr_log_info("APP: External Acceleration = [%.2f,%.2f,%.2f]", result_g.x_axis, result_g.y_axis, result_g.z_axis);

            if ((fabs(result_g.x_axis) >= MAX_G_X) || (fabs(result_g.y_axis) >= MAX_G_Y) || (fabs(result_g.z_axis) >= MAX_G_Z)) {
                pub_ext_acceleration();
            }
        }
    } else if (event == TWR_LIS2DH12_EVENT_ERROR) {
        result_g.x_axis = NAN;
        result_g.y_axis = NAN;
        result_g.z_axis = NAN;
        twr_log_error("APP: External Accelerometer error");
    }
}

void counter_handler(twr_switch_t *self, twr_switch_event_t event, void *event_param)
{
    if (event == TWR_SWITCH_EVENT_OPENED) {
        twr_led_pulse(&led, 100);
        twr_log_info("APP: Counter = %lu", ++counter);
    }
}

void application_init(void)
{
    // Initialize log
    twr_log_init(TWR_LOG_LEVEL_DUMP, TWR_LOG_TIMESTAMP_ABS);
    twr_log_info("APP: Reset");

    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, false);
    twr_led_set_mode(&led, TWR_LED_MODE_OFF);

    // Initialize button
    twr_button_init(&button, TWR_GPIO_BUTTON, TWR_GPIO_PULL_DOWN, false);
    twr_button_set_event_handler(&button, button_event_handler, NULL);

    twr_module_x1_init();

    // Initialize Exgternal thermometer
    twr_ds18b20_init(&ds18b20, twr_module_x1_get_onewire(), ds18b20_sensors, 1, TWR_DS18B20_RESOLUTION_BITS_12);
    twr_ds18b20_set_event_handler(&ds18b20, ds18b20_event_handler, NULL);
    twr_ds18b20_set_update_interval(&ds18b20, (UPDATE_THERMOMETER_SECOND) * 1000);

    // Initialize External accelerometer
    twr_lis2dh12_init(&lis2dh12, TWR_I2C_I2C_1W, 0x19);
    twr_lis2dh12_set_event_handler(&lis2dh12, lis2dh12_event_handler, NULL);
    twr_lis2dh12_set_update_interval(&lis2dh12, (UPDATE_ACCELEROMETER_SECOND) * 1000);

    // Initialize Magnet Sensor
    twr_switch_init(&magnet, TWR_GPIO_P7, TWR_SWITCH_TYPE_NC, TWR_SWITCH_PULL_UP_DYNAMIC);
    twr_switch_set_scan_interval(&magnet, 20);
    twr_switch_set_event_handler(&magnet, counter_handler, NULL);

    twr_radio_init(TWR_RADIO_MODE_NODE_SLEEPING);
    twr_radio_pairing_request("okim-bps-hugo", VERSION);

    twr_led_pulse(&led, 2000);

    twr_scheduler_plan_current_from_now(10000);
}

void application_task(void)
{
    twr_log_info("APP: Task");

    pub_ext_temperature();
    pub_ext_acceleration();
    pub_counter();

    twr_scheduler_plan_current_from_now((PUBLISH_INTERVAL_SECOND) * 1000);
}

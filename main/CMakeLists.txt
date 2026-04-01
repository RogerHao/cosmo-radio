idf_component_register(
    SRCS "tusb_hid_example_main.c"
         "input_handler.c"
         "led_indicator.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_gpio esp_timer led_strip
)

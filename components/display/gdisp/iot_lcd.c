
#include "sdkconfig.h"
#include "iot_lcd.h"


/**
 * Define screen instance
 */
#ifdef CONFIG_LCD_DRIVER_SCREEN_CONTROLLER_ST7789
extern lcd_driver_fun_t lcd_st7789_default_driver;
static lcd_driver_fun_t *g_lcd_driver = &lcd_st7789_default_driver;
#elif defined CONFIG_LCD_DRIVER_SCREEN_CONTROLLER_ST7796
extern lcd_driver_fun_t lcd_st7796_default_driver;
static lcd_driver_fun_t *g_lcd_driver = &lcd_st7796_default_driver;
#elif defined CONFIG_LCD_DRIVER_SCREEN_CONTROLLER_ILI9341
extern lcd_driver_fun_t lcd_ili9341_default_driver;
static lcd_driver_fun_t *g_lcd_driver = &lcd_ili9341_default_driver;
#elif defined CONFIG_LCD_DRIVER_SCREEN_CONTROLLER_NT35510
extern lcd_driver_fun_t lcd_nt35510_default_driver;
static lcd_driver_fun_t *g_lcd_driver = &lcd_nt35510_default_driver;
#elif defined CONFIG_LCD_DRIVER_SCREEN_CONTROLLER_ILI9806
extern lcd_driver_fun_t lcd_ili9806_default_driver;
static lcd_driver_fun_t *g_lcd_driver = &lcd_ili9806_default_driver;
#elif defined CONFIG_LCD_DRIVER_SCREEN_CONTROLLER_ILI9486
extern lcd_driver_fun_t lcd_ili9486_default_driver;
static lcd_driver_fun_t *g_lcd_driver = &lcd_ili9486_default_driver;
#elif defined CONFIG_LCD_DRIVER_SCREEN_CONTROLLER_SSD1306
extern lcd_driver_fun_t lcd_ssd1306_default_driver;
static lcd_driver_fun_t *g_lcd_driver = &lcd_ssd1306_default_driver;
#elif defined CONFIG_LCD_DRIVER_SCREEN_CONTROLLER_SSD1351
extern lcd_driver_fun_t lcd_ssd1351_default_driver;
static lcd_driver_fun_t *g_lcd_driver = &lcd_ssd1351_default_driver;
#elif defined CONFIG_LCD_DRIVER_SCREEN_CONTROLLER_SSD1322
extern lcd_driver_fun_t lcd_ssd1322_default_driver;
static lcd_driver_fun_t *g_lcd_driver = &lcd_ssd1322_default_driver;
#else
#error "Must select a screen controller driver"
#endif

esp_err_t iot_lcd_init(lcd_driver_fun_t *driver)
{
    lcd_config_t lcd_conf = {
#ifdef CONFIG_LCD_DRIVER_INTERFACE_I2C
        .iface_i2c = {
            .pin_num_sda = CONFIG_LCD_I2C_SDA_PIN,
            .pin_num_scl = CONFIG_LCD_I2C_SCL_PIN,
            .clk_freq = CONFIG_LCD_I2C_CLOCK_FREQ,
            .i2c_port = CONFIG_LCD_I2C_PORT_NUM,
            .i2c_addr = CONFIG_LCD_I2C_ADDRESS,
        },
#endif
#ifdef CONFIG_LCD_DRIVER_INTERFACE_SPI
        .iface_spi = {
            .pin_num_miso = CONFIG_LCD_SPI_MISO_PIN,
            .pin_num_mosi = CONFIG_LCD_SPI_MOSI_PIN,
            .pin_num_clk = CONFIG_LCD_SPI_CLK_PIN,
            .pin_num_cs = CONFIG_LCD_SPI_CS_PIN,
            .pin_num_dc = CONFIG_LCD_SPI_DC_PIN,
            .clk_freq = CONFIG_LCD_SPI_CLOCK_FREQ,
            .spi_host = CONFIG_LCD_SPI_HOST,
            .dma_chan = 2,
            .init_spi_bus = true,
        },
#endif
#ifdef CONFIG_LCD_DRIVER_INTERFACE_I2S
        .iface_8080 = {
            .data_width = CONFIG_LCD_I2S_BITWIDTH,
            .pin_data_num = {
                CONFIG_LCD_I2S_D0_PIN,
                CONFIG_LCD_I2S_D1_PIN,
                CONFIG_LCD_I2S_D2_PIN,
                CONFIG_LCD_I2S_D3_PIN,
                CONFIG_LCD_I2S_D4_PIN,
                CONFIG_LCD_I2S_D5_PIN,
                CONFIG_LCD_I2S_D6_PIN,
                CONFIG_LCD_I2S_D7_PIN,
#if CONFIG_LCD_I2S_BITWIDTH > 8
                CONFIG_LCD_I2S_D8_PIN,
                CONFIG_LCD_I2S_D9_PIN,
                CONFIG_LCD_I2S_D10_PIN,
                CONFIG_LCD_I2S_D11_PIN,
                CONFIG_LCD_I2S_D12_PIN,
                CONFIG_LCD_I2S_D13_PIN,
                CONFIG_LCD_I2S_D14_PIN,
                CONFIG_LCD_I2S_D15_PIN,
#endif
            },
            .pin_num_wr = CONFIG_LCD_I2S_WR_PIN,
            .pin_num_rd = -1,
            .pin_num_rs = CONFIG_LCD_I2S_RS_PIN,
            .i2s_port = CONFIG_LCD_I2S_PORT_NUM,
            .clk_freq = 20*1000000,
        },
#endif
        .pin_num_rst = CONFIG_LCD_RESET_PIN,
        .pin_num_bckl = CONFIG_LCD_BL_PIN,
        .rst_active_level = 0,
        .bckl_active_level = CONFIG_LCD_BCKL_ACTIVE_LEVEL,
        .width = CONFIG_LCD_DRIVER_SCREEN_WIDTH,
        .height = CONFIG_LCD_DRIVER_SCREEN_HEIGHT,
        .rotate = CONFIG_LCD_DIRECTION,
    };
    esp_err_t ret = g_lcd_driver->init(&lcd_conf);
    *driver = *g_lcd_driver;
    return ret;
}
#include "bme280.h"

#include <stdint.h>

typedef struct {
    int32_t integer_part;
    int32_t fractional_part;
} Temperature;

static Temperature bmp280_compensate_temp(int32_t adc_T) {
    // Kalibracyjne stałe
    int32_t dig_T1 = 27504;
    int32_t dig_T2 = 26435;
    int32_t dig_T3 = -1000;

    int32_t var1, var2, T;

    var1 =
        ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) *
              ((adc_T >> 4) - ((int32_t)dig_T1))) >>
             12) *
            ((int32_t)dig_T3)) >>
           14;
    T = var1 + var2;
    int32_t T_fine = T * 5 + 128;

    // Obliczanie temperatury w stopniach Celsjusza
    int32_t temp = T_fine / 256;
    int32_t temp_fraction = ((T_fine % 256) * 100) / 256;

    Temperature result;
    result.integer_part = temp / 100;
    result.fractional_part = temp_fraction;

    return result;
}

void read_temperature_from_bmp280(struct device *i2c_dev) {
    if (!i2c_dev) {
        printk("Failed to get binding for I2C device\n");
        return;
    }

    uint8_t temp_msb, temp_lsb, temp_xlsb;
    int ret;

    // Odczyt MSB
    ret = i2c_reg_read_byte(i2c_dev, BMP280_I2C_ADDRESS, BMP280_REG_TEMP_MSB,
                            &temp_msb);
    if (ret) {
        printk("Failed to read MSB register: %d\n", ret);
        return;
    }

    // Odczyt LSB
    ret = i2c_reg_read_byte(i2c_dev, BMP280_I2C_ADDRESS, BMP280_REG_TEMP_LSB,
                            &temp_lsb);
    if (ret) {
        printk("Failed to read LSB register: %d\n", ret);
        return;
    }

    // Odczyt XLSB
    ret = i2c_reg_read_byte(i2c_dev, BMP280_I2C_ADDRESS, BMP280_REG_TEMP_XLSB,
                            &temp_xlsb);
    if (ret) {
        printk("Failed to read XLSB register: %d\n", ret);
        return;
    }

    int32_t temp_raw =
        (int32_t)(((uint32_t)temp_msb << 12) | ((uint32_t)temp_lsb << 4) |
                  ((uint32_t)temp_xlsb >> 4));

    Temperature temp_celsius;
    temp_celsius = bmp280_compensate_temp(temp_raw);

    printk("Temperature: %d.02%d st. C\n", temp_celsius.integer_part,
           temp_celsius.fractional_part);
}
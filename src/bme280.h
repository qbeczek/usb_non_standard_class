#ifndef BME_280_H
#define BME_280_H
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>

#define BMP280_I2C_ADDRESS 0x76
// Rejestry BMP280
#define BMP280_REG_CHIPID 0xD0  // Adres rejestru identyfikatora chipu
#define BMP280_CHIPID 0x58      // Chip ID BMP280

#define BMP280_REG_CONTROL 0xF4
#define BMP280_REG_RESULT_PRESSURE \
    0xF7  // 0xF7(msb) , 0xF8(lsb) , 0xF9(xlsb) : stores the pressure data.
#define BMP280_REG_RESULT_TEMPRERATURE \
    0xFA  // 0xFA(msb) , 0xFB(lsb) , 0xFC(xlsb) : stores the temperature data.

#define BMP280_REG_TEMP_XLSB 0xFC
#define BMP280_REG_TEMP_LSB 0xFB
#define BMP280_REG_TEMP_MSB 0xFA

void read_temperature_from_bmp280(struct device *i2c_device);

#endif /* BME_280_h */
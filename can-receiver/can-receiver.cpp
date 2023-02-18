#include <iostream>
#include <string>
#include "pico/stdlib.h"
#include "mcp2515.h"
#include "pico-ssd1306/ssd1306.h"
#include "pico-ssd1306/textRenderer/TextRenderer.h"
#include "hardware/i2c.h"

using namespace std;
using namespace pico_ssd1306;

const uint I2C_SDA = 2;
const uint I2C_SCL = 3;

int main()
{
    stdio_init_all();

    gpio_init(I2C_SDA);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);

    gpio_init(I2C_SCL);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SCL);

    i2c_init(i2c1, 1000000);

    SSD1306 display(i2c1, 0x3C, Size::W128xH64);

    drawText(&display, font_8x8, "Temperature:", 0, 0);
    drawText(&display, font_8x8, "Waiting...", 0, 16);
    display.sendBuffer();

    MCP2515 canInterface;
    can_frame frame;

    canInterface.reset();
    canInterface.setBitrate(CAN_1000KBPS, MCP_16MHZ);
    canInterface.setNormalMode();

    char temperature[16] = {0};
    char canIdMSB[17] = {'0'}, canIdLSB[17] = {'0'};
    while (true)
    {
        if (canInterface.readMessage(&frame) == MCP2515::ERROR_OK)
        {
            cout << "Inside" << endl;
            drawText(&display, font_8x8, "Received...", 0, 16);
            display.sendBuffer();
            sleep_ms(1000);
            for (int i = 0; i < frame.can_dlc; i++)
            {
                temperature[i] = frame.data[i];
            }
            drawText(&display, font_8x8, temperature, 0, 16);
            for (int i = 0; i < 16; i++)
            {
                canIdLSB[i] = frame.can_id >> i;
                canIdLSB[i] += '0';
            }
            for (int i = 0; i < 16; i++)
            {
                canIdMSB[i] = frame.can_id >> (i + 16);
                canIdMSB[i] += '0';
            }
            drawText(&display, font_8x8, canIdMSB, 0, 32);
            drawText(&display, font_8x8, canIdLSB, 0, 48);
            display.sendBuffer();
        }
    }

    return 0;
}

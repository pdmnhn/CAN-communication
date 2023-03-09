#include <iostream>
#include <string>
#include <cstring>
#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/adc.h"
#include "mcp2515.h"

using namespace std;

const uint ADC_CHN = 4;
const uint ADC_PIN = ADC_CHN + 26;
const uint ADC_RANGE = 1 << 12;
const uint LED = 25;
const double ADC_VREF = 3.3;
const double ADC_CONVERSION_FACTOR = ADC_VREF / (ADC_RANGE - 1);
const unsigned char MAC[8] = {0xFF, 0xDD, 0xCC, 0xBB, 0x00, 0x11, 0x22, 0x33};

uint32_t getCanID(uint16_t dataId, uint16_t counter, uint8_t command)
{
    return dataId << 21 | (0b11 << 19) | (command << 17) | (counter << 1) | CAN_EFF_FLAG;
}

int main()
{
    stdio_init_all();
    const uint16_t temperatureId = 0b01001100110;
    uint16_t counter = 0;

    MCP2515 canInterface;
    can_frame frame;
    frame.can_id;

    canInterface.reset();
    canInterface.setBitrate(CAN_1000KBPS, MCP_16MHZ);
    canInterface.setNormalMode();

    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(ADC_CHN);
    gpio_init(LED);
    gpio_set_dir(LED, GPIO_OUT);

    double reading;
    unsigned char temperature = 0;
    // string temperatureString;

    while (true)
    {
        reading = adc_read() * ADC_CONVERSION_FACTOR;
        temperature = 27 - (reading - 0.706) / 0.001721;
        // temperature = (temperature + 1) % 10000;
        // temperatureString = to_string(temperature);

        // for (int i = 0; i < temperatureString.length(); i++)
        // {
        //     frame.data[i] = temperatureString[i];
        //     cout << frame.data[i] << ", ";
        // }
        // cout << endl;

        frame.data[0] = temperature;
        cout << "Temperature: " << (int)temperature << endl;
        frame.can_dlc = 1;

        frame.can_id = getCanID(temperatureId, counter, 0b00);

        if (canInterface.sendMessage(&frame) == MCP2515::ERROR_OK)
        {
            frame.can_id = getCanID(temperatureId, counter, 0b01);
            frame.can_dlc = 8;
            for (int i = 0; i < 8; i++)
            {
                frame.data[i] = MAC[i];
            }
            if (canInterface.sendMessage(&frame) == MCP2515::ERROR_OK)
            {
                cout << "Success: " << counter << endl
                     << endl;
                gpio_put(LED, 1);
                sleep_ms(2500);
                gpio_put(LED, 0);
                sleep_ms(2500);
            }
        }
        counter = (counter + 1) % 0xFFFF;
    }

    return 0;
}

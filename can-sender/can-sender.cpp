#include <string>
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

int main()
{
    stdio_init_all();

    MCP2515 canInterface;
    can_frame frame;
    frame.can_id = 0x000;

    // Initialize interface
    canInterface.reset();
    canInterface.setBitrate(CAN_1000KBPS, MCP_16MHZ);
    canInterface.setNormalMode();
    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(ADC_CHN);
    gpio_init(LED);
    gpio_set_dir(LED, GPIO_OUT);

    double reading, temperature;
    string temperatureString;

    while (true)
    {
        gpio_put(LED, 1);
        reading = adc_read() * ADC_CONVERSION_FACTOR;
        temperature = 27 - (reading - 0.706) / 0.001721;
        temperatureString = to_string(temperature);
        for (uint i = 0; i < temperatureString.length(); i++)
        {
            frame.data[i] = temperatureString[i];
        }
        frame.can_dlc = temperatureString.length();
        canInterface.sendMessage(&frame);
        sleep_ms(5000);
        gpio_put(LED, 0);
        sleep_ms(5000);
    }

    return 0;
}

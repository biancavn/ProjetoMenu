#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "inc/ssd1306.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

// ---------------------------------------------------------
// CONSTANTES
// ---------------------------------------------------------
#define VRX 26
#define VRY 27

#define ADC_CHANNEL_0 0
#define ADC_CHANNEL_1 1

#define SW 22

#define I2C_SDA 14
#define I2C_SCL 15

#define NAV_UP_THRESHOLD 1900
#define NAV_DOWN_THRESHOLD 2100

#define BUZZER_PIN 21

const uint LED = 12;
const uint16_t PERIOD = 2000;
const float DIVIDER_PWM = 16.0;
const uint16_t LED_STEP = 100;

const int LED_B = 13;
const int LED_R = 11;
const uint16_t PERIOD_JOYSTICK = 4096;

const uint star_wars_notes[] = {
    330, 330, 330, 262, 392, 523, 330, 262,
    392, 523, 330, 659, 659, 659, 698, 523,
    415, 349, 330, 262, 392, 523, 330, 262,
    392, 523, 330, 659, 659, 659, 698, 523,
    415, 349, 330, 523, 494, 440, 392, 330,
    659, 784, 659, 523, 494, 440, 392, 330,
    659, 659, 330, 784, 880, 698, 784, 659,
    523, 494, 440, 392, 659, 784, 659, 523,
    494, 440, 392, 330, 659, 523, 659, 262,
    330, 294, 247, 262, 220, 262, 330, 262,
    330, 294, 247, 262, 330, 392, 523, 440,
    349, 330, 659, 784, 659, 523, 494, 440,
    392, 659, 784, 659, 523, 494, 440, 392
};

const uint note_duration[] = {
    500, 500, 500, 350, 150, 300, 500, 350,
    150, 300, 500, 500, 500, 500, 350, 150,
    300, 500, 500, 350, 150, 300, 500, 350,
    150, 300, 650, 500, 150, 300, 500, 350,
    150, 300, 500, 150, 300, 500, 350, 150,
    300, 650, 500, 350, 150, 300, 500, 350,
    150, 300, 500, 500, 500, 500, 350, 150,
    300, 500, 500, 350, 150, 300, 500, 350,
    150, 300, 500, 350, 150, 300, 500, 500,
    350, 150, 300, 500, 500, 350, 150, 300,
};


// ---------------------------------------------------------
// VARIÁVEIS GLOBAIS
// ---------------------------------------------------------
uint16_t led_level = 100;

uint16_t led_b_level, led_r_level = 100;
uint slice_led_b, slice_led_r;

char *menu_options[] = {"Code 1", "Code 2", "Code 3"};
int num_options = sizeof(menu_options) / sizeof(menu_options[0]);

volatile bool button_flag = false;
// ---------------------------------------------------------
// DECLARAÇÃO DAS FUNÇÕES
// ---------------------------------------------------------

void setup_hardware();
void setup_pwm_led(uint led, uint *slice, uint16_t level);
void setup_pwm();
void joystick_read_axis(uint16_t *vry_value);
void joystick_read_axis_both(uint16_t *vrx_value, uint16_t *vry_value);
void wait_for_button_press_release();
void button_callback(uint gpio, uint32_t events);
void display_menu(int current_option);
void pwm_init_buzzer(uint pin);
void play_tone(uint pin, uint frequency, uint duration_ms);
void play_star_wars(uint pin);
void code1();
void code2();
void code3();

// ---------------------------------------------------------
// AGUARDA O BOTÃO SER PRESSIONADO E LIBERADO E CALLBACK
// ---------------------------------------------------------

void wait_for_button_press_release()
{
    while(!gpio_get(SW))
    {
        sleep_ms(10);
    }
    sleep_ms(200);
    button_flag = false;
}
void button_callback(uint gpio, uint32_t events)
{
    button_flag = true;
}


// ---------------------------------------------------------
// CONFIGURAÇÃO DOS SETUPS
// ---------------------------------------------------------
void setup_hardware()
{
    adc_init();
    adc_gpio_init(VRY);
    adc_gpio_init(VRX);

    gpio_init(SW);
    gpio_set_dir(SW, GPIO_IN);
    gpio_pull_up(SW);

    gpio_set_irq_enabled_with_callback(SW, GPIO_IRQ_EDGE_FALL, true, button_callback);

    i2c_init(i2c1, 100*1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_init();
    ssd1306_clear();    
}

void setup_pwm_led(uint led, uint *slice, uint16_t level)
{
    gpio_set_function(led, GPIO_FUNC_PWM);
    *slice = pwm_gpio_to_slice_num(led);
    pwm_set_clkdiv(*slice, DIVIDER_PWM);
    pwm_set_wrap(*slice, PERIOD_JOYSTICK);
    pwm_set_gpio_level(led, level);
    pwm_set_enabled(*slice, true);
}
void setup_pwm()
{
    uint slice;
    gpio_set_function(LED, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(LED);
    pwm_set_clkdiv(slice, DIVIDER_PWM);
    pwm_set_wrap(slice, PERIOD);
    pwm_set_gpio_level(LED, led_level);
    pwm_set_enabled(slice, true);
}

// ---------------------------------------------------------
// LEITURA DOS EIXOS DO JOYSTICK
// ---------------------------------------------------------
void joystick_read_axis(uint16_t *vry_value){
    adc_select_input(ADC_CHANNEL_1);
    sleep_us(2);
    *vry_value = adc_read();
}

void joystick_read_axis_both(uint16_t *vrx_value, uint16_t *vry_value)
{
    adc_select_input(ADC_CHANNEL_0);
    sleep_us(2);
    *vrx_value = adc_read();

    adc_select_input(ADC_CHANNEL_1);
    sleep_us(2);
    *vry_value = adc_read();
}

// ---------------------------------------------------------
// EXIBE O MENU NA TELA OLED
// ---------------------------------------------------------
void display_menu(int current_option)
{
    struct render_area frame_area = {
        .start_column = 0,
        .end_column = ssd1306_width - 1,
        .start_page = 0,
        .end_page = ssd1306_n_pages - 1
    };
    calculate_render_area_buffer_length(&frame_area);

    uint8_t buffer[ssd1306_buffer_length];
    memset(buffer, 0, ssd1306_buffer_length);

    int start_y = (ssd1306_height - (num_options*12))/2;

    for(int i = 0; i < num_options; i++)
    {
        int x_offset = 5;
        int y_position = start_y + (i*15);
        if(i == current_option)
        {
            ssd1306_draw_string(buffer, x_offset, y_position, "X  ");
            ssd1306_draw_string(buffer, x_offset +20, y_position, menu_options[i]);
        }
        else
        {
            ssd1306_draw_string(buffer, x_offset + 20, y_position, menu_options[i]);
        }
    }
    render_on_display(buffer, &frame_area);
}
// ---------------------------------------------------------
// CONFIGURAÇÃO DO BUZZER E TOCAR STAR WARS
// ---------------------------------------------------------

void pwm_init_buzzer(uint pin)
{
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f);
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(pin, 0);
}

void play_tone(uint pin, uint frequency, uint duration_ms)
{
    uint slice_num = pwm_gpio_to_slice_num(pin);
    uint32_t clock_freq = clock_get_hz(clk_sys);
    uint32_t top = clock_freq / frequency - 1;

    pwm_set_wrap(slice_num, top);
    pwm_set_gpio_level(pin, top/2);

    uint elapsed = 0;
    while(elapsed < duration_ms)
    {
        sleep_ms(10);
        elapsed += 10;

        if(button_flag)
        {
            break;
        }
    }

    pwm_set_gpio_level(pin, 0);

    elapsed = 0;
    while(elapsed < 50)
    {
        sleep_ms(10);
        elapsed += 10;

        if(button_flag)
        {
            break;
        }
    }
}

void play_star_wars(uint pin)
{
    int total_notes = sizeof(star_wars_notes) / sizeof(star_wars_notes[0]);
    for(int i = 0; i < total_notes; i++)
    {
        if(button_flag) 
        {
            break;
        }

        if(star_wars_notes[i] == 0)
        {
            uint elapsed = 0;
            while(elapsed < note_duration[i])
            {
                sleep_ms(10);
                elapsed += 10;
                if(button_flag)
                {
                    break;
                }
            }
        }
        else
        {
            play_tone(pin, star_wars_notes[i], note_duration[i]);
        }
        if(button_flag)
        {
            break;
        }
    }
}
// ---------------------------------------------------------
// CODES
// ---------------------------------------------------------
void code1()
{
    setup_pwm_led(LED_B, &slice_led_b, led_b_level);
    setup_pwm_led(LED_R, &slice_led_r, led_r_level);

    uint16_t vrx_value, vry_value;
    while(!button_flag)
    {
        joystick_read_axis_both(&vrx_value, &vry_value);
        pwm_set_gpio_level(LED_B, vrx_value);
        pwm_set_gpio_level(LED_R, vry_value);
        sleep_ms(50);
    }
    wait_for_button_press_release();

    uint slice1 = pwm_gpio_to_slice_num(LED_B);
    pwm_set_enabled(slice1, false);
    gpio_set_function(LED_B, GPIO_FUNC_SIO);
    gpio_put(LED_B, 0);

    uint slice2 = pwm_gpio_to_slice_num(LED_R);
    pwm_set_enabled(slice2, false);
    gpio_set_function(LED_R, GPIO_FUNC_SIO);
    gpio_put(LED_R, 0);
}

void code2()
{
    pwm_init_buzzer(BUZZER_PIN);

    while(!button_flag)
    {
        play_star_wars(BUZZER_PIN);
        sleep_ms(50);
    }

    wait_for_button_press_release();
}

void code3()
{
    uint up_down = 1;
    setup_pwm();
    while(!button_flag)
    {
        pwm_set_gpio_level(LED, led_level);
        sleep_ms(1000);
        if(up_down)
        {
            led_level += LED_STEP;
            if(led_level >= PERIOD)
            {
                up_down = 0;
            }
        }
        else
        {
            led_level -= LED_STEP;
            if(led_level <= LED_STEP)
            {
                up_down = 1;
            }
        }
        sleep_ms(50);
    }
    wait_for_button_press_release();

    uint slice = pwm_gpio_to_slice_num(LED);
    pwm_set_enabled(slice, false);
    gpio_set_function(LED, GPIO_FUNC_SIO);
    gpio_put(LED, 0);
}
// ---------------------------------------------------------
// FUNÇÃO PRINCIPAL
// ---------------------------------------------------------
int main()
{
    stdio_init_all();
    setup_hardware();

    int current_option = 0;
    uint16_t vry_value = 0;

    display_menu(current_option);

    while(true)
    {
        joystick_read_axis(&vry_value);

        if(vry_value < NAV_UP_THRESHOLD && current_option > 0)
        {
            current_option--;
            display_menu(current_option);
            sleep_ms(200);
        }

        if(vry_value > NAV_DOWN_THRESHOLD && current_option < num_options - 1)
        {
            current_option++;
            display_menu(current_option);
            sleep_ms(200);
        }

        if(button_flag)
        {
            wait_for_button_press_release();

            if(current_option == 0)
            {
                code1();
            }
            else if(current_option == 1)
            {
                code2();
            }
            else
            {
                code3();
            }

            display_menu(current_option);
        }
        sleep_ms(50);
    }

    return 0;
}

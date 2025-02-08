#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "inc/ssd1306.h"

// ---------------------------------------------------------
// DEFINIÇÕES
// ---------------------------------------------------------
#define VRY         27      // Pino analógico para o eixo Y do joystick (GP27 → ADC1)
#define ADC_CHANNEL 1       // Canal ADC correspondente
#define SW          22      // Pino do botão do joystick
#define I2C_SDA     14      // Pino SDA para I2C
#define I2C_SCL     15      // Pino SCL para I2C

#define NAV_UP_THRESHOLD    1900  // Limite superior para mover para cima
#define NAV_DOWN_THRESHOLD  2100  // Limite inferior para mover para baixo

// ---------------------------------------------------------
// PROTÓTIPOS DAS FUNÇÕES
// ---------------------------------------------------------
void setup_hardware(void);
void joystick_read_axis(uint16_t *vry_value);
void wait_for_button_press_and_release(void);
void display_menu(int current_option);

// ---------------------------------------------------------
// VARIÁVEIS GLOBAIS
// ---------------------------------------------------------
char *menu_options[] = {"Opcao 1", "Opcao 2", "Opcao 3"};
int num_options = sizeof(menu_options) / sizeof(menu_options[0]);

// ---------------------------------------------------------
// FUNÇÃO PRINCIPAL
// ---------------------------------------------------------
int main() {
    stdio_init_all();
    setup_hardware();

    // Inicializa o estado do menu
    int current_option = 0;
    uint16_t vry_value = 0;

    while (true) {
        joystick_read_axis(&vry_value);

        // Navegação para cima (somente com o eixo Y)
        if (vry_value < NAV_UP_THRESHOLD && current_option > 0) {
            current_option--;
            display_menu(current_option);
            sleep_ms(200); // Debounce
        }

        // Navegação para baixo (somente com o eixo Y)
        if (vry_value > NAV_DOWN_THRESHOLD && current_option < num_options - 1) {
            current_option++;
            display_menu(current_option);
            sleep_ms(200); // Debounce
        }

        // Seleção da opção
        if (!gpio_get(SW)) {
            wait_for_button_press_and_release();
            printf("Opcao selecionada: %s\n", menu_options[current_option]);
            sleep_ms(500); // Pausa para evitar seleções múltiplas
        }

        sleep_ms(50); // Pequena pausa para evitar sobrecarga
    }

    return 0;
}

// ---------------------------------------------------------
// CONFIGURAÇÃO DO HARDWARE
// ---------------------------------------------------------
void setup_hardware(void) {
    // Configura o ADC para o eixo Y do joystick
    adc_init();
    adc_gpio_init(VRY);

    // Configura o botão do joystick com pull-up
    gpio_init(SW);
    gpio_set_dir(SW, GPIO_IN);
    gpio_pull_up(SW);

    // Inicializa o I2C para o display OLED
    i2c_init(i2c1, 100 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicializa o display SSD1306
    ssd1306_init();
    ssd1306_clear();
}

// ---------------------------------------------------------
// LEITURA DO EIXO Y DO JOYSTICK (somente o eixo Y é utilizado para navegação)
// ---------------------------------------------------------
void joystick_read_axis(uint16_t *vry_value) {
    // Usando apenas o eixo Y (não importa o valor do eixo X)
    adc_select_input(ADC_CHANNEL); 
    *vry_value = adc_read();
}

// ---------------------------------------------------------
// AGUARDA O BOTÃO SER PRESSIONADO E LIBERADO
// ---------------------------------------------------------
void wait_for_button_press_and_release(void) {
    while (gpio_get(SW) == 1) {
        sleep_ms(10);
    }
    sleep_ms(200); // Debounce
    while (gpio_get(SW) == 0) {
        sleep_ms(10);
    }
    sleep_ms(200); // Debounce
}

// ---------------------------------------------------------
// EXIBE O MENU NA TELA OLED
// ---------------------------------------------------------
void display_menu(int current_option) {
    struct render_area frame_area = {
        .start_column = 0,
        .end_column   = ssd1306_width - 1,
        .start_page   = 0,
        .end_page     = ssd1306_n_pages - 1
    };
    calculate_render_area_buffer_length(&frame_area);

    uint8_t buffer[ssd1306_buffer_length];
    memset(buffer, 0, ssd1306_buffer_length);

    // Centralizar o menu na tela
    int start_y = (ssd1306_height - (num_options * 12)) / 2;

    // Exibe as opções do menu
    for (int i = 0; i < num_options; i++) {
        int x_offset = 5;
        int y_position = start_y + (i * 15); // Espaçamento ajustado para maior clareza entre as opções

        // Exibe as opções com um "X" fixo à esquerda
        if (i == current_option) {
            // Exibe o "X" fixo à esquerda da opção selecionada
            ssd1306_draw_string(buffer, x_offset, y_position, "X   "); // O "X" fixo
            ssd1306_draw_string(buffer, x_offset + 20, y_position, menu_options[i]); // Texto da opção
        } else {
            // Apenas exibe a opção sem o "X"
            ssd1306_draw_string(buffer, x_offset + 20, y_position, menu_options[i]);
        }
    }

    render_on_display(buffer, &frame_area);
}

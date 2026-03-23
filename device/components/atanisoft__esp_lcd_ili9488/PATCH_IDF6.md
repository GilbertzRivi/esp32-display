# Patch IDF 6.0 -- atanisoft/esp_lcd_ili9488

## Problem

Komponent `atanisoft/esp_lcd_ili9488` v1.0.11 uzywa przestarzalego API
`esp_lcd_panel_dev_config_t.color_space` (typ `esp_lcd_color_space_t`),
ktore zostalo usuniete w ESP-IDF v6.0.

IDF 6.0 uzywa zamiast tego pola `rgb_ele_order` (typ `lcd_rgb_element_order_t`).

Blad kompilacji bez patcha (IDF >= 6.0):

    error: 'esp_lcd_panel_dev_config_t' has no member named 'color_space'

## Wprowadzone zmiany (esp_lcd_ili9488.c, ~linia 370)

Zastapiono:

    switch (panel_dev_config->color_space)
    {
        case ESP_LCD_COLOR_SPACE_RGB: ...
        case ESP_LCD_COLOR_SPACE_BGR: ...

Na:

    switch (panel_dev_config->rgb_ele_order)
    {
        case LCD_RGB_ELEMENT_ORDER_RGB: ...
        case LCD_RGB_ELEMENT_ORDER_BGR: ...

## Dlaczego komponent jest w components/ a nie managed_components/

Katalog `managed_components/` jest zarzadzany automatycznie przez `idf.py`
i jest nadpisywany przy `idf.py update-dependencies` lub `idf.py fullclean`.
Stad przeniesienie do `components/`, ktory nie jest dotkany przez system
zarzadzania komponentami.

Wpis `atanisoft/esp_lcd_ili9488` zostal usuniety z `main/idf_component.yml`.

## Jesli komponent trzeba zaktualizowac

1. Pobierz nowa wersje ze https://components.espressif.com/components/atanisoft/esp_lcd_ili9488
2. Rozpakuj do `components/atanisoft__esp_lcd_ili9488/`
3. Zastosuj zmiane opisana powyzej w `esp_lcd_ili9488.c`
4. Zaktualizuj ten plik jesli cos sie zmienilo

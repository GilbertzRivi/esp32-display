## Pinout

### Wspólna magistrala SPI używana w projekcie

Mimo że moduł ma osobno wyprowadzone sygnały LCD i touch, w tym projekcie oba układy są podłączone do tej samej magistrali SPI po stronie MCU, z osobnymi liniami CS.

| Sygnał | GPIO |
|--------|------|
| SCK    | 5    |
| MOSI   | 6    |
| MISO   | 4    |

### ILI9488 (LCD)

| Oznaczenie na module | Normalna nazwa | GPIO |
|----------------------|----------------|------|
| CS                   | CS             | 9    |
| RST                  | RST            | 8    |
| D/C                  | DC             | 7    |

### XPT2046 (touch)

| Oznaczenie na module | Normalna nazwa | GPIO |
|----------------------|----------------|------|
| TCS                  | CS             | 3    |
| PEN                  | INT / IRQ      | 2    |

## Tłumaczenie oznaczeń z modułu

| Oznaczenie na module | Znaczy |
|----------------------|--------|
| SDI                  | MOSI |
| SDO                  | MISO |
| SCK                  | CLK |
| D/C                  | DC |
| TCS                  | Touch CS |
| PEN                  | Touch INT / IRQ |

## Mostkowanie przy lutowaniu

TCK/TDI/TDO nie są połączone z magistralą SPI na płytce modułu — trzeba je zmostkować ręcznie:

| Pad touch | Zlutować z |
|-----------|------------|
| TCK       | SCK (GPIO 5) |
| TDI       | SDI (GPIO 6) |
| TDO       | SDO (GPIO 4) |

**UWAGA: SDO (LCD) odlutowane od GPIO4.** ILI9488 trzyma linię MISO low gdy nie jest aktywny, co blokuje odpowiedzi XPT2046. SDO LCD nie jest potrzebne — ILI9488 jest write-only. Na GPIO4 podłączone jest tylko TDO (touch).

## Pełna tabela: pin modułu → GPIO

| Pin modułu | GPIO | Uwagi |
|------------|------|-------|
| VDD        | 3.3V | |
| GND        | GND  | |
| CS         | 9    | LCD chip select |
| RST        | 8    | LCD reset |
| D/C        | 7    | LCD data/command |
| SDI        | 6    | SPI MOSI |
| SCK        | 5    | SPI CLK |
| BL         | 3.3V | podswietlenie |
| SDO        | NC   | NIE podlaczac — ciagnie MISO low, blokuje touch |
| TCK        | 5    | zlutowac z SCK |
| TCS        | 3    | Touch chip select |
| TDI        | 6    | zlutowac z SDI |
| TDO        | 4    | zlutowac z SDO |
| PEN        | 2    | Touch INT/IRQ |

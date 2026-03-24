# ESP32 Display — Theme & Layout Specification

Dokument opisuje **dokładnie** co firmware parsuje z `theme.json` i `layout.json`.
Nic poza tym nie jest obsługiwane.

---

## theme.json

```json
{
  "palette":   { ... },
  "font":      "unscii_8",
  "rotation":  0,
  "widgets":   { ... }
}
```

### `palette`

```json
"palette": {
  "bg":     "#0a0a0a",
  "fg":     "#00ff41",
  "accent": "#ff8c00",
  "dim":    "#005500",
  "danger": "#c25757"
}
```

Wszystkie klucze opcjonalne — brakujące zostają na poprzedniej wartości.
Tylko format `#rrggbb`. Nazwy palet (`"fg"`, `"accent"` itd.) działają jako wartości kolorów w sekcji `widgets`.

### `font`

```
"unscii_8" | "unscii_16" | "montserrat_12" | "montserrat_14" | "montserrat_20"
```

Globalny font dla wszystkich widgetów. Default: `"unscii_8"`.

### `rotation`

```
0 (default) | 90 | 180 | 270
```

Obrót LVGL software rotation. Dotyk obraca się automatycznie.

---

### `widgets`

Style overrides per typ widgetu. Każdy klucz jest opcjonalny — ustawia tylko to co podane, reszta zostaje z domyślnego stylu.

```json
"widgets": {
  "screen":    { ...style_fields... },
  "label":     { ...style_fields... },
  "button":    { ...style_fields..., "pressed_bg": "dim", "pressed_text": "bg" },
  "icon":      { ...style_fields... },
  "ticker":    { ...style_fields... },
  "container": { ...style_fields..., "justify": "start" },
  "sparkline": { ...style_fields... },

  "bar": {
    ...style_fields...,
    "indicator": { ...style_fields... }
  },
  "gauge": {
    ...style_fields...,
    "indicator": { ...style_fields... }
  },
  "spinner": {
    ...style_fields...,
    "indicator": { ...style_fields... }
  },
  "toggle": {
    ...style_fields...,
    "checked": { ...style_fields... }
  },

  "graph": {
    ...style_fields...,
    "style": "bars"
  }
}
```

**`bar.indicator`** — styl wypełnienia paska (`LV_PART_INDICATOR`).
**`gauge.indicator`** — styl łuku wskaźnika.
**`spinner.indicator`** — styl obracającego się łuku.
**`toggle.checked`** — styl nakładany gdy toggle jest w stanie ON.
**`graph.style`** — typ wykresu: `"bars"` | `"line"` | `"area"` | `"symmetric"`.

#### Pełna konfiguracja `graph`

```json
"graph": {
  "style":       "bars",
  "bar_width":   2,
  "bar_gap":     1,
  "line_width":  1,
  "color":       "fg",
  "fill_color":  "dim",
  "fill_opa":    128,
  "grid_h_lines": 2,
  "grid_color":  "dim",
  "y_min":       0,
  "y_max":       100,
  "point_count": 0,
  "color_by_value": 1,
  "thresholds": [
    { "threshold": 50, "color": "#00ff41" },
    { "threshold": 80, "color": "#ff8c00" },
    { "threshold": 100, "color": "#c25757" }
  ],
  "sym_color_by_distance": 1,
  "sym_center_color": "#00ff41",
  "sym_peak_color":   "#ff8c00"
}
```

`point_count: 0` = szerokość widgetu w px (1 punkt = 1 px).
Max 4 progi w `thresholds`.

#### Konfiguracja `sparkline`

```json
"sparkline": {
  "color":       "fg",
  "line_width":  1,
  "y_min":       0,
  "y_max":       100,
  "point_count": 0
}
```

---

### Style fields (wspólne dla `widgets.*` i inline `"style": {}`)

Stosowane przez `ws_parse` / `ws_apply`. Wszystkie opcjonalne.

#### Tło i ramka

| Klucz | Typ | Opis |
|---|---|---|
| `bg_color` | color | kolor tła |
| `bg_opa` | 0–255 | opacity tła |
| `border_color` | color | kolor ramki |
| `border_width` | int | grubość ramki px |
| `border_opa` | 0–255 | opacity ramki |
| `radius` | int | zaokrąglenie narożników px |

#### Padding

| Klucz | Typ | Opis |
|---|---|---|
| `pad_top` | int | |
| `pad_bottom` | int | |
| `pad_left` | int | |
| `pad_right` | int | |
| `pad_all` | int | skrót — ustawia wszystkie 4 strony |
| `pad_gap` | int | odstęp między dziećmi (flex/grid) |

#### Tekst i font

| Klucz | Typ | Opis |
|---|---|---|
| `text_color` | color | kolor tekstu |
| `text_opa` | 0–255 | opacity tekstu |
| `letter_space` | int | odstęp między znakami px |
| `line_space` | int | odstęp między liniami px |
| `font` | string | nazwa fontu (te same co globalne) |
| `text_align` | string | `"left"` \| `"center"` \| `"right"` |

#### Wskaźnik (INDICATOR part)

Pola `fill_*` zawsze trafiają do `LV_PART_INDICATOR` niezależnie od kontekstu — dotyczy zarówno pól w głównym obiekcie stylu jak i w `"indicator": {}`.

| Klucz | Typ | Opis |
|---|---|---|
| `fill_color` | color | bg + arc kolor wskaźnika |
| `fill_opa` | 0–255 | opacity wskaźnika |

#### Łuk (arc / gauge / spinner)

| Klucz | Typ | Opis |
|---|---|---|
| `arc_color` | color | kolor toru łuku (MAIN) |
| `arc_width` | int | grubość łuku px |

#### Linia i wykres

| Klucz | Typ | Opis |
|---|---|---|
| `line_color` | color | kolor linii LVGL (`lv_style_line_color`) |
| `line_width` | int | grubość linii px |

> **Uwaga graph**: `line_color` w inline `"style": {}` na widgecie `graph` trafia do `lv_chart_set_series_color` (nie do LVGL style), bo lv_chart ma własny system kolorów serii. W `theme.json widgets.graph` `line_color` trafia do LVGL style (mniejszy efekt).

#### Przycisk — stany

Używane tylko dla `button`.

| Klucz | Typ | Opis |
|---|---|---|
| `pressed_bg` | color | tło przy STATE_PRESSED |
| `pressed_text` | color | tekst przy STATE_PRESSED |

#### Cień i outline

| Klucz | Typ | Opis |
|---|---|---|
| `shadow_color` | color | |
| `shadow_width` | int | px (0 = wyłączony) |
| `outline_color` | color | zewnętrzna obwódka |
| `outline_width` | int | px |

#### Flex justify (kontenery)

| Klucz | Wartości |
|---|---|
| `justify` | `"start"` \| `"end"` \| `"center"` \| `"space-between"` \| `"space-around"` \| `"space-evenly"` |

Zmienia tylko main axis. Cross axis i track pozostają tak jak ustawił typ layoutu.

#### Wartości kolorów

```
"#rrggbb"          — hex bezpośrednio
"bg"               — paleta bg
"fg"               — paleta fg
"accent"           — paleta accent
"dim"              — paleta dim
"danger"           — paleta danger
```

---

## layout.json

```json
{
  "screens": [
    {
      "id": "main",
      "widgets": [ ...widget_objects... ]
    }
  ]
}
```

Wiele ekranów dozwolone. Pierwszy `screen_mgr_show("main")` wyświetla ekran o id `"main"`.

---

### Wspólne pola widgetów

| Klucz | Typ | Default | Opis |
|---|---|---|---|
| `type` | string | — | **wymagane** |
| `x` | int | 0 | pozycja |
| `y` | int | 0 | pozycja |
| `w` | int | 100 | szerokość px |
| `h` | int | 20 | wysokość px |
| `style` | object | — | inline style override (ws fields) |
| `style.indicator` | object | — | override dla `LV_PART_INDICATOR` |
| `on_tap` | string | — | callback przy kliknięciu |
| `on_hold` | string | — | callback przy długim przytrzymaniu |

`on_tap` / `on_hold` działają na każdym widgecie, nie tylko buttonie.

---

### `label`

```json
{
  "type": "label", "x": 10, "y": 10, "w": 200, "h": 16,
  "bind": { "text": "$cpu_label" },
  "style": { "text_align": "center" }
}
```

| bind key | typ wartości | opis |
|---|---|---|
| `text` | string / `$placeholder` | wyświetlany tekst |

Tryb long: `LV_LABEL_LONG_CLIP` — tekst ucięty, nie zawija się.

---

### `button`

```json
{
  "type": "button", "x": 10, "y": 40, "w": 80, "h": 24,
  "bind": { "label": "NEXT" },
  "on_tap": "reply.media.next",
  "on_hold": "reply.media.prev"
}
```

| bind key | typ wartości | opis |
|---|---|---|
| `label` | string / `$placeholder` | tekst etykiety |

Etykieta wyśrodkowana (`lv_obj_center`). Styl wciśnięcia z `theme.widgets.button.pressed_bg/pressed_text`.

---

### `bar`

```json
{
  "type": "bar", "x": 10, "y": 60, "w": 200, "h": 18,
  "bind": { "value": "$cpu_usage", "max": 100, "label": "$cpu_pct" }
}
```

| bind key | typ wartości | opis |
|---|---|---|
| `value` | int / `$placeholder` | bieżąca wartość (0..max) |
| `max` | int / `$placeholder` | wartość maksymalna (default 100) |
| `label` | string / `$placeholder` | tekst nakładkowy (centrowany, kontrast auto) |

Etykieta rysowana przez custom draw callback — kolor FG na tle pustym, kolor BG na tle wypełnienia.

---

### `graph`

```json
{
  "type": "graph", "x": 0, "y": 80, "w": 240, "h": 60,
  "graph_style": "area",
  "line_width": 2,
  "color": "accent",
  "y_min": 0, "y_max": 100,
  "bind": { "value": "$net_rx" },
  "style": { "line_color": "#ff4400" }
}
```

| bind key | typ wartości | opis |
|---|---|---|
| `value` | float / `$placeholder` | każdy update dodaje nowy punkt i przesuwa historię w lewo |

#### Inline graph config overrides

Pola konfiguracyjne wykresu można nadpisać per-widget — dziedziczą z `theme.widgets.graph`, brakujące zostają z theme.

| Klucz | Typ | Opis |
|---|---|---|
| `graph_style` | string | `"bars"` \| `"line"` \| `"area"` \| `"symmetric"` |
| `bar_width` | int | szerokość słupka px (tylko `bars`) |
| `bar_gap` | int | odstęp między słupkami px (tylko `bars`) |
| `line_width` | int | grubość linii px (tylko `line` / `area`) |
| `grid_lines` | int | liczba poziomych linii siatki |
| `y_min` | int | dolna granica osi Y |
| `y_max` | int | górna granica osi Y |
| `point_count` | int | liczba punktów (0 = szerokość w px) |
| `color` | color | kolor serii |

> `style.line_color` w bloku `"style": {}` też zmienia kolor serii (via `lv_chart_set_series_color`) — działa tak samo jak `color` powyżej, ale przez mechanizm ws_apply.

---

### `sparkline`

```json
{
  "type": "sparkline", "x": 120, "y": 10, "w": 60, "h": 14,
  "bind": { "value": "$cpu_usage" }
}
```

| bind key | typ wartości | opis |
|---|---|---|
| `value` | float / `$placeholder` | jak graph — push kolejnych punktów |

Brak tła, osi, ramki. Do osadzenia inline.

---

### `gauge`

```json
{
  "type": "gauge", "x": 10, "y": 10, "w": 80, "h": 80,
  "bind": { "value": "$gpu_temp", "max": 120, "label": "$gpu_temp" }
}
```

| bind key | typ wartości | opis |
|---|---|---|
| `value` | int / `$placeholder` | bieżąca wartość |
| `max` | int / `$placeholder` | zakres (default 100) |
| `label` | string / `$placeholder` | tekst w centrum łuku |

Geometria łuku: start 135° (lewy dół), sweep 270°. Stały — nieprzekonfiurowalny z JSON.

---

### `spinner`

```json
{
  "type": "spinner", "x": 220, "y": 140, "w": 40, "h": 40
}
```

Brak bind fields. Animacja wewnętrzna LVGL. Prędkość i kąt łuku z domyślnych wartości firmware (1000 ms / 60°).

---

### `icon`

```json
{
  "type": "icon", "x": 5, "y": 5, "w": 14, "h": 14,
  "bind": { "text": "$wifi_icon" }
}
```

| bind key | typ wartości | opis |
|---|---|---|
| `text` | string / `$placeholder` | jeden znak/symbol, wyrównanie centrum |

---

### `image`

```json
{
  "type": "image", "x": 5, "y": 5, "w": 24, "h": 24,
  "src": "logo"
}
```

| Klucz | Typ | Opis |
|---|---|---|
| `src` | string | nazwa assetu; pobierany z `HOST_URL/images/<src>` |

Brak bind — `src` to statyczna nazwa, nie placeholder. Piksele alokowane w PSRAM.

#### Format binarny obrazka (host → device)

```
Bytes 0-1:  szerokość  (uint16, big-endian)
Bytes 2-3:  wysokość   (uint16, big-endian)
Bytes 4+:   piksele RGB565, little-endian (lv_color_t), row-major
```

Rozmiar: `4 + width × height × 2` bajtów. Limit: **512 KB**.

#### `bg_image` — obraz tła (dowolny widget)

```json
{
  "type": "container", "layout": "absolute", "x": 0, "y": 0, "w": 480, "h": 320,
  "bg_image": "wallpaper",
  "children": [ ... ]
}
```

Pole `"bg_image"` dostępne na wszystkich typach **poza** `bar`, `graph`, `button`, `sparkline`, `image` (te mają własny `user_data`). Stosowane przez `lv_obj_set_style_bg_img_src`.

#### Uwagi pamięciowe

| Rozmiar | RAM |
|---|---|
| 100×100 | 20 KB |
| 240×80 | 38 KB |
| 480×320 (full screen) | 300 KB |

Piksele w PSRAM (8 MB Octal @80 MHz, memory-mapped). Descriptor `lv_img_dsc_t` (16 B) w heap. Zwolnienie przy `LV_EVENT_DELETE`.

---

### `container`

```json
{
  "type": "container",
  "layout": "row",
  "x": 0, "y": 280, "w": 480, "h": 36,
  "style": { "justify": "space-between", "pad_all": 0 },
  "children": [
    { "type": "button", "w": 60, "h": 28, "bind": { "label": "A" }, "on_tap": "reply.x.a" },
    { "type": "button", "w": 60, "h": 28, "bind": { "label": "B" }, "on_tap": "reply.x.b" }
  ]
}
```

| Klucz | Wartości | Opis |
|---|---|---|
| `layout` | `"row"` | dzieci poziomo, `cross=CENTER` |
| | `"column"` | dzieci pionowo, `cross=START` |
| | `"grid"` | dzieci w wierszach (flex row wrap), `cross=START` |
| | `"absolute"` | brak layout managera, dzieci z `x`/`y` |

`children` — tablica widgetów, rekurencja. Dzieci nie mają `x`/`y` w trybach `row`/`column`/`grid` (LVGL je ignoruje).

Dla `layout: "grid"` dostępne pole:

| Klucz | Typ | Opis |
|---|---|---|
| `cols` | int | liczba kolumn; szerokość każdego dziecka obliczana automatycznie jako `(container_width - gap*(cols-1)) / cols` |

Przykład siatki 3-kolumnowej:

```json
{
  "type": "container", "layout": "grid", "cols": 3,
  "x": 0, "y": 200, "w": 480, "h": 80,
  "children": [ ... ]
}
```

---

### `ticker`

```json
{
  "type": "ticker", "x": 5, "y": 5, "w": 56, "h": 14,
  "frames": ["|", "/", "-", "\\"],
  "interval_ms": 100
}
```

| Klucz | Typ | Default | Opis |
|---|---|---|---|
| `frames` | string[] | — | sekwencja klatek; max 32 znaki/klatkę |
| `interval_ms` | int | 100 | czas między klatkami ms |

Animacja przez `lv_timer`, bez WebSocket. Timer usuwa się automatycznie przy `LV_EVENT_DELETE`.

---

## Callbacki

Format: `"namespace.akcja"` lub `"namespace.akcja:arg"`

| Callback | Efekt |
|---|---|
| `screen.SCREEN_ID` | Przełącza na ekran o podanym id |
| `reply.EVENT` | Wysyła `{"event":"EVENT"}` przez WebSocket |
| `reply.EVENT:ARG` | Wysyła `{"event":"EVENT","arg":"ARG"}` przez WebSocket |

Nowe namespace rejestruje się przez `callback_register(ns, handler_fn)` w `app_main`.

---

## WebSocket — host → device

```json
{ "cpu_usage": 75, "cpu_label": "75%", "net_rx": "1.2" }
```

- Klucze odpowiadają nazwom placeholderów bez `$`
- Wartości jako stringi lub liczby (oboje parsowane)
- Max 32 znaki na klucz, 32 znaki na wartość
- `"__reload": 1` — wywołuje `esp_restart()`

Każdy klucz w ramce jest niezależny — można wysyłać częściowe aktualizacje.

---

## Kolejność priorytetów stylów (od najniższego do najwyższego)

1. Domyślny styl widgetu z `theme_build_styles()` (budowany z palety)
2. `theme.widgets.TYPE.*` — override z theme.json (`apply_theme_ws`)
3. `layout.widgets[i].style.*` — inline override z layout.json (`ws_apply`)
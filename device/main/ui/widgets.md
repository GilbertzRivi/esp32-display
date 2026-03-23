# Dokumentacja widgetow

Wszystkie widgety to prymitywy wbudowane w firmware. Host definiuje ekrany w layout JSON, a device instancjonuje i binduje dane przez WebSocket — bez potrzeby reflashowania.

## Konwencja set_field

Kazdy widget z bindowalnymi polami implementuje:

```c
void w_<typ>_set_field(lv_obj_t *obj, int field_id, const char *val);
```

To jest jedyna funkcja wywolywana przez `placeholder.c` gdy WebSocket dostarcza nowe dane. `val` zawsze jest stringiem — widget konwertuje sam (`atoi`/`atof`).

---

## label

Wyswietla tekst. Statyczny lub z placeholdera WebSocket.

### API

```c
lv_obj_t *w_label_create(lv_obj_t *parent, int x, int y, int w, int h);
void w_label_set_text(lv_obj_t *obj, const char *text);
void w_label_set_field(lv_obj_t *obj, int field_id, const char *val);
```

### Pola

| ID | Stala | Typ | Opis |
|----|-------|-----|------|
| 0 | `W_LABEL_TEXT` | string | Wyswietlany tekst |

### Parametry theme

| Parametr | Klucz JSON | Opis |
|----------|-----------|------|
| kolor tekstu | `"fg"` z palety | THEME_FG |
| tlo | `"bg"` z palety | THEME_BG, przezroczyste |
| font | `"font"` w theme | Globalny font theme |

### Layout JSON

```json
{
  "id": "cpu_label",
  "type": "label",
  "layout": "absolute",
  "x": 10, "y": 10, "w": 200, "h": 20,
  "bind": { "text": "$cpu_label" }
}
```

Tekst statyczny (bez placeholdera):

```json
{ "type": "label", "x": 0, "y": 0, "w": 120, "h": 16,
  "bind": { "text": "CPU Usage:" } }
```

---

## button

Klikalny obszar z etykieta. Wyzwala callbacki `on_tap` i `on_hold`. Nie przechowuje stanu.

### API

```c
lv_obj_t *w_button_create(lv_obj_t *parent, int x, int y, int w, int h);
void w_button_set_label(lv_obj_t *obj, const char *text);

/* C callbacki — dla kodu firmware */
void w_button_set_on_tap(lv_obj_t *obj, lv_event_cb_t cb, void *user_data);
void w_button_set_on_hold(lv_obj_t *obj, lv_event_cb_t cb, void *user_data);

/* String actions — dla widget_factory (layout JSON) */
void w_button_set_tap_action(lv_obj_t *obj, const char *action);
void w_button_set_hold_action(lv_obj_t *obj, const char *action);

void w_button_set_field(lv_obj_t *obj, int field_id, const char *val);
```

`on_tap` / `tap_action` = `LV_EVENT_CLICKED`. `on_hold` / `hold_action` = `LV_EVENT_LONG_PRESSED`.

String actions sa dispatchowane przez `callback_dispatch` — patrz sekcja **Callback system** ponizej.

### Pola

| ID | Stala | Typ | Opis |
|----|-------|-----|------|
| 0 | `W_BUTTON_LABEL` | string | Tekst etykiety |

### Parametry theme

| Parametr | Pole w `theme_data_t` | Opis |
|----------|-----------------------|------|
| padding | `button_pad` | Wewnetrzny odstep od krawedzi |
| grubosc bordera | `button_border_w` | 0 = brak bordera |
| kolor etykiety | `palette[THEME_BG]` | Kontrastuje z tlem przycisku |
| kolor tla | `palette[THEME_ACCENT]` | Tlo w stanie normalnym |
| kolor tla wcisniety | `palette[THEME_DIM]` | Tlo podczas dotykania |

### Layout JSON

```json
{
  "id": "next_btn",
  "type": "button",
  "layout": "absolute",
  "x": 200, "y": 280, "w": 80, "h": 30,
  "bind": { "label": "NEXT" },
  "on_tap": "reply.media.next",
  "on_hold": "reply.media.next_album"
}
```

---

## bar

Pasek postepu poziomy. `value` i `max` wyznaczaja wypelnienie. Opcjonalna etykieta nad paskiem.

### API

```c
lv_obj_t *w_bar_create(lv_obj_t *parent, int x, int y, int w, int h);
void w_bar_set_value(lv_obj_t *obj, int value, int max);
void w_bar_set_label(lv_obj_t *obj, const char *text);
void w_bar_set_field(lv_obj_t *obj, int field_id, const char *val);
```

### Pola

| ID | Stala | Typ | Opis |
|----|-------|-----|------|
| 0 | `W_BAR_VALUE` | int | Aktualna wartosc |
| 1 | `W_BAR_MAX` | int | Wartosc maksymalna |
| 2 | `W_BAR_LABEL` | string | Tekst nad paskiem |

Jesli `max` jest stala w `bind` (nie placeholderem), nadal mozna ustawic przez `W_BAR_MAX`.

### Parametry theme

| Parametr | Opis |
|----------|------|
| kolor sciezki | THEME_DIM — tlo wypelnienia |
| kolor wypelnienia | THEME_FG |
| kolor etykiety | THEME_FG |
| grubosc bordera | `bar_border_w` w `theme_data_t` |

### Layout JSON

```json
{
  "id": "cpu_bar",
  "type": "bar",
  "layout": "absolute",
  "x": 0, "y": 40, "w": 240, "h": 20,
  "bind": { "value": "$cpu_usage", "max": 100, "label": "$cpu_pct" }
}
```

---

## graph

Wykres z przewijaniem. Kazdy update WebSocket dodaje punkt z prawej i przesuwa historie w lewo. Cztery style renderowania.

### API

```c
lv_obj_t *w_graph_create(lv_obj_t *parent, int x, int y, int w, int h,
                          const graph_config_t *cfg);
void w_graph_set_range(lv_obj_t *obj, int min, int max);
void w_graph_push(lv_obj_t *obj, float value);
void w_graph_set_field(lv_obj_t *obj, int field_id, const char *val);
```

`cfg = NULL` oznacza uzycie `theme_graph()` jako konfiguracji.

Pattern nadpisywania per-widget:

```c
graph_config_t cfg = *theme_graph();
cfg.style     = GRAPH_STYLE_BARS;
cfg.bar_width = 3;
cfg.color     = lv_color_hex(0xff2222);
lv_obj_t *g = w_graph_create(parent, x, y, w, h, &cfg);
```

### Pola

| ID | Stala | Typ | Opis |
|----|-------|-----|------|
| 0 | `W_GRAPH_VALUE` | float | Dodaje nowy punkt do historii |

### graph_config_t

Zdefiniowana w `theme.h`. Przekazywana do `w_graph_create` i do `theme_data_t.graph` jako domyslna.

| Pole | Typ | Opis |
|------|-----|------|
| `style` | `graph_style_t` | `LINE`, `AREA`, `BARS`, `SYMMETRIC` |
| `bar_width` | int | Szerokosc slupka w px (BARS) |
| `bar_gap` | int | Odstep miedzy slupkami w px (BARS) |
| `line_width` | int | Grubosc linii w px (LINE, AREA) |
| `color` | `lv_color_t` | Kolor slupka lub linii |
| `fill_color` | `lv_color_t` | Kolor wypelnienia pod linia (AREA) |
| `fill_opa` | uint8_t | Przezroczystosc wypelnienia — LV_OPA_* |
| `grid_h_lines` | int | Liczba poziomych linii siatki; 0 = brak |
| `grid_color` | `lv_color_t` | Kolor siatki |
| `color_by_value` | bool | Wlacza kolorowanie per slupek/punkt wg progow |
| `thresholds[]` | `graph_threshold_t[4]` | Progi kolorow: `{int threshold, lv_color_t color}` |
| `threshold_count` | int | Liczba aktywnych progow (max 4) |
| `sym_color_by_distance` | bool | Gradient od centrum do krawedzi (SYMMETRIC) |
| `sym_center_color` | `lv_color_t` | Kolor przy centrum (SYMMETRIC) |
| `sym_peak_color` | `lv_color_t` | Kolor na krawedziach (SYMMETRIC) |
| `y_min` | int | Minimum osi Y |
| `y_max` | int | Maksimum osi Y |
| `point_count` | int | Liczba punktow w buforze; 0 = auto (z `w` widgetu) |

### Style renderowania

**`GRAPH_STYLE_LINE`** — linia bez wypelnienia. Jeden punkt = jedna probka.

**`GRAPH_STYLE_AREA`** — linia + wypelnienie pod kreska. `fill_color` i `fill_opa` steruja wyglad wypelnienia.

**`GRAPH_STYLE_BARS`** — pionowe slupki jak w btop. Liczba slupkow = `w / (bar_width + bar_gap)` jesli `point_count = 0`. Minimum 1px szerokosc.

**`GRAPH_STYLE_SYMMETRIC`** — symetryczny od srodka — do waveformow audio lub sygnalow symetrycznych. Uzywa `lv_canvas` z wlasnym buforem pikseli. Wartosc 0 = tylko centrum; wartosc `y_max` = pelna szerokosc. `sym_color_by_distance` wlacza gradient centrum → krawedz.

### color_by_value

Gdy `color_by_value = true`, kolor slupka lub punktu linii jest dobierany przez posortowana tablice progow. Wartosci <= prog[i].threshold dostaja `thresholds[i].color`. Wartosc powyzej wszystkich progow dostaje `color` (base).

Przyklad: zielony <= 60, pomaranczowy <= 85, czerwony powyzej:

```c
cfg.color_by_value = true;
cfg.threshold_count = 2;
cfg.thresholds[0] = (graph_threshold_t){ .threshold = 60, .color = lv_color_hex(0x00ff41) };
cfg.thresholds[1] = (graph_threshold_t){ .threshold = 85, .color = lv_color_hex(0xff8c00) };
cfg.color = lv_color_hex(0xff2222);  // powyzej 85
```

### Layout JSON

```json
{
  "id": "net_graph",
  "type": "graph",
  "layout": "absolute",
  "x": 0, "y": 60, "w": 240, "h": 80,
  "bind": { "value": "$net_rx_bytes" }
}
```

---

## sparkline

Kompaktowy wykres bez osi, etykiet i tla. Przeznaczony do osadzania inline obok labelow.

### API

```c
lv_obj_t *w_sparkline_create(lv_obj_t *parent, int x, int y, int w, int h,
                               const sparkline_config_t *cfg);
void w_sparkline_push(lv_obj_t *obj, float value);
void w_sparkline_set_range(lv_obj_t *obj, int min, int max);
void w_sparkline_set_field(lv_obj_t *obj, int field_id, const char *val);
```

`cfg = NULL` oznacza uzycie `theme_sparkline()` jako konfiguracji.

### Pola

| ID | Stala | Typ | Opis |
|----|-------|-----|------|
| 0 | `W_SPARKLINE_VALUE` | float | Dodaje nowy punkt do historii |

### sparkline_config_t

Zdefiniowana w `theme.h`.

| Pole | Typ | Opis |
|------|-----|------|
| `color` | `lv_color_t` | Kolor linii |
| `line_width` | int | Grubosc linii w px; 0 = 1px |
| `y_min` | int | Minimum osi Y |
| `y_max` | int | Maksimum osi Y |
| `point_count` | int | Liczba punktow; 0 = auto (z `w` widgetu) |

Pattern nadpisywania:

```c
sparkline_config_t cfg = *theme_sparkline();
cfg.color = lv_color_hex(0xff2222);
cfg.y_max = 200;
lv_obj_t *s = w_sparkline_create(parent, x, y, 60, 16, &cfg);
```

### Layout JSON

```json
{
  "id": "cpu_spark",
  "type": "sparkline",
  "layout": "absolute",
  "x": 120, "y": 10, "w": 60, "h": 16,
  "bind": { "value": "$cpu_usage" }
}
```

---

## gauge

Wskaznik kolowy — luk od 0 do `max`, wypelniony do `value`. Opcjonalna etykieta w centrum.

### API

```c
lv_obj_t *w_gauge_create(lv_obj_t *parent, int x, int y, int w, int h,
                          const w_gauge_config_t *cfg);
void w_gauge_set_value(lv_obj_t *obj, int value, int max);
void w_gauge_set_label(lv_obj_t *obj, const char *text);
void w_gauge_set_field(lv_obj_t *obj, int field_id, const char *val);
```

### Pola

| ID | Stala | Typ | Opis |
|----|-------|-----|------|
| 0 | `W_GAUGE_VALUE` | int | Aktualna wartosc |
| 1 | `W_GAUGE_MAX` | int | Wartosc maksymalna |
| 2 | `W_GAUGE_LABEL` | string | Tekst w centrum |

### w_gauge_config_t

| Pole | Typ | Opis | Domyslnie |
|------|-----|------|-----------|
| `rotation` | int | Poczatek luku w stopniach od godziny 12 | 135 (lewy dolny rog) |
| `sweep` | int | Dlugosc luku w stopniach | 270 |

`cfg = NULL` uzywa `W_GAUGE_CONFIG_DEFAULT = {135, 270}`.

### Parametry theme

| Parametr | Opis |
|----------|------|
| kolor luku wypelnionego | THEME_ACCENT |
| kolor luku tla | THEME_DIM |
| kolor etykiety | THEME_FG |
| grubosc luku | `max(2, min(w,h) / 10)` — skaluje sie z rozmiarem widgetu |

### Layout JSON

```json
{
  "id": "temp_gauge",
  "type": "gauge",
  "layout": "absolute",
  "x": 10, "y": 10, "w": 100, "h": 100,
  "bind": { "value": "$gpu_temp", "max": 100, "label": "$gpu_temp" }
}
```

Niestandardowa geometria luku:

```json
{ "type": "gauge", "x": 0, "y": 0, "w": 80, "h": 80,
  "rotation": 180, "sweep": 180,
  "bind": { "value": "$vol", "max": 100 } }
```

---

## spinner

Animowany wskaznik zajetosci. Nie przyjmuje danych z WebSocket. Animacja dziala przez LVGL wewnetrznie.

### API

```c
lv_obj_t *w_spinner_create(lv_obj_t *parent, int x, int y, int w, int h);
```

Parametry animacji sa odczytywane z theme przy tworzeniu — LVGL 8 nie udostepnia setterow po utworzeniu spinnera.

### Parametry theme

| Parametr | Pole w `theme_data_t` | Opis |
|----------|-----------------------|------|
| predkosc animacji | `spinner_speed_ms` | Czas pelnego obrotu w ms |
| dlugosc luku | `spinner_arc_deg` | Kat widocznego luku w stopniach |
| kolor | THEME_ACCENT | |

### Layout JSON

```json
{
  "id": "loading",
  "type": "spinner",
  "layout": "absolute",
  "x": 220, "y": 140, "w": 40, "h": 40
}
```

---

## icon

Pojedynczy znak lub krotki symbol renderowany przez `lv_label`. Wyrownanie do centrum. Sluzy jako dekorator stanu lub wskaznik.

### API

```c
lv_obj_t *w_icon_create(lv_obj_t *parent, int x, int y, int w, int h);
void w_icon_set_text(lv_obj_t *obj, const char *text);
void w_icon_set_field(lv_obj_t *obj, int field_id, const char *val);
```

### Pola

| ID | Stala | Typ | Opis |
|----|-------|-----|------|
| 0 | `W_ICON_TEXT` | string | Wyswietlany symbol |

### Parametry theme

Kolor: THEME_ACCENT. Font: globalny font theme.

### Layout JSON

```json
{
  "id": "wifi_icon",
  "type": "icon",
  "layout": "absolute",
  "x": 460, "y": 2, "w": 16, "h": 16,
  "bind": { "text": "$wifi_icon" }
}
```

Statyczny:

```json
{ "type": "icon", "x": 5, "y": 5, "w": 16, "h": 16,
  "bind": { "text": "!" } }
```

---

## image

Mala bitmapa z assetu theme. W biezacej implementacji (M2) renderuje placeholder `[img]` z borderem. M3 zaimplementuje ladowanie assetow z sekcji `"assets"` w theme JSON.

### API

```c
lv_obj_t *w_image_create(lv_obj_t *parent, int x, int y, int w, int h);
```

Brak `set_field` do czasu implementacji assetow w M3.

### Layout JSON (M3)

```json
{
  "id": "logo",
  "type": "image",
  "layout": "absolute",
  "x": 5, "y": 5, "w": 24, "h": 24,
  "bind": { "src": "logo" }
}
```

Theme JSON musi zawierac sekcje `"assets"`:

```json
{ "assets": { "logo": "<base64-encoded bitmap>" } }
```

---

## container

Grupuje dzieci i stosuje do nich tryb layoutu. Sam jest pozycjonowany absolutnie na ekranie. Nie ma pol bindowalnych.

### API

```c
lv_obj_t *w_container_create(lv_obj_t *parent, int x, int y, int w, int h,
                               const char *layout);

/* Odstep miedzy dziecmi */
void w_container_set_gap(lv_obj_t *obj, int gap);

/* Wewnetrzny padding (miedzy borderem a dziecmi) */
void w_container_set_pad(lv_obj_t *obj, int top, int right, int bottom, int left);

```

### Tryby layoutu

| Wartosc `layout` | Opis |
|------------------|------|
| `"row"` | Dzieci ulozone poziomo od lewej |
| `"column"` | Dzieci ulozone pionowo od gory |
| `"grid"` | Dzieci w wierszach (flex row-wrap) |
| `"absolute"` | Brak managera layoutu — dzieci pozycjonowane recznie |

Odstep miedzy dziecmi: `theme_container_gap()` jako domyslny, nadpisywalny przez `w_container_set_gap`.

### Parametry theme

| Parametr | Pole w `theme_data_t` | Opis |
|----------|-----------------------|------|
| odstep miedzy dziecmi | `container_gap` | W px |
| tlo | THEME_BG | |

### Layout JSON

```json
{
  "id": "nav_row",
  "type": "container",
  "layout": "row",
  "x": 0, "y": 290, "w": 480, "h": 30,
  "children": [
    { "id": "b_prev", "type": "button", "bind": { "label": "PREV" }, "on_tap": "reply.media.prev" },
    { "id": "b_next", "type": "button", "bind": { "label": "NEXT" }, "on_tap": "reply.media.next" }
  ]
}
```

Grid z kolumnami:

```json
{ "type": "container", "layout": "grid", "cols": 3,
  "x": 0, "y": 0, "w": 480, "h": 200,
  "children": [ ... ] }
```

---

## ticker

Sekwencja stringow cyklowana lokalnie przez `lv_timer`. Bez WebSocket, bez hosta. Uzywany do animowanych wskaznikow aktywnosci i dekoracji ASCII.

`w_ticker_start` kopiuje tablice ramek wewnetrznie — caller nie musi trzymac tablicy przy zyciu. Timer jest usuwany automatycznie przy niszczeniu widgetu (`LV_EVENT_DELETE`).

### API

```c
lv_obj_t *w_ticker_create(lv_obj_t *parent, int x, int y, int w, int h);
void w_ticker_start(lv_obj_t *obj, const char **frames, int count, int interval_ms);
void w_ticker_stop(lv_obj_t *obj);
```

### Layout JSON

Prosty wskaznik aktywnosci:

```json
{
  "id": "spinner_txt",
  "type": "ticker",
  "layout": "absolute",
  "x": 5, "y": 5, "w": 20, "h": 16,
  "frames": ["|", "/", "-", "\\"],
  "interval_ms": 100
}
```

Animowany pasek ladowania:

```json
{ "type": "ticker", "x": 0, "y": 0, "w": 60, "h": 16,
  "frames": ["[    ]", "[=   ]", "[==  ]", "[=== ]", "[====]", "[ ===]", "[  ==]", "[   =]"],
  "interval_ms": 80 }
```

Brak bindowalnych pol — animacja jest w pelni zdefiniowana w layout JSON.

---

---

## value

Wyswietla zmieniajaca sie wartosc — semantycznie odroznia dane dynamiczne od statycznych etykiet. Identyczna implementacja jak `label`, ale styl ACCENT zamiast FG.

### API

```c
lv_obj_t *w_value_create(lv_obj_t *parent, int x, int y, int w, int h);
void w_value_set_text(lv_obj_t *obj, const char *text);
void w_value_set_field(lv_obj_t *obj, int field_id, const char *val);
```

### Pola

| ID | Stala | Typ | Opis |
|----|-------|-----|------|
| 0 | `W_VALUE_TEXT` | string | Wyswietlana wartosc |

### Layout JSON

```json
{
  "type": "value",
  "x": 120, "y": 10, "w": 80, "h": 16,
  "bind": { "text": "$cpu_usage_str" }
}
```

Typowy pattern — label statyczny obok value dynamicznego:

```json
{ "type": "container", "layout": "row", "x": 0, "y": 10, "w": 200, "h": 16,
  "children": [
    { "type": "label", "bind": { "text": "CPU:" } },
    { "type": "value", "bind": { "text": "$cpu_pct" } }
  ] }
```

---

## toggle

Przelacznik dwustanowy (`lv_switch`). Przy zmianie stanu przez uzytkownika dispatchuje przypisana akcje. Programowa zmiana stanu przez WebSocket nie wyzwala akcji.

### API

```c
lv_obj_t *w_toggle_create(lv_obj_t *parent, int x, int y, int w, int h);
void w_toggle_set_actions(lv_obj_t *obj, const char *on_action, const char *off_action);
void w_toggle_set_state(lv_obj_t *obj, bool state);
void w_toggle_set_field(lv_obj_t *obj, int field_id, const char *val);
```

### Pola

| ID | Stala | Typ | Opis |
|----|-------|-----|------|
| 0 | `W_TOGGLE_STATE` | int (0/1) | Ustawia stan bez wyzwalania akcji |

### Parametry theme

| Czesc | Kolor |
|-------|-------|
| track (wylaczony) | THEME_DIM |
| track (wlaczony) | THEME_ACCENT |
| knob | THEME_FG |

### Layout JSON

```json
{
  "id": "light_sw",
  "type": "toggle",
  "layout": "absolute",
  "x": 10, "y": 10, "w": 50, "h": 24,
  "on_action": "reply.light.on",
  "off_action": "reply.light.off",
  "bind": { "state": "$light_state" }
}
```

---

## Callback system

Akcje w layout JSON sa stringami w formacie `<namespace>.<args>`. Dispatcher (`callback.c`) rozbija string na namespace i args, znajduje zarejestrowany handler i wywoluje go.

```c
void callback_init(void);
void callback_register(const char *ns, callback_handler_t handler);
void callback_dispatch(const char *action);
```

Wbudowane namespaces:

| Namespace | Przyklad | Efekt |
|-----------|---------|-------|
| `screen` | `"screen.main"` | `screen_mgr_show("main")` |
| `reply` | `"reply.media.next"` | Wysyla event przez WebSocket (M3) |

Dodawanie wlasnych namespace:

```c
void my_handler(const char *args) { /* ... */ }
callback_register("myns", my_handler);
/* "myns.foo" -> my_handler("foo") */
```

---

## Tabela podsumowujaca

| Widget | Bindowalne pola | Config per-widget | Notatki |
|--------|----------------|-------------------|---------|
| `label` | `text` | — | tekst statyczny lub z placeholdera |
| `value` | `text` | — | jak label, kolor ACCENT — dla danych dynamicznych |
| `button` | `label` | — | on_tap/on_hold C lub string action |
| `toggle` | `state` (0/1) | — | dispatch on_action / off_action przy tapie |
| `bar` | `value`, `max`, `label` | — | |
| `graph` | `value` | `graph_config_t` | 4 style, color_by_value, symmetric gradient |
| `sparkline` | `value` | `sparkline_config_t` | brak osi i tla |
| `gauge` | `value`, `max`, `label` | `w_gauge_config_t` | rotation, sweep |
| `spinner` | — | parametry z theme | LVGL nie pozwala zmienic po utworzeniu |
| `icon` | `text` | — | |
| `image` | `src` (M3) | — | M2: placeholder [img] |
| `container` | — | layout string | grupuje dzieci |
| `ticker` | — | frames[], interval_ms | lokalny timer, bez WebSocket |

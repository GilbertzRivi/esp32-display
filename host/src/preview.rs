use eframe::egui::{self, Color32, Pos2, Rect, Rounding, Stroke, Vec2};
use serde_json::{json, Value};
use std::collections::{HashMap, VecDeque};

pub struct PreviewApp {
    pub artifacts: crate::artifacts::Artifacts,
    pub state: crate::state::State,
    pub history: HashMap<String, VecDeque<f32>>,
    pub last_history_tick: std::time::Instant,
    pub current_screen: String,
    pub event_tx: std::sync::mpsc::SyncSender<String>,
    pub reload_tx: tokio::sync::broadcast::Sender<()>,
    pub layout_path: String,
    pub theme_path: String,
    pub images_dir: String,
    pub image_cache: HashMap<String, egui::TextureHandle>,
}

impl eframe::App for PreviewApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        let now = std::time::Instant::now();
        if now.duration_since(self.last_history_tick) >= std::time::Duration::from_secs(1) {
            self.last_history_tick = now;
            let snapshot = self.state.read().unwrap().clone();
            for (k, v) in &snapshot {
                if let Ok(f) = v.parse::<f32>() {
                    let buf = self.history.entry(k.clone()).or_default();
                    buf.push_back(f);
                    if buf.len() > 512 {
                        buf.pop_front();
                    }
                }
            }
        }

        ctx.request_repaint_after(std::time::Duration::from_millis(100));

        egui::SidePanel::right("state_panel")
            .exact_width(240.0)
            .show(ctx, |ui| self.render_state_panel(ui));

        egui::CentralPanel::default().show(ctx, |ui| self.render_canvas(ui));
    }
}

impl PreviewApp {
    fn render_canvas(&mut self, ui: &mut egui::Ui) {
        let avail = ui.available_size();
        let scale = ((avail.x / 480.0).min((avail.y - 24.0) / 320.0)).max(0.25);
        let canvas_w = 480.0 * scale;
        let canvas_h = 320.0 * scale;
        let ctx = ui.ctx().clone();
        let time = ctx.input(|i| i.time);

        ui.label(format!("Preview  480×320  @ {:.2}×", scale));

        let (rect, response) =
            ui.allocate_exact_size(Vec2::new(canvas_w, canvas_h), egui::Sense::click());
        let click_pos = if response.clicked() {
            response.interact_pointer_pos()
        } else {
            None
        };
        let painter = ui.painter_at(rect);

        let theme_str = self.artifacts.get_theme();
        let theme: Value = serde_json::from_str(&theme_str).unwrap_or(Value::Null);
        let palette = parse_palette_val(&theme);
        let state = self.state.read().unwrap().clone();

        let bg = palette
            .get("bg")
            .copied()
            .unwrap_or(Color32::from_rgb(26, 26, 26));
        painter.rect_filled(rect, Rounding::ZERO, bg);

        let layout = self.artifacts.get_layout();
        let images_dir = self.images_dir.clone();
        if let Ok(layout_val) = serde_json::from_str::<Value>(&layout) {
            if let Some(screens) = layout_val["screens"].as_array() {
                let screen = screens
                    .iter()
                    .find(|s| s["id"].as_str() == Some(&self.current_screen))
                    .or_else(|| screens.first());

                if let Some(screen) = screen {
                    if let Some(widgets) = screen["widgets"].as_array() {
                        for widget in widgets {
                            if let Some(on_tap) = render_widget(
                                widget,
                                &painter,
                                rect.min,
                                scale,
                                &palette,
                                &state,
                                &self.history,
                                time,
                                click_pos,
                                &theme,
                                &ctx,
                                &mut self.image_cache,
                                &images_dir,
                            ) {
                                self.handle_tap(&on_tap);
                            }
                        }
                    }
                }
            }
        }
    }

    fn handle_tap(&mut self, on_tap: &str) {
        if let Some(target) = on_tap.strip_prefix("screen.") {
            self.current_screen = target.to_string();
        } else if let Some(event) = on_tap.strip_prefix("reply.") {
            let ev = event.split_once(':').map(|(e, _)| e).unwrap_or(event);
            let _ = self.event_tx.try_send(ev.to_string());
        }
    }

    fn render_state_panel(&mut self, ui: &mut egui::Ui) {
        ui.horizontal(|ui| {
            if ui.button("Reload").clicked() {
                self.artifacts.reload(&self.layout_path, &self.theme_path);
            }
            if ui.button("Reload Device").clicked() {
                let _ = self.reload_tx.send(());
            }
        });
        ui.separator();

        egui::ScrollArea::vertical()
            .id_salt("side_scroll")
            .show(ui, |ui| {
                ui.label("State");
                ui.add_space(4.0);

                let state = self.state.read().unwrap().clone();
                let mut keys: Vec<_> = state.keys().cloned().collect();
                keys.sort();

                for key in keys {
                    let val = state.get(&key).cloned().unwrap_or_default();
                    ui.horizontal(|ui| {
                        ui.label(format!("• {key}"));
                        ui.with_layout(
                            egui::Layout::right_to_left(egui::Align::Center),
                            |ui| {
                                ui.monospace(&val);
                            },
                        );
                    });
                }

                ui.add_space(8.0);
                ui.separator();
                ui.add_space(4.0);

                let theme_str = self.artifacts.get_theme();
                let theme = parse_palette(&theme_str);
                ui.label("Palette:");
                ui.horizontal_wrapped(|ui| {
                    let mut entries: Vec<_> = theme.iter().collect();
                    entries.sort_by_key(|(k, _)| k.as_str());
                    for (name, color) in entries {
                        ui.vertical(|ui| {
                            let (r, _) =
                                ui.allocate_exact_size(Vec2::new(24.0, 24.0), egui::Sense::hover());
                            ui.painter().rect_filled(r, Rounding::ZERO, *color);
                            ui.small(name);
                        });
                    }
                });
            }); // ScrollArea
    }
}

// ── Font resolution ───────────────────────────────────────────────────────────

fn font_size_from_name(name: &str) -> f32 {
    // LVGL bitmap font height in device pixels.
    // montserrat_N: N = line height; visible cap height ≈ N*0.7.
    // unscii_N: N = exact pixel height (pixel art font).
    match name {
        "unscii_8"       => 8.0,
        "unscii_16"      => 16.0,
        "montserrat_10"  => 10.0,
        "montserrat_12"  => 12.0,
        "montserrat_14"  => 14.0,
        "montserrat_16"  => 16.0,
        "montserrat_18"  => 18.0,
        "montserrat_20"  => 20.0,
        "montserrat_24"  => 24.0,
        _                => 8.0,
    }
}

fn font_id_from_name(_name: &str, size_px: f32) -> egui::FontId {
    egui::FontId::monospace(size_px)
}

fn theme_base_font_name<'a>(theme: &'a Value) -> &'a str {
    theme["font"].as_str().unwrap_or("unscii_8")
}

fn theme_base_font(theme: &Value) -> f32 {
    font_size_from_name(theme_base_font_name(theme))
}

fn widget_font_name<'a>(theme: &'a Value, wtype: &str) -> Option<&'a str> {
    theme["widgets"][wtype]["font"].as_str()
}

/// Returns the egui FontId for a widget type, using correct proportional vs monospace.
fn widget_font_id(theme: &Value, wtype: &str, scale: f32) -> egui::FontId {
    let base_name = theme_base_font_name(theme);
    let name = widget_font_name(theme, wtype).unwrap_or(base_name);
    let size = font_size_from_name(name) * scale;
    font_id_from_name(name, size)
}

// ── Style resolution helpers ──────────────────────────────────────────────────

fn style_color(style: &Value, key: &str, palette: &HashMap<String, Color32>) -> Option<Color32> {
    let s = style[key].as_str()?;
    if s.starts_with('#') {
        parse_hex_color(s)
    } else {
        palette.get(s).copied()
    }
}

fn style_f32(style: &Value, key: &str) -> Option<f32> {
    style[key].as_f64().map(|v| v as f32)
}

fn style_pad(style: &Value, key: &str, scale: f32) -> f32 {
    style[key].as_f64().unwrap_or(0.0) as f32 * scale
}

/// Resolve a color: per-widget inline style → theme widget defaults → fallback.
fn resolve_color(
    inline: &Value,
    theme_ws: &Value,
    key: &str,
    palette: &HashMap<String, Color32>,
    fallback: Color32,
) -> Color32 {
    style_color(inline, key, palette)
        .or_else(|| style_color(theme_ws, key, palette))
        .unwrap_or(fallback)
}

/// Same for the nested `indicator` sub-object.
fn resolve_indicator_color(
    inline: &Value,
    theme_ws: &Value,
    key: &str,
    palette: &HashMap<String, Color32>,
    fallback: Color32,
) -> Color32 {
    style_color(&inline["indicator"], key, palette)
        .or_else(|| style_color(&theme_ws["indicator"], key, palette))
        .unwrap_or(fallback)
}

/// Resolve an f32: inline → theme widget defaults → fallback.
fn resolve_f32(inline: &Value, theme_ws: &Value, key: &str, fallback: f32) -> f32 {
    style_f32(inline, key)
        .or_else(|| style_f32(theme_ws, key))
        .unwrap_or(fallback)
}

fn resolve_indicator_f32(inline: &Value, theme_ws: &Value, key: &str, fallback: f32) -> f32 {
    style_f32(&inline["indicator"], key)
        .or_else(|| style_f32(&theme_ws["indicator"], key))
        .unwrap_or(fallback)
}

// ── Palette ───────────────────────────────────────────────────────────────────

fn parse_palette(theme_json: &str) -> HashMap<String, Color32> {
    let val: Value = serde_json::from_str(theme_json).unwrap_or(Value::Null);
    parse_palette_val(&val)
}

fn parse_palette_val(theme: &Value) -> HashMap<String, Color32> {
    let mut map = HashMap::new();
    if let Some(palette) = theme["palette"].as_object() {
        for (k, v) in palette {
            if let Some(hex) = v.as_str() {
                if let Some(color) = parse_hex_color(hex) {
                    map.insert(k.clone(), color);
                }
            }
        }
    }
    map
}

fn parse_hex_color(hex: &str) -> Option<Color32> {
    let hex = hex.trim_start_matches('#');
    if hex.len() == 6 {
        let r = u8::from_str_radix(&hex[0..2], 16).ok()?;
        let g = u8::from_str_radix(&hex[2..4], 16).ok()?;
        let b = u8::from_str_radix(&hex[4..6], 16).ok()?;
        Some(Color32::from_rgb(r, g, b))
    } else {
        None
    }
}

// ── Bind / value resolution ───────────────────────────────────────────────────

fn resolve_bind(val: &Value, key: &str, state: &HashMap<String, String>) -> String {
    let raw = val[key].as_str().unwrap_or("");
    if !raw.contains('$') {
        return raw.to_string();
    }

    let mut out = String::new();
    let mut chars = raw.chars().peekable();

    while let Some(c) = chars.next() {
        if c == '$' {
            let mut name = String::new();
            while let Some(&ch) = chars.peek() {
                if ch.is_alphanumeric() || ch == '_' {
                    name.push(ch);
                    chars.next();
                } else {
                    break;
                }
            }
            out.push_str(state.get(&name).map(|s| s.as_str()).unwrap_or("?"));
        } else {
            out.push(c);
        }
    }

    out
}

fn resolve_bind_f32(val: &Value, key: &str, state: &HashMap<String, String>) -> f32 {
    if let Some(n) = val[key].as_f64() {
        return n as f32;
    }
    resolve_bind(val, key, state).parse().unwrap_or(0.0)
}

fn widget_rect(widget: &Value, origin: Pos2, scale: f32) -> Rect {
    let x = widget["x"].as_f64().unwrap_or(0.0) as f32 * scale + origin.x;
    let y = widget["y"].as_f64().unwrap_or(0.0) as f32 * scale + origin.y;
    let w = widget["w"].as_f64().unwrap_or(80.0) as f32 * scale;
    let h = widget["h"].as_f64().unwrap_or(20.0) as f32 * scale;
    Rect::from_min_size(Pos2::new(x, y), Vec2::new(w, h))
}

// ── Widget renderer ───────────────────────────────────────────────────────────

#[allow(clippy::too_many_arguments)]
fn render_widget(
    widget: &Value,
    painter: &egui::Painter,
    origin: Pos2,
    scale: f32,
    palette: &HashMap<String, Color32>,
    state: &HashMap<String, String>,
    history: &HashMap<String, VecDeque<f32>>,
    time: f64,
    click_pos: Option<Pos2>,
    theme: &Value,
    ctx: &egui::Context,
    image_cache: &mut HashMap<String, egui::TextureHandle>,
    images_dir: &str,
) -> Option<String> {
    let kind = widget["type"].as_str().unwrap_or("");
    let bind = &widget["bind"];
    // Per-widget inline style overrides (not in layout spec but supported for preview)
    let inline = &widget["style"];

    // Theme widget style for this type
    let theme_ws = &theme["widgets"][kind];

    let fg     = palette.get("fg")     .copied().unwrap_or(Color32::from_rgb(202, 202, 202));
    let bg     = palette.get("bg")     .copied().unwrap_or(Color32::from_rgb(26,  26,  26));
    let accent = palette.get("accent") .copied().unwrap_or(Color32::LIGHT_BLUE);
    let dim    = palette.get("dim")    .copied().unwrap_or(Color32::DARK_GRAY);

    let base_font = theme_base_font(theme);

    match kind {
        "label" => {
            let rect = widget_rect(widget, origin, scale);
            let text = resolve_bind(bind, "text", state);
            let text_color = resolve_color(inline, theme_ws, "text_color", palette, fg);

            if let Some(bg_c) = style_color(inline, "bg_color", palette) {
                painter.rect_filled(rect, Rounding::ZERO, bg_c);
            }
            draw_border_resolved(painter, rect, inline, theme_ws, palette, scale);

            let align_str = inline["text_align"]
                .as_str()
                .or_else(|| theme_ws["text_align"].as_str())
                .unwrap_or("left");
            let (align, pos) = text_anchor(align_str, rect, scale);
            painter.text(pos, align, text, widget_font_id(theme, kind, scale), text_color);
            None
        }

        "icon" => {
            let rect = widget_rect(widget, origin, scale);
            let text = resolve_bind(bind, "text", state);
            let text_color = resolve_color(inline, theme_ws, "text_color", palette, accent);

            let align_str = inline["text_align"]
                .as_str()
                .or_else(|| theme_ws["text_align"].as_str())
                .unwrap_or("center");
            let (align, pos) = text_anchor(align_str, rect, scale);
            painter.text(pos, align, text, widget_font_id(theme, kind, scale), text_color);
            None
        }

        "button" => {
            let rect = widget_rect(widget, origin, scale);
            let label = resolve_bind(bind, "label", state);
            let clicked = click_pos.is_some_and(|p| rect.contains(p));

            let bg_c    = resolve_color(inline, theme_ws, "bg_color",     palette, dim);
            let fg_c    = resolve_color(inline, theme_ws, "text_color",   palette, fg);
            let bdr_c   = resolve_color(inline, theme_ws, "border_color", palette, accent);
            let bdr_w   = resolve_f32(inline, theme_ws, "border_width", 1.0);
            let radius  = resolve_f32(inline, theme_ws, "radius", 0.0) * scale;

            let (fill, text_c, stroke_c) = if clicked {
                let pressed_bg  = resolve_color(inline, theme_ws, "pressed_bg",   palette, accent);
                let pressed_txt = resolve_color(inline, theme_ws, "pressed_text", palette, bg);
                (pressed_bg, pressed_txt, bdr_c)
            } else {
                (bg_c, fg_c, bdr_c)
            };

            painter.rect_filled(rect, Rounding::same(radius), fill);
            if bdr_w > 0.0 {
                painter.rect_stroke(rect, Rounding::same(radius), Stroke::new(bdr_w, stroke_c));
            }
            painter.text(
                rect.center(),
                egui::Align2::CENTER_CENTER,
                label,
                widget_font_id(theme, kind, scale),
                text_c,
            );

            if clicked {
                Some(widget["on_tap"].as_str().unwrap_or("").to_string())
            } else {
                None
            }
        }

        "bar" => {
            let rect = widget_rect(widget, origin, scale);
            let value = resolve_bind_f32(bind, "value", state);
            let max = if let Some(n) = bind["max"].as_f64() {
                n as f32
            } else {
                resolve_bind_f32(bind, "max", state)
            };
            let label = resolve_bind(bind, "label", state);
            let fill_ratio = if max > 0.0 { (value / max).clamp(0.0, 1.0) } else { 0.0 };

            let track_c  = resolve_color(inline, theme_ws, "bg_color", palette, dim);
            let fill_c   = resolve_indicator_color(inline, theme_ws, "fill_color", palette, fg);
            let bdr_c    = resolve_color(inline, theme_ws, "border_color", palette, accent);
            let bdr_w    = resolve_f32(inline, theme_ws, "border_width", 0.0);
            let radius   = resolve_f32(inline, theme_ws, "radius", 0.0) * scale;
            let ind_r    = resolve_indicator_f32(inline, theme_ws, "radius", radius / scale) * scale;

            painter.rect_filled(rect, Rounding::same(radius), track_c);

            let fill_rect = Rect::from_min_size(
                rect.min,
                Vec2::new(rect.width() * fill_ratio, rect.height()),
            );
            if fill_ratio > 0.0 {
                painter.rect_filled(fill_rect, Rounding::same(ind_r), fill_c);
            }

            if bdr_w > 0.0 {
                painter.rect_stroke(rect, Rounding::same(radius), Stroke::new(bdr_w, bdr_c));
            }

            // Bar label: FG color on unfilled area, BG color on filled area (matches device)
            if !label.is_empty() {
                let font = widget_font_id(theme, kind, scale);

                // Unfilled portion (text in FG)
                let unfilled_painter = painter.with_clip_rect(rect);
                unfilled_painter.text(
                    rect.center(),
                    egui::Align2::CENTER_CENTER,
                    &label,
                    font.clone(),
                    fg,
                );

                // Filled portion (text in BG, clipped to fill area)
                if fill_ratio > 0.0 {
                    let filled_painter = painter.with_clip_rect(fill_rect);
                    filled_painter.text(
                        rect.center(),
                        egui::Align2::CENTER_CENTER,
                        &label,
                        font,
                        bg,
                    );
                }
            }

            None
        }

        "gauge" => {
            let rect = widget_rect(widget, origin, scale);
            let value = resolve_bind_f32(bind, "value", state);
            let max = if let Some(n) = bind["max"].as_f64() {
                n as f32
            } else {
                resolve_bind_f32(bind, "max", state)
            };
            let fill_ratio = if max > 0.0 { (value / max).clamp(0.0, 1.0) } else { 0.0 };
            let center = rect.center();
            let radius = rect.width().min(rect.height()) / 2.0 - 2.0;

            let track_c = resolve_color(inline, theme_ws, "arc_color", palette, dim);
            let fill_c  = resolve_indicator_color(inline, theme_ws, "fill_color", palette, accent);
            let text_c  = resolve_color(inline, theme_ws, "text_color", palette, fg);
            let arc_w   = resolve_f32(inline, theme_ws, "arc_width", 3.0) * scale / 2.0;

            painter.circle_stroke(center, radius, Stroke::new(arc_w, track_c));

            let start_angle = std::f32::consts::PI * 0.75;
            let sweep = std::f32::consts::PI * 1.5 * fill_ratio;
            let segs = 32usize;
            if fill_ratio > 0.0 {
                let step = sweep / segs as f32;
                for i in 0..segs {
                    let a0 = start_angle + step * i as f32;
                    let a1 = start_angle + step * (i + 1) as f32;
                    let p0 = center + Vec2::new(a0.cos() * radius, a0.sin() * radius);
                    let p1 = center + Vec2::new(a1.cos() * radius, a1.sin() * radius);
                    painter.line_segment([p0, p1], Stroke::new(arc_w, fill_c));
                }
            }

            let label = resolve_bind(bind, "label", state);
            if !label.is_empty() {
                painter.text(
                    center,
                    egui::Align2::CENTER_CENTER,
                    label,
                    widget_font_id(theme, kind, scale),
                    text_c,
                );
            }

            None
        }

        "spinner" => {
            let rect = widget_rect(widget, origin, scale);
            let center = rect.center();
            let radius = rect.width().min(rect.height()) / 2.0 - 2.0;

            let track_c = resolve_color(inline, theme_ws, "arc_color", palette, dim);
            let fill_c  = resolve_indicator_color(inline, theme_ws, "fill_color", palette, accent);
            let arc_w   = resolve_f32(inline, theme_ws, "arc_width", 2.0) * scale / 2.0;

            // Background arc
            painter.circle_stroke(center, radius, Stroke::new(arc_w, track_c));

            let start = (time as f32 * std::f32::consts::TAU / 1.5) % std::f32::consts::TAU;
            let span = std::f32::consts::PI * 1.5;
            let segs = 24usize;
            for i in 0..segs {
                let a0 = start + span * i as f32 / segs as f32;
                let a1 = start + span * (i + 1) as f32 / segs as f32;
                let p0 = center + Vec2::new(a0.cos() * radius, a0.sin() * radius);
                let p1 = center + Vec2::new(a1.cos() * radius, a1.sin() * radius);
                painter.line_segment([p0, p1], Stroke::new(arc_w, fill_c));
            }

            None
        }

        "graph" | "sparkline" => {
            let rect = widget_rect(widget, origin, scale);
            let placeholder = bind["value"]
                .as_str()
                .unwrap_or("")
                .trim_start_matches('$')
                .to_string();

            let line_c   = resolve_color(inline, theme_ws, "color", palette, accent);
            let bg_c     = resolve_color(inline, theme_ws, "bg_color", palette, bg);
            let bdr_c    = resolve_color(inline, theme_ws, "border_color", palette, dim);
            let bdr_w    = resolve_f32(inline, theme_ws, "border_width", 0.0);
            let radius   = resolve_f32(inline, theme_ws, "radius", 0.0) * scale;
            let line_w   = resolve_f32(inline, theme_ws, "line_width", 1.5);

            if kind == "graph" {
                let fill_opa = resolve_f32(inline, theme_ws, "bg_opa", 255.0);
                let bg_alpha = (fill_opa / 255.0 * 255.0) as u8;
                let bg_tinted = Color32::from_rgba_unmultiplied(bg_c.r(), bg_c.g(), bg_c.b(), bg_alpha);
                painter.rect_filled(rect, Rounding::same(radius), bg_tinted);
                if bdr_w > 0.0 {
                    painter.rect_stroke(rect, Rounding::same(radius), Stroke::new(bdr_w, bdr_c));
                }

                // Horizontal grid lines
                let grid_lines = resolve_f32(inline, theme_ws, "grid_h_lines", 0.0) as usize;
                if grid_lines > 0 {
                    let grid_c = resolve_color(inline, theme_ws, "grid_color", palette, dim);
                    for i in 1..=grid_lines {
                        let y = rect.min.y + rect.height() * i as f32 / (grid_lines + 1) as f32;
                        painter.line_segment(
                            [Pos2::new(rect.min.x, y), Pos2::new(rect.max.x, y)],
                            Stroke::new(1.0, grid_c),
                        );
                    }
                }
            }

            if let Some(buf) = history.get(&placeholder) {
                let y_max = resolve_f32(inline, theme_ws, "y_max", 100.0);
                let y_min = resolve_f32(inline, theme_ws, "y_min", 0.0);
                let range = (y_max - y_min).max(1.0);

                let n = buf.len().min(rect.width() as usize);
                if n >= 2 {
                    let pts: Vec<Pos2> = buf
                        .iter()
                        .rev()
                        .take(n)
                        .collect::<Vec<_>>()
                        .into_iter()
                        .rev()
                        .enumerate()
                        .map(|(i, &v)| {
                            Pos2::new(
                                rect.min.x + i as f32 * (rect.width() / (n - 1) as f32),
                                rect.max.y - ((v - y_min) / range).clamp(0.0, 1.0) * rect.height(),
                            )
                        })
                        .collect();
                    painter.add(egui::Shape::line(pts, Stroke::new(line_w, line_c)));
                }
            }

            None
        }

        "image" => {
            let rect = widget_rect(widget, origin, scale);
            let src_name = bind["src"].as_str().unwrap_or_else(|| widget["src"].as_str().unwrap_or(""));

            if let Some(tex) = load_or_get_texture(src_name, images_dir, ctx, image_cache) {
                let uv = Rect::from_min_max(Pos2::ZERO, Pos2::new(1.0, 1.0));
                painter.image(tex.id(), rect, uv, Color32::WHITE);
            } else {
                // Fallback placeholder when asset not found
                let bg_c = style_color(inline, "bg_color", palette).unwrap_or(dim);
                painter.rect_filled(rect, Rounding::same(2.0), bg_c);
                painter.rect_stroke(rect, Rounding::same(2.0), Stroke::new(1.0, fg.gamma_multiply(0.35)));
                if !src_name.is_empty() {
                    painter.text(
                        rect.center(),
                        egui::Align2::CENTER_CENTER,
                        src_name,
                        egui::FontId::monospace(base_font * 0.7 * scale),
                        fg.gamma_multiply(0.6),
                    );
                }
            }

            None
        }

        "toggle" => {
            let rect = widget_rect(widget, origin, scale);
            let value = resolve_bind_f32(bind, "value", state);
            let on = value != 0.0;

            let checked_ws = &theme_ws["checked"];
            let off_bg = resolve_color(inline, theme_ws, "bg_color", palette, dim);
            let on_bg  = style_color(checked_ws, "bg_color", palette).unwrap_or(accent);
            let track_c = if on { on_bg } else { off_bg };
            let radius = resolve_f32(inline, theme_ws, "radius", rect.height() / 2.0 / scale) * scale;

            painter.rect_filled(rect, Rounding::same(radius), track_c);

            if resolve_f32(inline, theme_ws, "border_width", 0.0) > 0.0 {
                let bdr_c = resolve_color(inline, theme_ws, "border_color", palette, accent);
                painter.rect_stroke(rect, Rounding::same(radius), Stroke::new(1.0, bdr_c));
            }

            let knob_r = rect.height() / 2.0 - 2.0 * scale;
            let knob_x = if on {
                rect.max.x - knob_r - 2.0 * scale
            } else {
                rect.min.x + knob_r + 2.0 * scale
            };
            painter.circle_filled(Pos2::new(knob_x, rect.center().y), knob_r, fg);
            None
        }

        "container" => {
            let rect = widget_rect(widget, origin, scale);

            // Background — only if bg_opa > 0
            let bg_opa = resolve_f32(inline, theme_ws, "bg_opa", 0.0);
            if bg_opa > 0.0 {
                let bg_c = resolve_color(inline, theme_ws, "bg_color", palette, bg);
                let alpha = (bg_opa / 255.0 * 255.0) as u8;
                let bg_tinted = Color32::from_rgba_unmultiplied(bg_c.r(), bg_c.g(), bg_c.b(), alpha);
                painter.rect_filled(rect, Rounding::ZERO, bg_tinted);
            }
            let bdr_w = resolve_f32(inline, theme_ws, "border_width", 0.0);
            if bdr_w > 0.0 {
                let bdr_c = resolve_color(inline, theme_ws, "border_color", palette, dim);
                painter.rect_stroke(rect, Rounding::ZERO, Stroke::new(bdr_w, bdr_c));
            }

            let mut hit = None;

            if let Some(children) = widget["children"].as_array() {
                let layout = widget["layout"].as_str().unwrap_or("absolute");

                let pad_left   = style_pad(inline, "pad_left",   scale);
                let pad_right  = style_pad(inline, "pad_right",  scale);
                let pad_top    = style_pad(inline, "pad_top",    scale);
                let pad_bottom = style_pad(inline, "pad_bottom", scale);
                let gap        = resolve_f32(inline, theme_ws, "pad_gap", 0.0) * scale;

                let content_min = Pos2::new(rect.min.x + pad_left, rect.min.y + pad_top);
                let content_w   = (rect.width() - pad_left - pad_right).max(1.0);
                let _content_h  = (rect.height() - pad_top - pad_bottom).max(1.0);

                let cols   = widget["cols"].as_u64().unwrap_or(1).max(1) as usize;
                let cell_w = if layout == "grid" {
                    ((content_w - gap * (cols.saturating_sub(1)) as f32) / cols as f32).max(1.0)
                } else {
                    0.0
                };

                let mut cursor_x = content_min.x;
                let mut cursor_y = content_min.y;
                let mut row_h    = 0.0f32;
                let mut grid_col = 0usize;

                for child in children {
                    if layout == "absolute" {
                        if let Some(on_tap) = render_widget(
                            child, painter, rect.min, scale, palette, state, history, time, click_pos, theme, ctx, image_cache, images_dir,
                        ) {
                            hit = Some(on_tap);
                        }
                        continue;
                    }

                    let mut child_widget = child.clone();
                    child_widget["x"] = json!(0);
                    child_widget["y"] = json!(0);

                    let child_h = child["h"].as_f64().unwrap_or(20.0) as f32 * scale;

                    let child_origin = match layout {
                        "row" => {
                            let p = Pos2::new(cursor_x, cursor_y);
                            let child_w = child["w"].as_f64().unwrap_or(80.0) as f32 * scale;
                            cursor_x += child_w + gap;
                            p
                        }
                        "column" => {
                            let p = Pos2::new(cursor_x, cursor_y);
                            cursor_y += child_h + gap;
                            p
                        }
                        "grid" => {
                            child_widget["w"] = json!(cell_w / scale);
                            let x = content_min.x + grid_col as f32 * (cell_w + gap);
                            let p = Pos2::new(x, cursor_y);
                            if child_h > row_h { row_h = child_h; }
                            grid_col += 1;
                            if grid_col >= cols {
                                grid_col = 0;
                                cursor_y += row_h + gap;
                                row_h = 0.0;
                            }
                            p
                        }
                        _ => rect.min,
                    };

                    if let Some(on_tap) = render_widget(
                        &child_widget, painter, child_origin, scale, palette, state, history, time, click_pos, theme, ctx, image_cache, images_dir,
                    ) {
                        hit = Some(on_tap);
                    }
                }
            }

            hit
        }

        "ticker" => {
            let rect = widget_rect(widget, origin, scale);
            let text_c = resolve_color(inline, theme_ws, "text_color", palette, accent);

            if let Some(frames) = widget["frames"].as_array() {
                if !frames.is_empty() {
                    let interval_ms = widget["interval_ms"].as_f64().unwrap_or(100.0).max(1.0);
                    let idx = ((time * 1000.0 / interval_ms) as usize) % frames.len();
                    let frame = frames[idx].as_str().unwrap_or("-");
                    painter.text(
                        rect.min + Vec2::new(2.0, 2.0),
                        egui::Align2::LEFT_TOP,
                        frame,
                        widget_font_id(theme, kind, scale),
                        text_c,
                    );
                }
            }

            None
        }

        _ => None,
    }
}

// ── Image loading ─────────────────────────────────────────────────────────────

fn load_or_get_texture<'c>(
    name: &str,
    images_dir: &str,
    ctx: &egui::Context,
    cache: &'c mut HashMap<String, egui::TextureHandle>,
) -> Option<&'c egui::TextureHandle> {
    if name.is_empty() {
        return None;
    }
    if !cache.contains_key(name) {
        if let Some(handle) = load_image_from_dir(name, images_dir, ctx) {
            cache.insert(name.to_string(), handle);
        }
    }
    cache.get(name)
}

/// Looks up image file the same way the server does: images_dir/<name>[.ext]
fn load_image_from_dir(name: &str, images_dir: &str, ctx: &egui::Context) -> Option<egui::TextureHandle> {
    let base = std::path::Path::new(images_dir).join(name);
    let path = if base.is_file() {
        base
    } else {
        ["png", "jpg", "jpeg", "bmp"]
            .iter()
            .map(|ext| base.with_extension(ext))
            .find(|p| p.is_file())?
    };

    let img = image::open(&path).ok()?.into_rgba8();
    let (w, h) = img.dimensions();
    let pixels: Vec<Color32> = img
        .pixels()
        .map(|p| Color32::from_rgba_unmultiplied(p[0], p[1], p[2], p[3]))
        .collect();

    let color_image = egui::ColorImage { size: [w as usize, h as usize], pixels };
    Some(ctx.load_texture(name, color_image, egui::TextureOptions::default()))
}

// ── Drawing helpers ───────────────────────────────────────────────────────────

fn draw_border_resolved(
    painter: &egui::Painter,
    rect: Rect,
    inline: &Value,
    theme_ws: &Value,
    palette: &HashMap<String, Color32>,
    scale: f32,
) {
    let bdr_w = resolve_f32(inline, theme_ws, "border_width", 0.0);
    if bdr_w <= 0.0 {
        return;
    }
    let Some(color) = style_color(inline, "border_color", palette)
        .or_else(|| style_color(theme_ws, "border_color", palette))
    else {
        return;
    };
    let radius = resolve_f32(inline, theme_ws, "radius", 0.0) * scale;
    painter.rect_stroke(rect, Rounding::same(radius), Stroke::new(bdr_w, color));
}

fn text_anchor(align: &str, rect: Rect, _scale: f32) -> (egui::Align2, Pos2) {
    match align {
        "center" => (egui::Align2::CENTER_CENTER, rect.center()),
        "right"  => (egui::Align2::RIGHT_CENTER,  Pos2::new(rect.max.x - 2.0, rect.center().y)),
        _        => (egui::Align2::LEFT_TOP,       rect.min),
    }
}

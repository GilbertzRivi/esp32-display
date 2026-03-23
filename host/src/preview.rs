use eframe::egui::{self, Color32, Pos2, Rect, Rounding, Stroke, Vec2};
use serde_json::Value;
use std::collections::{HashMap, VecDeque};
use tokio::sync::broadcast;

pub struct PreviewApp {
    pub artifacts: crate::artifacts::Artifacts,
    pub state: crate::state::State,
    pub tray_rx: std::sync::mpsc::Receiver<TrayEvent>,
    pub visible: bool,
    pub history: HashMap<String, VecDeque<f32>>,
    pub reload_tx: broadcast::Sender<()>,
    pub last_history_tick: std::time::Instant,
    pub current_screen: String,
    pub event_tx: std::sync::mpsc::SyncSender<String>,
}

#[derive(Debug)]
pub enum TrayEvent {
    Toggle,
    Quit,
}

impl eframe::App for PreviewApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        // Handle tray events
        while let Ok(event) = self.tray_rx.try_recv() {
            match event {
                TrayEvent::Toggle => {
                    self.visible = !self.visible;
                    ctx.send_viewport_cmd(egui::ViewportCommand::Visible(self.visible));
                }
                TrayEvent::Quit => {
                    ctx.send_viewport_cmd(egui::ViewportCommand::Close);
                }
            }
        }

        // Update history ring buffers at fixed 1s interval (independent of repaint rate)
        let now = std::time::Instant::now();
        if now.duration_since(self.last_history_tick) >= std::time::Duration::from_secs(1) {
            self.last_history_tick = now;
            let snapshot = self.state.read().unwrap().clone();
            for (k, v) in &snapshot {
                if let Ok(f) = v.parse::<f32>() {
                    let buf = self.history.entry(k.clone()).or_default();
                    buf.push_back(f);
                    if buf.len() > 512 { buf.pop_front(); }
                }
            }
        }

        ctx.request_repaint_after(std::time::Duration::from_millis(100));

        egui::CentralPanel::default().show(ctx, |ui| {
            ui.horizontal(|ui| {
                // Left: canvas
                ui.vertical(|ui| {
                    ui.label("Preview (480x320)");
                    ui.add_space(4.0);
                    self.render_canvas(ui);
                });

                ui.separator();

                // Right: state panel
                ui.vertical(|ui| {
                    ui.label("State");
                    ui.add_space(4.0);
                    self.render_state_panel(ui);
                });
            });
        });
    }
}

impl PreviewApp {
    fn render_canvas(&mut self, ui: &mut egui::Ui) {
        let scale = 1.5_f32;
        let canvas_w = 480.0 * scale;
        let canvas_h = 320.0 * scale;
        let time = ui.ctx().input(|i| i.time);

        let (rect, response) = ui.allocate_exact_size(
            Vec2::new(canvas_w, canvas_h),
            egui::Sense::click(),
        );
        let click_pos = if response.clicked() { response.interact_pointer_pos() } else { None };
        let painter = ui.painter_at(rect);

        // Parse theme
        let theme = self.artifacts.get_theme();
        let palette = parse_palette(&theme);
        let state = self.state.read().unwrap().clone();

        let bg = palette.get("bg").copied().unwrap_or(Color32::from_rgb(26, 26, 26));
        painter.rect_filled(rect, Rounding::ZERO, bg);

        // Parse layout
        let layout = self.artifacts.get_layout();
        if let Ok(layout_val) = serde_json::from_str::<Value>(&layout) {
            if let Some(screens) = layout_val["screens"].as_array() {
                let screen = screens.iter()
                    .find(|s| s["id"].as_str() == Some(&self.current_screen))
                    .or_else(|| screens.first());
                if let Some(screen) = screen {
                    if let Some(widgets) = screen["widgets"].as_array() {
                        for widget in widgets {
                            if let Some(on_tap) = render_widget(
                                widget, &painter, rect.min, scale,
                                &palette, &state, &self.history, time, click_pos,
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
            let _ = self.event_tx.try_send(event.to_string());
        }
    }

    fn render_state_panel(&self, ui: &mut egui::Ui) {
        let state = self.state.read().unwrap().clone();
        let mut keys: Vec<_> = state.keys().cloned().collect();
        keys.sort();

        egui::ScrollArea::vertical().show(ui, |ui| {
            for key in keys {
                let val = state.get(&key).cloned().unwrap_or_default();
                ui.horizontal(|ui| {
                    ui.label(format!("* {key}"));
                    ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                        ui.monospace(&val);
                    });
                });
            }
        });

        ui.add_space(8.0);
        if ui.button("Reload Device").clicked() {
            let _ = self.reload_tx.send(());
        }

        ui.add_space(8.0);
        let theme = self.artifacts.get_theme();
        let palette = parse_palette(&theme);
        ui.label("Palette:");
        ui.horizontal(|ui| {
            let mut entries: Vec<_> = palette.iter().collect();
            entries.sort_by_key(|(k, _)| k.as_str());
            for (name, color) in entries {
                ui.vertical(|ui| {
                    let (rect, _) = ui.allocate_exact_size(Vec2::new(24.0, 24.0), egui::Sense::hover());
                    ui.painter().rect_filled(rect, Rounding::ZERO, *color);
                    ui.small(name);
                });
            }
        });
    }
}

fn parse_palette(theme_json: &str) -> HashMap<String, Color32> {
    let mut map = HashMap::new();
    if let Ok(val) = serde_json::from_str::<Value>(theme_json) {
        if let Some(palette) = val["palette"].as_object() {
            for (k, v) in palette {
                if let Some(hex) = v.as_str() {
                    if let Some(color) = parse_hex_color(hex) {
                        map.insert(k.clone(), color);
                    }
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

fn resolve_bind(val: &Value, key: &str, state: &HashMap<String, String>) -> String {
    let raw = val[key].as_str().unwrap_or("");
    if !raw.contains('$') {
        return raw.to_string();
    }
    // Replace all $name tokens (e.g. "$cpu_usage%" → "42%")
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
) -> Option<String> {
    let kind = widget["type"].as_str().unwrap_or("");
    let bind = &widget["bind"];

    let fg = palette.get("fg").copied().unwrap_or(Color32::LIGHT_GRAY);
    let bg = palette.get("bg").copied().unwrap_or(Color32::from_rgb(26, 26, 26));
    let accent = palette.get("accent").copied().unwrap_or(Color32::LIGHT_BLUE);
    let dim = palette.get("dim").copied().unwrap_or(Color32::DARK_GRAY);

    match kind {
        "label" | "icon" => {
            let rect = widget_rect(widget, origin, scale);
            let text = resolve_bind(bind, "text", state);
            painter.text(
                rect.min + Vec2::new(2.0, 2.0),
                egui::Align2::LEFT_TOP,
                text,
                egui::FontId::monospace(10.0 * scale),
                fg,
            );
            None
        }

        "button" => {
            let rect = widget_rect(widget, origin, scale);
            let label = resolve_bind(bind, "label", state);
            let clicked = click_pos.is_some_and(|p| rect.contains(p));
            let border_color = if clicked { fg } else { accent };
            painter.rect_stroke(rect, Rounding::same(2.0), Stroke::new(1.0, border_color));
            if clicked {
                painter.rect_filled(rect, Rounding::same(2.0), accent.gamma_multiply(0.3));
            }
            painter.text(
                rect.center(),
                egui::Align2::CENTER_CENTER,
                label,
                egui::FontId::monospace(9.0 * scale),
                fg,
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
            let max = {
                let raw = bind["max"].as_f64().map(|v| v as f32);
                raw.unwrap_or_else(|| resolve_bind_f32(bind, "max", state))
            };
            let label = resolve_bind(bind, "label", state);
            let fill_ratio = if max > 0.0 { (value / max).clamp(0.0, 1.0) } else { 0.0 };

            painter.rect_filled(rect, Rounding::ZERO, dim);
            let fill_rect = Rect::from_min_size(rect.min, Vec2::new(rect.width() * fill_ratio, rect.height()));
            painter.rect_filled(fill_rect, Rounding::ZERO, accent);
            painter.text(
                rect.center(),
                egui::Align2::CENTER_CENTER,
                label,
                egui::FontId::monospace(8.0 * scale),
                fg,
            );
            None
        }

        "gauge" => {
            let rect = widget_rect(widget, origin, scale);
            let value = resolve_bind_f32(bind, "value", state);
            let max = {
                let raw = bind["max"].as_f64().map(|v| v as f32);
                raw.unwrap_or_else(|| resolve_bind_f32(bind, "max", state))
            };
            let fill_ratio = if max > 0.0 { (value / max).clamp(0.0, 1.0) } else { 0.0 };
            let center = rect.center();
            let radius = rect.width().min(rect.height()) / 2.0 - 2.0;

            painter.circle_stroke(center, radius, Stroke::new(2.0, dim));
            let start_angle = std::f32::consts::PI * 0.75;
            let end_angle = start_angle + std::f32::consts::PI * 1.5 * fill_ratio;
            let segments = 32;
            let step = (end_angle - start_angle) / segments as f32;
            for i in 0..segments {
                let a0 = start_angle + step * i as f32;
                let a1 = start_angle + step * (i + 1) as f32;
                let p0 = center + Vec2::new(a0.cos() * radius, a0.sin() * radius);
                let p1 = center + Vec2::new(a1.cos() * radius, a1.sin() * radius);
                painter.line_segment([p0, p1], Stroke::new(3.0, accent));
            }
            let label = resolve_bind(bind, "label", state);
            painter.text(center, egui::Align2::CENTER_CENTER, label, egui::FontId::monospace(8.0 * scale), fg);
            None
        }

        "spinner" => {
            let rect = widget_rect(widget, origin, scale);
            let center = rect.center();
            let radius = rect.width().min(rect.height()) / 2.0 - 2.0;
            let start = (time as f32 * std::f32::consts::TAU / 1.5) % std::f32::consts::TAU;
            let span = std::f32::consts::PI * 1.5;
            let segs = 24usize;
            for i in 0..segs {
                let a0 = start + span * i as f32 / segs as f32;
                let a1 = start + span * (i + 1) as f32 / segs as f32;
                let p0 = center + Vec2::new(a0.cos() * radius, a0.sin() * radius);
                let p1 = center + Vec2::new(a1.cos() * radius, a1.sin() * radius);
                painter.line_segment([p0, p1], Stroke::new(2.0, accent));
            }
            None
        }

        "graph" | "sparkline" => {
            let rect = widget_rect(widget, origin, scale);
            let placeholder = bind["value"].as_str().unwrap_or("").trim_start_matches('$').to_string();

            if kind == "graph" {
                painter.rect_filled(rect, Rounding::ZERO, bg);
                painter.rect_stroke(rect, Rounding::ZERO, Stroke::new(1.0, dim));
            }

            if let Some(buf) = history.get(&placeholder) {
                let n = buf.len().min(rect.width() as usize);
                if n >= 2 {
                    let points: Vec<Pos2> = buf.iter().rev().take(n).collect::<Vec<_>>()
                        .into_iter().rev().enumerate()
                        .map(|(i, &v)| Pos2::new(
                            rect.min.x + i as f32 * (rect.width() / (n - 1) as f32),
                            rect.max.y - (v / 100.0).clamp(0.0, 1.0) * rect.height(),
                        ))
                        .collect();
                    painter.add(egui::Shape::line(points, Stroke::new(1.5, accent)));
                }
            }
            None
        }

        "container" => {
            let rect = widget_rect(widget, origin, scale);
            painter.rect_stroke(rect, Rounding::ZERO, Stroke::new(1.0, dim));

            let mut hit = None;
            if let Some(children) = widget["children"].as_array() {
                let layout = widget["layout"].as_str().unwrap_or("absolute");
                let mut offset = Vec2::ZERO;

                for child in children {
                    let child_origin = if layout == "absolute" {
                        rect.min
                    } else {
                        rect.min + offset
                    };

                    if let Some(on_tap) = render_widget(
                        child, painter, child_origin, scale, palette, state, history, time, click_pos,
                    ) {
                        hit = Some(on_tap);
                    }

                    if layout == "row" {
                        let cw = child["w"].as_f64().unwrap_or(80.0) as f32 * scale;
                        offset.x += cw;
                    } else if layout == "column" {
                        let ch = child["h"].as_f64().unwrap_or(20.0) as f32 * scale;
                        offset.y += ch;
                    }
                }
            }
            hit
        }

        "ticker" => {
            let rect = widget_rect(widget, origin, scale);
            if let Some(frames) = widget["frames"].as_array() {
                if !frames.is_empty() {
                    let interval_ms = widget["interval_ms"].as_f64().unwrap_or(100.0).max(1.0);
                    let idx = ((time * 1000.0 / interval_ms) as usize) % frames.len();
                    let frame = frames[idx].as_str().unwrap_or("-");
                    painter.text(
                        rect.min + Vec2::new(2.0, 2.0),
                        egui::Align2::LEFT_TOP,
                        frame,
                        egui::FontId::monospace(10.0 * scale),
                        fg,
                    );
                }
            }
            None
        }

        _ => None,
    }
}

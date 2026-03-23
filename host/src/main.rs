mod config;
mod state;
mod artifacts;
mod source;
mod collector;
mod server;
mod preview;

use preview::{PreviewApp, TrayEvent};
use std::sync::mpsc;
use tray_icon::{TrayIconBuilder, menu::{Menu, MenuItem, MenuEvent}};

fn main() {
    tracing_subscriber::fmt::init();

    let cfg = config::load("config.toml");
    let state = state::new();
    let artifacts = artifacts::Artifacts::load(&cfg.artifacts.layout, &cfg.artifacts.theme);

    // Start file watcher
    artifacts.start_watcher(
        cfg.artifacts.layout.clone(),
        cfg.artifacts.theme.clone(),
    );

    let (broadcast_tx, _) = tokio::sync::broadcast::channel::<()>(64);
    let (reload_tx, _) = tokio::sync::broadcast::channel::<()>(4);
    let (tray_tx, tray_rx) = mpsc::channel::<TrayEvent>();
    let (event_tx, event_rx) = mpsc::sync_channel::<String>(16);

    // Dispatch preview button events to configured commands
    {
        let cfg_events = cfg.events.clone();
        std::thread::spawn(move || {
            while let Ok(event) = event_rx.recv() {
                if let Some(ev_cfg) = cfg_events.iter().find(|e| e.event == event) {
                    std::process::Command::new("sh").args(["-c", &ev_cfg.command]).spawn().ok();
                }
            }
        });
    }

    // Start tokio runtime in background thread
    let rt = tokio::runtime::Runtime::new().unwrap();
    {
        let state = state.clone();
        let artifacts = artifacts.clone();
        let broadcast_tx = broadcast_tx.clone();
        let reload_tx2 = reload_tx.clone();
        let cfg2 = cfg.clone();

        rt.spawn(async move {
            let app_state = server::AppState {
                artifacts,
                state: state.clone(),
                broadcast_tx: broadcast_tx.clone(),
                events: cfg2.events.clone(),
                reload_tx: reload_tx2,
            };

            tokio::join!(
                server::run(cfg2.server.host, cfg2.server.port, app_state),
                collector::run(cfg2.sources, state, broadcast_tx),
            );
        });
    }

    // Tray icon (GTK must be initialized first on Linux)
    gtk::init().expect("failed to init GTK");

    // Suppress the libayatana-appindicator deprecation warning printed by the C library
    let _log_handler = gtk::glib::log_set_handler(
        Some("libayatana-appindicator"),
        gtk::glib::LogLevels::all(),
        false,
        false,
        |_, _, _| {},
    );
    let tray_menu = Menu::new();
    let show_item = MenuItem::new("Show/Hide", true, None);
    let quit_item = MenuItem::new("Quit", true, None);
    let show_id = show_item.id().clone();
    let quit_id = quit_item.id().clone();
    tray_menu.append(&show_item).unwrap();
    tray_menu.append(&quit_item).unwrap();

    let icon_rgba: Vec<u8> = vec![0u8; 32 * 32 * 4]; // blank icon placeholder
    let icon = tray_icon::Icon::from_rgba(icon_rgba, 32, 32).unwrap();

    let _tray = TrayIconBuilder::new()
        .with_menu(Box::new(tray_menu))
        .with_icon(icon)
        .with_tooltip("ESP32 Display Host")
        .build()
        .unwrap();

    let tray_tx2 = tray_tx.clone();
    std::thread::spawn(move || {
        loop {
            if let Ok(event) = MenuEvent::receiver().recv() {
                if event.id == show_id {
                    let _ = tray_tx2.send(TrayEvent::Toggle);
                } else if event.id == quit_id {
                    let _ = tray_tx2.send(TrayEvent::Quit);
                }
            }
        }
    });

    // Run eframe on main thread
    let native_options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_title("ESP32 Display Host")
            .with_inner_size([900.0, 400.0])
            .with_visible(false),
        ..Default::default()
    };

    eframe::run_native(
        "ESP32 Display Host",
        native_options,
        Box::new(|_cc| {
            Ok(Box::new(PreviewApp {
                artifacts,
                state,
                tray_rx,
                visible: false,
                history: std::collections::HashMap::new(),
                reload_tx,
                last_history_tick: std::time::Instant::now(),
                current_screen: "main".to_string(),
                event_tx,
            }))
        }),
    ).unwrap();

    drop(rt);
}

mod config;
mod state;
mod artifacts;
mod source;
mod collector;
mod server;
mod preview;

use preview::PreviewApp;

enum TrayCmd {
    ShowUi,
    Reload,
    ReloadDevice,
    Exit,
}

fn main() {
    tracing_subscriber::fmt::init();

    let cfg = config::load("config.toml");
    tracing::info!("starting host");
    tracing::info!("cwd = {:?}", std::env::current_dir().ok());
    tracing::info!("images_dir = {:?}", cfg.artifacts.images_dir);

    let state = state::new();
    let artifacts = artifacts::Artifacts::load(&cfg.artifacts.layout, &cfg.artifacts.theme);

    artifacts.start_watcher(
        cfg.artifacts.layout.clone(),
        cfg.artifacts.theme.clone(),
    );

    let (broadcast_tx, _) = tokio::sync::broadcast::channel::<()>(64);
    let (reload_tx, _) = tokio::sync::broadcast::channel::<()>(4);
    let (event_tx, event_rx) = std::sync::mpsc::sync_channel::<String>(16);

    {
        let cfg_events = cfg.events.clone();
        std::thread::spawn(move || {
            while let Ok(event) = event_rx.recv() {
                if let Some(ev_cfg) = cfg_events.iter().find(|e| e.event == event) {
                    std::process::Command::new("sh")
                        .args(["-c", &ev_cfg.command])
                        .spawn()
                        .ok();
                }
            }
        });
    }

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
                images_dir: cfg2.artifacts.images_dir.clone(),
            };

            tokio::join!(
                server::run(cfg2.server.host, cfg2.server.port, app_state),
                collector::run(cfg2.sources, state, broadcast_tx),
            );
        });
    }

    // GTK inicjalizacja przed tray icon (Linux)
    #[cfg(target_os = "linux")]
    if let Err(e) = gtk::init() {
        tracing::warn!("gtk init failed: {e}");
    }

    let (tray_tx, tray_rx) = std::sync::mpsc::sync_channel::<TrayCmd>(16);
    let _tray = build_tray(tray_tx);

    // Preview thread handle — None gdy okno zamknięte, Some gdy działa
    let mut preview_thread: Option<std::thread::JoinHandle<()>> = None;

    // Główna pętla: GTK events + tray commands. eframe jest w osobnym wątku gdy potrzebny.
    loop {
        #[cfg(target_os = "linux")]
        while gtk::events_pending() {
            gtk::main_iteration_do(false);
        }

        while let Ok(cmd) = tray_rx.try_recv() {
            match cmd {
                TrayCmd::ShowUi => {
                    let running = preview_thread
                        .as_ref()
                        .map(|h| !h.is_finished())
                        .unwrap_or(false);
                    if !running {
                        preview_thread = Some(spawn_preview(
                            state.clone(),
                            artifacts.clone(),
                            event_tx.clone(),
                            reload_tx.clone(),
                            cfg.artifacts.layout.clone(),
                            cfg.artifacts.theme.clone(),
                            cfg.artifacts.images_dir.clone(),
                        ));
                    }
                }
                TrayCmd::Reload => {
                    artifacts.reload(&cfg.artifacts.layout, &cfg.artifacts.theme);
                }
                TrayCmd::ReloadDevice => {
                    let _ = reload_tx.send(());
                }
                TrayCmd::Exit => {
                    std::process::exit(0);
                }
            }
        }

        std::thread::sleep(std::time::Duration::from_millis(50));
    }
}

fn spawn_preview(
    state: crate::state::State,
    artifacts: crate::artifacts::Artifacts,
    event_tx: std::sync::mpsc::SyncSender<String>,
    reload_tx: tokio::sync::broadcast::Sender<()>,
    layout_path: String,
    theme_path: String,
    images_dir: String,
) -> std::thread::JoinHandle<()> {
    std::thread::spawn(move || {
        let options = eframe::NativeOptions {
            viewport: eframe::egui::ViewportBuilder::default()
                .with_title("ESP32 Display Host")
                .with_inner_size([1000.0, 560.0]),
            // Wymagane gdy EventLoop jest tworzony poza głównym wątkiem (Linux/Windows).
            // Na macOS to jest niedozwolone, ale tu nie obsługujemy macOS.
            event_loop_builder: Some(Box::new(|builder| {
                #[cfg(target_os = "linux")]
                {
                    use winit::platform::x11::EventLoopBuilderExtX11;
                    use winit::platform::wayland::EventLoopBuilderExtWayland;
                    // Wywołujemy oba przez UFCS — tylko jeden zadziała w runtime
                    // zależnie od tego czy sesja to X11 czy Wayland.
                    EventLoopBuilderExtX11::with_any_thread(builder, true);
                    EventLoopBuilderExtWayland::with_any_thread(builder, true);
                }
                // Windows: winit nie ma tego ograniczenia, nic nie trzeba robić.
                let _ = builder;
            })),
            ..Default::default()
        };

        eframe::run_native(
            "ESP32 Display Host",
            options,
            Box::new(|_cc| {
                Ok(Box::new(PreviewApp {
                    artifacts,
                    state,
                    history: std::collections::HashMap::new(),
                    last_history_tick: std::time::Instant::now(),
                    current_screen: "main".to_string(),
                    event_tx,
                    reload_tx,
                    layout_path,
                    theme_path,
                    images_dir,
                    image_cache: std::collections::HashMap::new(),
                }))
            }),
        ).ok();
    })
}

fn build_tray(tx: std::sync::mpsc::SyncSender<TrayCmd>) -> Option<tray_icon::TrayIcon> {
    use tray_icon::{TrayIconBuilder, menu::{Menu, MenuItem, PredefinedMenuItem, MenuEvent}};

    let icon = make_tray_icon();
    let menu = Menu::new();

    let show_item         = MenuItem::new("Show UI", true, None);
    let reload_item       = MenuItem::new("Reload layout/theme", true, None);
    let reload_dev_item   = MenuItem::new("Reload device", true, None);
    let exit_item         = MenuItem::new("Exit", true, None);

    let _ = menu.append(&show_item);
    let _ = menu.append(&PredefinedMenuItem::separator());
    let _ = menu.append(&reload_item);
    let _ = menu.append(&reload_dev_item);
    let _ = menu.append(&PredefinedMenuItem::separator());
    let _ = menu.append(&exit_item);

    let show_id        = show_item.id().clone();
    let reload_id      = reload_item.id().clone();
    let reload_dev_id  = reload_dev_item.id().clone();
    let exit_id        = exit_item.id().clone();

    std::thread::spawn(move || {
        let recv = MenuEvent::receiver();
        loop {
            if let Ok(event) = recv.recv() {
                let cmd = if event.id == show_id {
                    TrayCmd::ShowUi
                } else if event.id == reload_id {
                    TrayCmd::Reload
                } else if event.id == reload_dev_id {
                    TrayCmd::ReloadDevice
                } else if event.id == exit_id {
                    TrayCmd::Exit
                } else {
                    continue;
                };
                let _ = tx.send(cmd);
            }
        }
    });

    match TrayIconBuilder::new()
        .with_menu(Box::new(menu))
        .with_icon(icon)
        .with_tooltip("ESP32 Display")
        .build()
    {
        Ok(tray) => Some(tray),
        Err(e) => {
            tracing::warn!("tray unavailable: {e}");
            None
        }
    }
}

fn make_tray_icon() -> tray_icon::Icon {
    let bytes = include_bytes!("../assets/tray_icon.jpg");
    let img = image::load_from_memory(bytes)
        .expect("load tray icon")
        .into_rgba8();
    let (w, h) = img.dimensions();
    tray_icon::Icon::from_rgba(img.into_raw(), w, h)
        .expect("valid tray icon rgba")
}

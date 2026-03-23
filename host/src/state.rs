use std::collections::HashMap;
use std::sync::{Arc, RwLock};

pub type State = Arc<RwLock<HashMap<String, String>>>;

pub fn new() -> State {
    Arc::new(RwLock::new(HashMap::new()))
}

pub fn get_all(state: &State) -> HashMap<String, String> {
    state.read().unwrap().clone()
}

pub fn to_json(state: &State) -> String {
    serde_json::to_string(&get_all(state)).unwrap_or_else(|_| "{}".to_string())
}

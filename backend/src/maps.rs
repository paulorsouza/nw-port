//! Which game server serves each map (one dedicated server process per map).
//!
//! `NW_MAP_SERVERS="L_SouthWindsward=host:7777,L_QuedaEterna=host:7778,L_Arena=host:7779"`. The address is what the
//! clients connect to. Empty = not configured: the client uses its own `-nwconnect` address (one server only).

use std::collections::HashMap;

/// Map a new character starts on (default of `characters.map`, migration 0001).
pub const DEFAULT_MAP: &str = "L_SouthWindsward";

#[derive(Debug, Default, Clone)]
pub struct MapServers {
    servers: HashMap<String, String>,
}

/// World short name, as the game server saves it: `/Game/NW/Maps/L_Arena.L_Arena` -> `L_Arena`.
pub fn short_name(map: &str) -> &str {
    let short = map.trim().rsplit('/').next().unwrap_or_default();
    short.split('.').next().unwrap_or_default()
}

/// `/Game/NW/Maps/L_Arena.L_Arena`, `L_Arena` and `l_arena` are the same map.
pub fn map_key(map: &str) -> String {
    short_name(map).to_ascii_lowercase()
}

impl MapServers {
    pub fn parse(text: &str) -> anyhow::Result<Self> {
        let mut servers = HashMap::new();
        for entry in text.split([',', ';']).map(str::trim).filter(|e| !e.is_empty()) {
            let (map, address) = entry
                .split_once('=')
                .ok_or_else(|| anyhow::anyhow!("NW_MAP_SERVERS: '{entry}' is not map=host:port"))?;
            let (map, address) = (map_key(map), address.trim());
            anyhow::ensure!(!map.is_empty() && !address.is_empty(), "NW_MAP_SERVERS: '{entry}' is not map=host:port");
            anyhow::ensure!(servers.insert(map, address.to_string()).is_none(), "NW_MAP_SERVERS: map repeated in '{entry}'");
        }
        Ok(Self { servers })
    }

    pub fn is_configured(&self) -> bool {
        !self.servers.is_empty()
    }

    /// Server of this map, if configured.
    pub fn address(&self, map: &str) -> Option<&str> {
        self.servers.get(&map_key(map)).map(String::as_str)
    }

    /// Where a character saved on `map` enters: its map, or the start map when that one is not served.
    pub fn entry(&self, map: &str) -> Option<(String, &str)> {
        if let Some(address) = self.address(map) {
            return Some((map.to_string(), address));
        }
        self.address(DEFAULT_MAP).map(|a| (DEFAULT_MAP.to_string(), a))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_and_finds() {
        let m = MapServers::parse(" L_SouthWindsward=10.0.0.2:7777, /Game/NW/Maps/L_Arena.L_Arena = 10.0.0.2:7779 ").unwrap();
        assert_eq!(m.address("L_Arena"), Some("10.0.0.2:7779"));
        assert_eq!(m.address("l_southwindsward"), Some("10.0.0.2:7777"));
        assert_eq!(m.address("L_QuedaEterna"), None);
        assert_eq!(m.entry("L_QuedaEterna"), Some((DEFAULT_MAP.to_string(), "10.0.0.2:7777")));
        assert_eq!(short_name("/Game/NW/Maps/L_Arena.L_Arena"), "L_Arena");
        assert!(MapServers::parse("").unwrap().entry("L_Arena").is_none());
        assert!(MapServers::parse("L_Arena").is_err());
        assert!(MapServers::parse("L_Arena=a:1,L_Arena=b:2").is_err());
    }
}

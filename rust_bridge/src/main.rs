use std::env;
use std::io::{self, BufRead, Write};
use std::net::TcpStream;

fn send_state(addr: &str, state: bool) -> io::Result<()> {
    let mut stream = TcpStream::connect(addr)?;
    let payload = if state { "ON\n" } else { "OFF\n" };
    stream.write_all(payload.as_bytes())?;
    stream.flush()
}

fn main() -> io::Result<()> {
    let addr = env::var("EXTERNAL_TOGGLE_ADDR").unwrap_or_else(|_| "127.0.0.1:10080".to_string());
    println!("Sending external toggle updates to {addr}");
    println!("Commands: 'on', 'off', 'toggle', 'quit'. Press Ctrl+C to exit.");

    let stdin = io::stdin();
    let mut current_state = false;
    for line in stdin.lock().lines() {
        let input = line?.trim().to_ascii_lowercase();
        match input.as_str() {
            "on" => current_state = true,
            "off" => current_state = false,
            "toggle" => current_state = !current_state,
            "quit" | "exit" => break,
            other if other.is_empty() => continue,
            _ => {
                eprintln!("Unknown command: {input}");
                continue;
            }
        }

        if let Err(err) = send_state(&addr, current_state) {
            eprintln!("Failed to send state to {addr}: {err}");
        } else {
            println!("Sent state {current_state} to {addr}");
        }
    }

    Ok(())
}

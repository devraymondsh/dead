use crate::MessageSocketConfig;
use std::env;

pub enum ClientKind {
    Guesser,
    Chooser,
}

pub fn parse_cli(is_server: bool) -> Result<(MessageSocketConfig, ClientKind), String> {
    let args: Vec<String> = env::args().collect();

    let mut kind = ClientKind::Guesser;
    let mut cli = MessageSocketConfig {
        port: 8080,
        unix_socket: false,
        pass: String::new(),
        unix_socket_file: String::from("/tmp/guessing-game-unix-socket"),
    };

    if is_server {
        if args.len() < 2 {
            return Err(format!(
                "Invalid number of arguments! The password should be passed as the first argument."
            ));
        }
    } else {
        if args.len() < 3 {
            return Err(format!(
                "Invalid number of arguments! The password should be passed as the first argument and the game type as the second one."
            ));
        }
    }
    cli.pass = args[1].clone();

    if args.len() >= 3 {
        let prot_idx: usize = if is_server { 2 } else { 3 };
        let listen_idx: usize = prot_idx + 1;

        if !is_server {
            if args[prot_idx - 1] == String::from("guesser") {
                kind = ClientKind::Guesser;
            } else if args[prot_idx - 1] == String::from("chooser") {
                kind = ClientKind::Chooser;
            } else {
                return Err(format!(
                    "Invalid {}th argument! You should either pass `guesser` or `chooser`.",
                    prot_idx - 1
                ));
            }
        }

        if let Some(k) = args.get(prot_idx) {
            if *k == String::from("tcp") {
                cli.unix_socket = false;
            } else if *k == String::from("unix") {
                cli.unix_socket = true;
            } else {
                return Err(format!(
                    "Invalid {}th argument! You should either pass `tcp` or `unix`.",
                    prot_idx
                ));
            }
        }

        if let Some(k) = args.get(listen_idx) {
            if cli.unix_socket {
                cli.unix_socket_file = k.clone();
            } else {
                cli.port = k
                    .parse::<u16>()
                    .map_err(|_| "Invalid third argument! It's not a positive u16 number.")?;
            }
        }
    }

    Ok((cli, kind))
}

use luxinos::*;

fn main() {
    let mut uid: usize = 0;

    let (socket_config, kind) = parse_cli(false)
        .map_err(|msg| return console::fatal_error(msg))
        .unwrap();

    exit_on_sigint!();

    let mut stream = socket_config.auto_connect().unwrap();

    let m1 = stream.read().unwrap();
    if m1.kind == MessageKind::EnterPasswd {
        stream
            .write(Message::new_anonymous::<String>(
                &socket_config.pass,
                MessageKind::EnterPasswd,
            ))
            .unwrap();

        let m2 = stream.read().unwrap();
        if m2.kind != MessageKind::AssignUid {
            fatal_error("Wrong password!");
        }
        uid = m2.value();
    }
    println!("Your id is: {}", uid);
    println!("Initially, we should wait until enough users connect so the game can be started.");

    loop {
        let message = stream.read().unwrap();
        match message.kind {
            MessageKind::ShowOpponents => {
                if let ClientKind::Guesser = kind {
                    continue;
                }

                let users = message.value::<Vec<usize>>();
                println!("We got enough opponents to start. Choose an opponent.");
                for user in (&users).into_iter() {
                    if *user != uid {
                        println!("User {}", user);
                    }
                }

                let selected = read_line::<usize>().unwrap();
                if users
                    .into_iter()
                    .find(|v| *v == selected && *v != uid)
                    .is_none()
                {
                    fatal_error("Invalid opponent selection!");
                }

                stream
                    .write(Message::new::<usize>(
                        &selected,
                        uid,
                        MessageKind::SelectOpponent,
                    ))
                    .unwrap();
            }
            _ => {}
        }
    }
}

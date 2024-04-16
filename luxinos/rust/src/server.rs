use luxinos::*;
use std::{thread, time::Duration};

struct User {
    pub id: usize,
    pub is_finished: bool,
    pub stream: Box<dyn MessageStream>,
}
struct Users {
    users: Vec<User>,
}
impl Users {
    pub fn new() -> Self {
        Self { users: Vec::new() }
    }

    pub fn new_user(self: &mut Self, stream: Box<dyn MessageStream>) -> usize {
        let id = self.users.len() + 1;
        self.users.push(User {
            id,
            stream,
            is_finished: false,
        });

        id
    }

    pub fn get_available_users(self: &Self) -> Vec<usize> {
        let mut users: Vec<usize> = Vec::new();
        for user in &self.users {
            if !user.is_finished {
                users.push(user.id);
            }
        }

        users
    }

    pub fn len(self: &Self) -> usize {
        self.users.len()
    }
}

fn main() {
    let (socket_config, _) = parse_cli(true)
        .map_err(|msg| return console::fatal_error(msg))
        .unwrap();

    exit_on_sigint!();

    let mut users = Users::new();

    let server = socket_config.auto_bind().unwrap();
    loop {
        if let Ok(mut s) = server.accept() {
            let config_pass = socket_config.pass.clone();
            let id = users.new_user(s.try_clone().unwrap());

            thread::spawn(move || {
                s.write(Message::new_anonymous::<()>(&(), MessageKind::EnterPasswd))
                    .unwrap_or_else(|_| return);

                let message = s.read().unwrap();
                if message.value::<String>() == config_pass {
                    s.write(Message::new_anonymous::<usize>(&id, MessageKind::AssignUid))
                        .unwrap_or_else(|_| return);
                } else {
                    s.write(Message::new_anonymous::<()>(&(), MessageKind::WrongPasswd))
                        .unwrap_or_else(|_| return);
                }

                loop {
                    let message = s.read().unwrap();
                    match message.kind {
                        MessageKind::ShowOpponents => {}
                        _ => {}
                    }
                }
            });

            if users.len() >= 2 {
                let available_users = users.get_available_users();
                for u in users.users.iter_mut() {
                    if u.id == id {
                        thread::sleep(Duration::from_secs(2));
                    }

                    u.stream
                        .write(Message::new::<Vec<usize>>(
                            &available_users,
                            u.id,
                            MessageKind::ShowOpponents,
                        ))
                        .unwrap_or_else(|_| return);
                }
            }
        }
    }
}

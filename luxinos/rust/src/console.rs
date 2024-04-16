use std::{io, process, str::FromStr};

pub fn fatal_error(msg: impl ToString) {
    eprintln!("{}", msg.to_string());
    process::exit(1);
}

pub fn read_line<T: FromStr>() -> Result<T, T::Err> {
    let mut stdin = String::new();
    io::stdin().read_line(&mut stdin).unwrap();

    stdin.trim().parse()
}

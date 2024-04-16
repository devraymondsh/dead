use crate::Message;
use std::{
    io::{Error as IoError, Read, Write},
    mem::{self},
    net::{SocketAddr, TcpListener, TcpStream},
    os::unix::net::{UnixListener, UnixStream},
};

#[derive(Debug, Clone)]
pub struct MessageSocketConfig {
    pub port: u16,
    pub pass: String,
    pub unix_socket: bool,
    pub unix_socket_file: String,
}
impl MessageSocketConfig {
    pub fn auto_connect(self: &Self) -> Result<Box<dyn MessageStream>, IoError> {
        Ok(if !self.unix_socket {
            Box::new(TcpStream::connect(SocketAddr::from((
                [127, 0, 0, 1],
                self.port,
            )))?)
        } else {
            Box::new(UnixStream::connect(&self.unix_socket_file)?)
        })
    }

    pub fn auto_bind(self: &Self) -> Result<Box<dyn MessageListener>, IoError> {
        Ok(if !self.unix_socket {
            Box::new(TcpListener::bind(SocketAddr::from((
                [127, 0, 0, 1],
                self.port,
            )))?)
        } else {
            Box::new(UnixListener::bind(&self.unix_socket_file)?)
        })
    }
}

pub trait MessageStream: Send + Sync {
    fn read(self: &mut Self) -> Result<Message, IoError>;
    fn write(self: &mut Self, message: Message) -> Result<(), IoError>;
    fn try_clone(&self) -> Result<Box<dyn MessageStream>, IoError>;
}
impl MessageStream for TcpStream {
    fn read(self: &mut Self) -> Result<Message, IoError> {
        let mut bytes = [0; mem::size_of::<Message>()];
        Read::read_exact(self, bytes.as_mut_slice())?;

        Ok(Message::from(Vec::from(bytes.as_mut_slice())))
    }

    fn write(self: &mut Self, message: Message) -> Result<(), IoError> {
        let bytes: Vec<u8> = message.into();
        self.write_all(&bytes)
    }

    fn try_clone(&self) -> Result<Box<dyn MessageStream>, IoError> {
        Ok(Box::new(self.try_clone()?))
    }
}
impl MessageStream for UnixStream {
    fn read(self: &mut Self) -> Result<Message, IoError> {
        let mut buf: Vec<u8> = Vec::with_capacity(mem::size_of::<Message>());
        Read::read_exact(self, &mut buf)?;

        Ok(Message::from(buf))
    }

    fn write(self: &mut Self, message: Message) -> Result<(), IoError> {
        let bytes: Vec<u8> = message.into();
        self.write_all(&bytes)
    }

    fn try_clone(&self) -> Result<Box<dyn MessageStream>, IoError> {
        Ok(Box::new(self.try_clone()?))
    }
}

pub trait MessageListener: Send + Sync {
    fn accept(self: &Self) -> Result<Box<dyn MessageStream>, IoError>;
}
impl MessageListener for TcpListener {
    fn accept(self: &Self) -> Result<Box<dyn MessageStream>, IoError> {
        Ok(Box::new(self.accept()?.0))
    }
}
impl MessageListener for UnixListener {
    fn accept(self: &Self) -> Result<Box<dyn MessageStream>, IoError> {
        Ok(Box::new(self.accept()?.0))
    }
}

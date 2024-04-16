use std::{
    mem::{self},
    ptr,
};

type MessageRawBytes = [u8; 1000];

/// Message kind.
#[derive(Debug, Clone, PartialEq)]
#[repr(C)]
pub enum MessageKind {
    /// Terminates the connection.
    Exit = 0,
    /// Asks (server) or enters (client) the password.
    EnterPasswd,
    /// Says the password is wrong.
    WrongPasswd,
    /// Assigns a user id.
    AssignUid,
    /// Shows available opponents.
    ShowOpponents,
    /// Accepts (server) or selects (client) an opponent.
    SelectOpponent,
    /// Asks the opponent to be a guesser.
    AskOpponent,
    /// Guesses.
    Guess,
    /// Shows that the guess is wrong.
    WrongGuess,
    /// Sends a hint to the guesser.
    Hint,
    /// Announces that the guess is correct.
    CorrectGuess,
}

unsafe impl Send for MessageKind {}
unsafe impl Sync for MessageKind {}

pub trait MessageType {
    fn to_value(message: &Message) -> Self;
    fn to_message(self: &Self, id: usize, kind: MessageKind) -> Message;
}
impl MessageType for () {
    fn to_message(self: &Self, id: usize, kind: MessageKind) -> Message {
        Message::new_raw(id, kind, 0)
    }

    fn to_value(_: &Message) -> Self {
        ()
    }
}
impl MessageType for usize {
    fn to_message(self: &Self, id: usize, kind: MessageKind) -> Message {
        let mut message = Message::new_raw(id, kind, mem::size_of::<usize>());
        unsafe {
            ptr::copy(
                &mut (self.clone()) as *mut usize as *mut u8,
                &mut message.raw_bytes as *mut [u8; 1000] as *mut u8,
                mem::size_of::<usize>(),
            )
        };

        return message;
    }

    fn to_value(message: &Message) -> Self {
        unsafe { *(&message.raw_bytes as *const [u8; 1000] as *const usize) }
    }
}
impl MessageType for String {
    fn to_message(self: &Self, id: usize, kind: MessageKind) -> Message {
        let mut str_bytes = self.bytes().collect::<Vec<u8>>();
        let mut message = Message::new_raw(id, kind, self.len());
        if str_bytes.len() <= mem::size_of::<MessageRawBytes>() {
            unsafe {
                ptr::copy(
                    str_bytes.as_mut_ptr(),
                    &mut message.raw_bytes as *mut [u8; 1000] as *mut u8,
                    str_bytes.len(),
                )
            };
        }

        message
    }

    fn to_value(message: &Message) -> Self {
        let b = Box::leak(Box::new(message.raw_bytes.clone()));
        unsafe {
            String::from_raw_parts(
                b as *mut [u8; 1000] as *mut u8,
                message.raw_bytes_len,
                message.raw_bytes_len,
            )
        }
    }
}
impl MessageType for Vec<usize> {
    fn to_message(self: &Self, id: usize, kind: MessageKind) -> Message {
        let count = self.len() * mem::size_of::<usize>();
        let mut message = Message::new_raw(id, kind, self.len());
        if count <= mem::size_of::<MessageRawBytes>() {
            unsafe {
                ptr::copy(
                    self.clone().as_mut_ptr(),
                    &mut message.raw_bytes as *mut [u8; 1000] as *mut usize,
                    count,
                )
            };
        }

        message
    }

    fn to_value(message: &Message) -> Self {
        let b = Box::leak(Box::new(message.raw_bytes.clone()));
        unsafe {
            Vec::from_raw_parts(
                b as *mut [u8; 1000] as *mut usize,
                message.raw_bytes_len,
                message.raw_bytes_len,
            )
        }
    }
}

/// Message buffer memory layout:
/// [<12-byte information><raw bytes + 4 bytes of padding>]
#[derive(Debug, Clone)]
#[repr(C)]
pub struct Message {
    pub id: usize,
    pub kind: MessageKind,
    pub raw_bytes_len: usize,
    pub raw_bytes: MessageRawBytes,
}

unsafe impl Send for Message {}
unsafe impl Sync for Message {}

impl From<Vec<u8>> for Message {
    fn from(mut v: Vec<u8>) -> Self {
        unsafe { ptr::read(v.as_mut_ptr() as *mut Message) }
    }
}

impl Into<Vec<u8>> for Message {
    fn into(self) -> Vec<u8> {
        unsafe {
            Vec::from_raw_parts(
                Box::into_raw(Box::from(self)) as *mut u8,
                mem::size_of::<Message>(),
                mem::size_of::<Message>(),
            )
        }
    }
}
impl Message {
    pub fn new_raw(id: usize, kind: MessageKind, raw_bytes_len: usize) -> Self {
        Self {
            id,
            kind,
            raw_bytes_len,
            raw_bytes: unsafe { mem::zeroed() },
        }
    }

    pub fn new<T: MessageType>(v: &T, id: usize, kind: MessageKind) -> Self {
        T::to_message(v, id, kind)
    }

    pub fn new_anonymous<T: MessageType>(v: &T, kind: MessageKind) -> Self {
        Self::new::<T>(v, 0, kind)
    }

    pub fn value<T: MessageType>(self: &Self) -> T {
        T::to_value(self)
    }
}

use std::ffi::{c_char, c_int};

extern "C" {
    pub fn printf(fmt: *const c_char, ...) -> c_int;
    pub fn signal(signal: c_int, handler: unsafe extern "C" fn(signal_num: c_int) -> ());
}

pub fn safe_signal(signal_num: c_int, handler: unsafe extern "C" fn(signal_num: c_int) -> ()) {
    unsafe { signal(signal_num, handler) }
}

#[macro_export]
macro_rules! handle_sigint {
    () => {
        use std::ffi::c_int;

        const SIGINT: c_int = 2;
        static mut CRTL_C_RECEIVED: bool = false;
        unsafe extern "C" fn sigint_handler(_: c_int) -> () {
            CRTL_C_RECEIVED = true;
        }
        safe_signal(SIGINT, sigint_handler);
    };
}

#[macro_export]
macro_rules! exit_on_sigint {
    () => {
        const SIGINT: ::std::ffi::c_int = 2;
        static mut CRTL_C_RECEIVED: bool = false;
        unsafe extern "C" fn sigint_handler(_: ::std::ffi::c_int) -> () {
            CRTL_C_RECEIVED = true;

            let string = ::std::ffi::CString::new("\nCrtl+C received.\n").unwrap();
            printf(string.as_ptr());

            ::std::process::exit(0);
        }
        safe_signal(SIGINT, sigint_handler);
    };
}

#[macro_export]
macro_rules! sigint_recv {
    () => {
        unsafe { CRTL_C_RECEIVED }
    };
}

#[macro_export]
macro_rules! break_on_sigint {
    () => {
        if sigint_recv!() {
            break;
        }
    };
}

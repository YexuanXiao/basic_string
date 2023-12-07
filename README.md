# basic_string

A fast, clean string implementation, aiming to provide an approximation of std::basic_string using standard C++23 with legal and portable code. The implementation maximum optimizes the short string optimization, and avoids self-referencing. It does not implement: write-out functions, find functions, and interfaces using allocators. The implementation is exception safety and suitable for teaching purposes.

Progress: Completed most of the functions, missing insert and erase functions, missing coverage test.
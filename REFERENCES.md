# Bibliography & Reading List

Running list of papers, blogs, and manuals I'm actually using to build this right:

- *The LMAX Architecture* (Martin Fowler) - The main inspiration for the ring buffer design, single-threaded bounded contexts, and the whole concept of Mechanical Sympathy.
- *Intel® 64 and IA-32 Architectures Optimization Reference Manual* - Crucial for understanding cache line sizes (64 bytes), false sharing prevention, and branching.
- *C++ Concurrency in Action* (Anthony Williams) - The absolute bible for `std::atomic` memory orderings (acquire/release vs sequentially consistent).
- *Project Panama (JEP 454)* - Java Foreign Function & Memory API documentation and mailing list discussions.

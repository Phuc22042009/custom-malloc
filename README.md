# Custom Malloc in C++

A simple 64-bit dynamic memory allocator built from scratch in C++.

---

## Why I Built This

Hi! I am a high school student interested in computer systems and low-level software. 

Whenever I wrote my program, I always used malloc a.k.a new in C++ without thinking much about what was happening underneath. I wanted to understand how memory management actually works under the hood:
- How does a program ask the operating system for raw memory?
- How does it divide that memory into pieces for variables?
- How does it track which blocks are used and which are free?

To answer these questions, I decided to build my own memory allocator from scratch using a doubly linked list.

---

## How It Works

1. **Requesting Memory Pages from OS:**  
   Instead of using standard library allocation, the allocator requests raw memory pages directly from the OS (`VirtualAlloc` on Windows and `mmap` on Linux).

2. **Block Headers:**  
   Every allocated or free chunk of memory is preceded by a small header (`struct Block`) that stores:
   - `length`: Size of the usable payload.
   - `is_free`: A boolean flag indicating whether the block is currently in use.
   - `prev` / `next`: Pointers to adjacent blocks in the heap.
   - `marker`: A safety magic byte (`0x43`) to detect invalid pointers.

3. **First-Fit Search:**  
   When `malloc(size)` is called, it runs through the linked list and grabs the first free block that is large enough.

4. **Block Splitting:**  
   If a free block is significantly larger than the requested size, it gets split into two: one for the user, and the remainder left as a new free block for future requests (reducing internal fragmentation).

5. **Block Merging:**  
   When `free(ptr)` is called, the allocator checks if the adjacent neighbors (`prev` and `next`) are also free. If so, it merges them back into one larger block to avoid external fragmentation.

6. **16-byte Alignment:**  
   Modern 64-bit CPUs require addresses to be aligned (typically 16 bytes for SIMD/AVX safety). All sizes are rounded up to multiples of 16 using bitwise math, and headers are declared with `alignas(16)`.

7. **Supported Functions:**
   - `malloc(size)`: Allocates raw memory.
   - `free(ptr)`: Frees memory and merges adjacent free blocks.
   - `calloc(num, size)`: Allocates memory and clears all bytes to zero (`memset 0`).
   - `realloc(ptr, new_size)`: Expands or shrinks an existing block, expanding in-place if the neighboring block is free.

---

## Memory Diagram

```text
+-------------------------------------------------------------------------------+
|                             VIRTUAL MEMORY PAGE                               |
+-------------------+-----------------------+-----------------------+-----------+
|   GlobalHeader    |     Block 1 Header    |    Payload 1 (Data)   |  Block 2  |
|  (first_block...) | (marker, free, len...) |  (returned to user)   |    ...    |
+-------------------+-----------------------+-----------------------+-----------+
                    ^                       ^
              my_header + 1           this + 1 (payload)
```

- When the user calls `malloc`, we return `this + 1` (the address right after the header).
- When the user calls `free(ptr)`, we do `(Block*)ptr - 1` to step backward and recover the header in $O(1)$ time.

---

## Challenges and What I Learned

- **Pointer arithmetic in C++:** Learning that `ptr + 1` jumps by `sizeof(*ptr)` bytes rather than 1 byte.
- **Handling merging correctly:** The trickiest bug was updating the pointers when merging both `prev` and `next` blocks simultaneously. If you merge with `prev`, your active pointer must shift to `prev_block` before checking `next`.
- **Memory alignment:** Understanding why computers don't like odd addresses and how unaligned access can cause performance penalties or crashes on 64-bit systems.

---

## Repository Files

- `malloc.cpp`: Core implementation of `SimpleDoublyLinkedMalloc`.
- `main.cpp`: Test file running sample allocations, splitting, coalescing, calloc, and realloc.
- `LICENSE`: MIT License.

---

## How to Build and Run

### On Windows:
```powershell
g++ -o main.exe main.cpp
.\main.exe
```

### On Linux:
```bash
g++ -o main main.cpp
./main
```

### Sample Output:
```text
Test 1: Malloc and Free
Array values: 10 20 30 40 50 
Freed successfully

Test 2: Splitting
Address p1: 0x201ea300040
Address p2: 0x201ea300080
Freed p1 and p2

Test 3: Calloc
Calloc values: 0 0 0 0 0 
Freed calloc

Test 4: Realloc
Before realloc: Hello
After realloc: Hello World from Custom Malloc!
Freed realloc
```

---

## License
This project is licensed under the [MIT License](LICENSE).

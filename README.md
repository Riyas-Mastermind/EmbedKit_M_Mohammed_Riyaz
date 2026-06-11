# EmbedKit_M_Mohammed_Riyaz

**Author:** M Mohammed Riyaz

## Modules
* `ringbuf.c`: A fixed-capacity circular buffer implementation for `uint8_t` data, optimized with bitwise arithmetic for high-performance environments.

## Build Instructions
This project uses the C99 standard. To compile the code with all warnings enabled (as required), navigate to the directory and run:

```bash
gcc -Wall -std=c99 ringbuf.c -o ring_buffer
```

### After compiling, execute the binary:
```bash
./ringbuf
```


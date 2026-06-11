#include <stdint.h>

#define BUFFER_SIZE 8

typedef struct {
    uint8_t storage[BUFFER_SIZE]; // ONE array with 8 data slots
    uint8_t head;                 // Index for the next write (0 to 7)
    uint8_t tail;                 // Index for the next read (0 to 7)
    uint8_t count;                // Tracks total unread items (0 to 8)
} RingBuffer_t;

int main(void) {
    // Create exactly ONE instance of your ring buffer box
    RingBuffer_t my_fifo;

    // 1. Initialize it (clears head, tail, and count to 0)
    ringbuf_init(&my_fifo);

    // 2. Write data to it
    // The function updates 'storage', 'head', and 'count' inside 'my_fifo'
    if (ringbuf_write(&my_fifo, 0xA1) == 0) {
        printf("Successfully wrote 0xA1\n");
    }

    // 3. Read data from it
    uint8_t rx_data;
    if (ringbuf_read(&my_fifo, &rx_data) == 0) {
        printf("Successfully read data: 0x%02X\n", rx_data);
    }

    return 0;
}
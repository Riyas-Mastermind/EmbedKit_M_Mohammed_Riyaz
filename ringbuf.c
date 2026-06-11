#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE                 8

/* Return codes for buffer operations */
#define RINGBUF_SUCCESS             0
#define RINGBUF_ERROR              -1

typedef struct {
    uint8_t storage[BUFFER_SIZE]; /* ONE array with 8 data slots */
    uint8_t head;                 /* Index for the next write (0 to 7) */
    uint8_t tail;                 /* Index for the next read (0 to 7) */
    uint8_t count;                /* Tracks total unread items (0 to 8) */
} RingBuffer_t;

/* --- Function API Prototypes --- */
void ringbuf_init(RingBuffer_t *rb);
int ringbuf_write(RingBuffer_t *rb, uint8_t data);
int ringbuf_read(RingBuffer_t *rb, uint8_t *data);
uint8_t ringbuf_count(const RingBuffer_t *rb);
int ringbuf_is_full(const RingBuffer_t *rb);
int ringbuf_is_empty(const RingBuffer_t *rb);

/* --- Function Implementations --- */

/**
 * Initializes the ring buffer to a clean, empty state.
 */
void ringbuf_init(RingBuffer_t *rb) {
    if (rb != NULL) {
        rb->head = 0;
        rb->tail = 0;
        rb->count = 0;
    }
}

/**
 * Checks whether the buffer is completely full.
 * Returns 1 if full, 0 otherwise.
 */
int ringbuf_is_full(const RingBuffer_t *rb) {
    return (rb != NULL && rb->count == BUFFER_SIZE);
}

/**
 * Checks whether the buffer is completely empty.
 * Returns 1 if empty, 0 otherwise.
 */
int ringbuf_is_empty(const RingBuffer_t *rb) {
    return (rb != NULL && rb->count == 0);
}

/**
 * Queries how many bytes are currently stored in the buffer.
 */
uint8_t ringbuf_count(const RingBuffer_t *rb) {
    if (rb != NULL) {
        return rb->count;
    }
    return 0;
}

/**
 * Writes one byte into the buffer. 
 * Returns RINGBUF_SUCCESS on success, or RINGBUF_ERROR if the buffer is full.
 */
int ringbuf_write(RingBuffer_t *rb, uint8_t data) {
    if (rb == NULL || ringbuf_is_full(rb)) {
        return RINGBUF_ERROR;
    }

    rb->storage[rb->head] = data;

    /* * BONUS TASK IMPLEMENTATION:
     * Instead of using the standard modulo operator: rb->head = (rb->head + 1) % BUFFER_SIZE;
     * we utilize a bitwise AND operation with a mask: (BUFFER_SIZE - 1).
     *
     * WHY THIS IS FASTER ON MCUs WITHOUT A HARDWARE DIVIDER:
     * The standard modulo (%) operator compiles into a hardware division instruction (e.g., IDIV). 
     * On lightweight or lower-end microcontrollers (like many ARM Cortex-M0/M0+, AVR, or MSP430 devices) 
     * that lack a dedicated hardware divider unit, division must be emulated in software via a mathematical 
     * routine. This emulation takes dozens of CPU clock cycles. Conversely, a bitwise AND ('&') instruction 
     * executes natively inside the Arithmetic Logic Unit (ALU) in exactly ONE single clock cycle. When a ring 
     * buffer operates within a high-frequency Interrupt Service Routine (ISR) like a UART receive context, 
     * saving dozens of cycles per byte prevents CPU starvation and latency issues.
     *
     * WHY IT ONLY WORKS WHEN BUFFER_SIZE IS A POWER OF 2:
     * When a number is a power of two (e.g., 8, which is 2^3, binary: 00001000), subtracting 1 yields a continuous 
     * sequence of lower-order binary set bits (e.g., 7, binary: 00000111). Bitwise ANDing an incrementing 
     * pointer with this sequence clears all upper overflow bits and leaves only the index bits untouched. 
     * For example, as the index increments:
     * 7 (0111) & 7 (0111) = 7
     * 8 (1000) & 7 (0111) = 0  <-- Flawless, automated wrap-around back to index 0.
     * If the buffer size were a non-power-of-two (e.g., 6), BUFFER_SIZE - 1 would be 5 (binary: 0101). 
     * An incrementing sequence masked with 0101 would skip indices entirely (e.g., index 2 (0010) & 0101 = 0), 
     * corrupting the structural logic of the queue.
     */
    rb->head = (rb->head + 1) & (BUFFER_SIZE - 1);
    rb->count++;

    return RINGBUF_SUCCESS;
}

/**
 * Reads one byte from the buffer.
 * Returns RINGBUF_SUCCESS on success, or RINGBUF_ERROR if the buffer is empty.
 */
int ringbuf_read(RingBuffer_t *rb, uint8_t *data) {
    if (rb == NULL || data == NULL || ringbuf_is_empty(rb)) {
        return RINGBUF_ERROR;
    }

    *data = rb->storage[rb->tail];
    
    /* BONUS TASK IMPLEMENTATION: Bitwise pointer wrap-around wrap */
    rb->tail = (rb->tail + 1) & (BUFFER_SIZE - 1);
    rb->count--;

    return RINGBUF_SUCCESS;
}

/* --- Main Application Testing Sequence --- */
int main(void) {
    RingBuffer_t my_fifo;
    uint8_t test_data;
    uint8_t initial_bytes[8] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48};
    uint8_t secondary_bytes[3] = {0x49, 0x4A, 0x4B};
    int i;

    /* Initialize the buffer box */
    ringbuf_init(&my_fifo);

    /* ------------------------------------------------------------------------
     * 1. Write the 8 bytes 0x41 to 0x48 one at a time.
     * ------------------------------------------------------------------------ */
    for (i = 0; i < 8; i++) {
        if (ringbuf_write(&my_fifo, initial_bytes[i]) == RINGBUF_SUCCESS) {
            printf("[WRITE] 0x%02X -> OK (count=%u)", initial_bytes[i], (unsigned int)ringbuf_count(&my_fifo));
            if (ringbuf_is_full(&my_fifo)) {
                printf(" FULL");
            }
            printf("\n");
        }
    }

    /* ------------------------------------------------------------------------
     * 2. Attempt to write one more byte (0x99). Expect failure.
     * ------------------------------------------------------------------------ */
    if (ringbuf_write(&my_fifo, 0x99) == RINGBUF_ERROR) {
        printf("[WRITE] 0x99 -> FAIL (buffer full)\n");
    }

    /* ------------------------------------------------------------------------
     * 3. Read 3 bytes one at a time. Expecting 0x41, 0x42, 0x43.
     * ------------------------------------------------------------------------ */
    for (i = 0; i < 3; i++) {
        if (ringbuf_read(&my_fifo, &test_data) == RINGBUF_SUCCESS) {
            printf("[READ] -> 0x%02X (count=%u)\n", test_data, (unsigned int)ringbuf_count(&my_fifo));
        }
    }

    /* ------------------------------------------------------------------------
     * 4. Write 3 new bytes 0x49, 0x4A, 0x4B. Reusing slots.
     * ------------------------------------------------------------------------ */
    for (i = 0; i < 3; i++) {
        if (ringbuf_write(&my_fifo, secondary_bytes[i]) == RINGBUF_SUCCESS) {
            printf("[WRITE] 0x%02X -> OK (count=%u)", secondary_bytes[i], (unsigned int)ringbuf_count(&my_fifo));
            if (ringbuf_is_full(&my_fifo)) {
                printf(" FULL");
            }
            printf("\n");
        }
    }

    /* ------------------------------------------------------------------------
     * 5. Read all remaining 8 bytes one at a time.
     * ------------------------------------------------------------------------ */
    while (!ringbuf_is_empty(&my_fifo)) {
        if (ringbuf_read(&my_fifo, &test_data) == RINGBUF_SUCCESS) {
            printf("[READ] -> 0x%02X (count=%u)", test_data, (unsigned int)ringbuf_count(&my_fifo));
            if (ringbuf_is_empty(&my_fifo)) {
                printf(" EMPTY");
            }
            printf("\n");
        }
    }

    /* ------------------------------------------------------------------------
     * 6. Attempt to read from the now-empty buffer. Expect failure.
     * ------------------------------------------------------------------------ */
    if (ringbuf_read(&my_fifo, &test_data) == RINGBUF_ERROR) {
        printf("[READ] (empty) -> FAIL (buffer empty)\n");
    }

    return 0;
}
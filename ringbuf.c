#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* We lock our buffer size to 8. Remember, it MUST be a power of 2 for our bitwise magic to work later! */
#define BUFFER_SIZE                 8

/* Simple return codes so our main application knows if an operation lived or died */
#define RINGBUF_SUCCESS             0
#define RINGBUF_ERROR              -1

/* * The Ring Buffer structure. 
 * Think of this as the physical holding area in the microcontroller's RAM.
 */
typedef struct {
    uint8_t storage[BUFFER_SIZE];       /* The actual memory array where our bytes sleep */
    uint8_t head;                       /* The 'Write' pointer: Where do we drop the NEXT incoming byte? */
    uint8_t tail;                       /* The 'Read' pointer: Where is the OLDEST byte waiting to be read? */
    uint8_t count;                      /* The 'Capacity' tracker: How many unread bytes are currently sitting inside? */
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
 * If the MCU just booted up, we need to make absolutely sure our pointers 
 * and counters start at zero before UART interrupts start firing.
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
 * If our total unread items hit the BUFFER_SIZE (8), there is no room at the inn.
 */
int ringbuf_is_full(const RingBuffer_t *rb) {
    return (rb != NULL && rb->count == BUFFER_SIZE);
}

/**
 * Checks whether the buffer is completely empty.
 * If count is 0, the CPU has caught up and processed all the incoming data.
 */
int ringbuf_is_empty(const RingBuffer_t *rb) {
    return (rb != NULL && rb->count == 0);
}

/**
 * Queries how many bytes are currently waiting in line.
 * Great for checking if we have enough data to start a specific task.
 */
uint8_t ringbuf_count(const RingBuffer_t *rb) {
    if (rb != NULL) {
        return rb->count;
    }
    return 0;
}

/**
 * Writes one byte into the buffer. 
 * This is usually called inside a fast, high-priority hardware interrupt (like UART RX).
 */
int ringbuf_write(RingBuffer_t *rb, uint8_t data) {
    /* Safety first: If the buffer is full, we bounce the data and return an error. 
       We NEVER silently overwrite data that the main loop hasn't read yet! */
    if (rb == NULL || ringbuf_is_full(rb)) {
        return RINGBUF_ERROR;
    }

    /* Drop the new byte exactly where the 'head' pointer is currently looking */
    rb->storage[rb->head] = data;

    /* * --- THE BITWISE BONUS TRICK ---
     * Normally, to make the head wrap around to 0 when it hits 8, you'd write: 
     * rb->head = (rb->head + 1) % BUFFER_SIZE;
     * * But on standard microcontrollers, the modulo (%) operator triggers a massive software 
     * division routine that can steal dozens of CPU clock cycles. Inside a tight interrupt, 
     * that's a disaster.
     * * Because our BUFFER_SIZE is exactly 8 (binary 1000), BUFFER_SIZE - 1 is 7 (binary 0111).
     * By bitwise ANDing the incrementing head with 0111, it naturally strips away the overflow 
     * bit the second it hits 8, instantly snapping it back to 0. It takes exactly ONE clock 
     * cycle in the CPU's ALU. This is how the pros do it!
     */
    rb->head = (rb->head + 1) & (BUFFER_SIZE - 1);
    
    /* We just added a byte, so the line gets one person longer */
    rb->count++;

    return RINGBUF_SUCCESS;
}

/**
 * Reads one byte from the buffer.
 * This is usually called from our slow, background main() loop.
 */
int ringbuf_read(RingBuffer_t *rb, uint8_t *data) {
    /* Safety first: If there's no new data waiting for us, bail out. 
       We NEVER want to hand the application old, garbage data. */
    if (rb == NULL || data == NULL || ringbuf_is_empty(rb)) {
        return RINGBUF_ERROR;
    }

    /* Grab the oldest waiting byte from where the 'tail' pointer is looking */
    *data = rb->storage[rb->tail];
    
    /* Step the tail forward to the next oldest byte, using the exact same bitwise wrap trick */
    rb->tail = (rb->tail + 1) & (BUFFER_SIZE - 1);
    
    /* We just processed a byte, so the line gets one person shorter */
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

    /* Step 0: Assemble the queue and set pointers to zero */
    ringbuf_init(&my_fifo);

    /* ------------------------------------------------------------------------
     * 1. Let's pack the buffer until it's completely bursting.
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
     * 2. Try to shove one more byte (0x99) into the full buffer. 
     * Our safety checks should block this and return an error!
     * ------------------------------------------------------------------------ */
    if (ringbuf_write(&my_fifo, 0x99) == RINGBUF_ERROR) {
        printf("[WRITE] 0x99 -> FAIL (buffer full)\n");
    }

    /* ------------------------------------------------------------------------
     * 3. Now, let's process the first 3 bytes from the queue. 
     * This will free up 3 empty slots in our memory array.
     * ------------------------------------------------------------------------ */
    for (i = 0; i < 3; i++) {
        if (ringbuf_read(&my_fifo, &test_data) == RINGBUF_SUCCESS) {
            printf("[READ] -> 0x%02X (count=%u)\n", test_data, (unsigned int)ringbuf_count(&my_fifo));
        }
    }

    /* ------------------------------------------------------------------------
     * 4. Let's write 3 new bytes. The head pointer will wrap around 
     * and safely overwrite those 3 slots we just emptied.
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
     * 5. Time to clean house. We will loop and read bytes until the 
     * buffer runs completely dry.
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
     * 6. The buffer is empty. If we try to read again, it should fail 
     * safely instead of handing us garbage data from RAM.
     * ------------------------------------------------------------------------ */
    if (ringbuf_read(&my_fifo, &test_data) == RINGBUF_ERROR) {
        printf("[READ] (empty) -> FAIL (buffer empty)\n");
    }

    return 0;
}
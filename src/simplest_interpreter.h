#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME app

typedef struct tag_fifo_program_data_t {
	void *fifo_reserved;
	uint8_t data_byte;
} fifo_program_data_t;

void evaluate_fifo_buffer(void);

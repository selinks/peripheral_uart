#include <zephyr/types.h>
#include <zephyr/kernel.h>

#include "simplest_interpreter.h"

LOG_MODULE_DECLARE(LOG_MODULE_NAME, CONFIG_LOG_MAX_LEVEL);

extern struct k_fifo fifo_program;
extern char buf_program_output[20];
extern bool fifo_has_enter;

void evaluate_fifo_buffer(void) {
	uint8_t count = 0;
	fifo_program_data_t *buf;
    
	LOG_DBG("From evaluate_fifo_buffer()");
    buf = k_fifo_get(&fifo_program, K_NO_WAIT);
	while (buf) {
		if (buf->data_byte >= 97 && buf->data_byte <=122) {
			buf_program_output[count] = buf->data_byte - 32;
		}
		else {
			buf_program_output[count] = buf->data_byte;
		}
        count++;
		buf = k_fifo_get(&fifo_program, K_NO_WAIT);
	}
	buf_program_output[count] = 0;
	
	return;
}

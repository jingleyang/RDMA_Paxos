#include "../include/output/output.h"

#include "../include/output/crc64.h"

struct output_handler_t* init_output(view *cur_view)
{
	struct output_handler_t *output_handler = (struct output_handler_t*)malloc(sizeof(struct output_handler_t));
	memset((void*)output_handler, 0, sizeof(struct output_handler_t));
	output_handler->output_list = listCreate();
	output_handler->count = 0;
	output_handler->cur_view = cur_view;
	pthread_mutex_init(&output_handler->lock, NULL);
	return output_handler;
}

void store_output(const void *buf, ssize_t ret, struct output_handler_t* output_handler)
{
	const unsigned char *s = (const unsigned char *)buf;
	listNode *last_node = listLast(output_handler->output_list);
	uint64_t *new_hash = (uint64_t*)malloc(sizeof(uint64_t));
	if (last_node != NULL)
	{
		uint64_t *last_hash = (uint64_t*)listNodeValue(last_node);
		*new_hash = crc64(*last_hash, s, ret);
	} else {
		*new_hash = crc64(0, s, ret);
	}
	listAddNodeTail(output_handler->output_list, (void*)new_hash); // Add a new node to the list, to tail, containing the specified 'value' pointer as value. On error, NULL is returned.
	output_handler->count++;
	return;
}
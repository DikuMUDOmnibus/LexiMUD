#if !defined(_MEMORY_H_)
#define _MEMORY_H_

#include "types.h"

void Free_Error(char * file, int line);

#define ALLOC(a, b)		calloc(a, b)
						
#define FREE(ptr)		if (ptr == NULL) {						\
							Free_Error(__FUNCTION__, __LINE__);	\
						} else {								\
							free(ptr);							\
							ptr = NULL;							\
						}

#endif

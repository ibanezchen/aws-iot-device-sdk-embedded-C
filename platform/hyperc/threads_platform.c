#include "threads_interface.h"
#include "threads_platform.h"
#include "aws_iot_error.h"

#ifdef _ENABLE_THREAD_SUPPORT_

IoT_Error_t aws_iot_thread_mutex_init(IoT_Mutex_t *o)
{
	mut_init(&o->o);
	return 0;
}

IoT_Error_t aws_iot_thread_mutex_lock(IoT_Mutex_t *o)
{
	mut_lock(&o->o, WAIT);
	return SUCCESS;
}

IoT_Error_t aws_iot_thread_mutex_trylock(IoT_Mutex_t *o)
{
	if( !mut_lock(&o->o, WAIT_NO))
		return SUCCESS;
	else
		return MUTEX_LOCK_ERROR;
}

IoT_Error_t aws_iot_thread_mutex_unlock(IoT_Mutex_t *o)
{
	mut_unlock(&o->o);
	return SUCCESS;
}

IoT_Error_t aws_iot_thread_mutex_destroy(IoT_Mutex_t *o)
{
	return SUCCESS;
}
#endif

#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(example_load);
SWITCH_MODULE_RUNTIME_FUNCTION(example_runtime);
SWITCH_MODULE_SHUTDOWN_FUNCTION(example_shutdown);

static struct {
	int debug;
	int looping;
	int print;	
	int string;
	int32_t threads;
	int32_t running;
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;
} globals;

struct call_helper {
	switch_memory_pool_t *pool;
};

SWITCH_MODULE_DEFINITION(mod_example, example_load, example_shutdown, NULL);

static switch_status_t example_on_state_change(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	const char *change_count;
	switch_channel_state_t state;
	char chan_state[64];

	channel = switch_core_session_get_channel(session);

	change_count = switch_channel_get_variable(channel, "change_count");
	if(change_count)
	{
		int number = atoi(change_count);
		number++;
		switch_channel_set_variable_printf(channel,"count", "%d",number);
	} else {
		switch_channel_set_variable(channel,"change_count", "1");
	}

	state = switch_channel_get_state(channel);

	switch(state){
		case CS_HANGUP:
			sprintf(chan_state, "CS_HANGUP");
			break;
		case CS_INIT:
			sprintf(chan_state, "CS_INIT");
			break;
		default:
			break;
	}
		
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "STATE CHANGED 	to %d (%s), change_count is %s call_state is %s\n", switch_channel_get_state(channel), chan_state, switch_channel_get_variable(channel, "change_count"), switch_channel_callstate2str(switch_channel_get_callstate(channel)));
	
	return SWITCH_STATUS_SUCCESS;
}

switch_state_handler_table_t example_state_handler = {
		/* on_init */ example_on_state_change,
		/* on_routing */ NULL,
		/* on_execute */ NULL,
		/* on_hangup */ example_on_state_change,
		/* on_exch_media */ NULL,
		/* on_sof_exec */ NULL,
		/* on_consume_med */ NULL,
		/* on_hibernate */ NULL,
		/* on_reset */ NULL,
		/* on_park */ NULL,
		/* on_reporting */ NULL,
		/* on_destroy */ NULL
};

void example_event_handler(switch_event_t *event)
{
}

static switch_status_t do_config() {
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_APP(example_app)
{
	//return SWITCH_STATUS_SUCCESS;	
}

SWITCH_STANDARD_API(example_api)
{
	return SWITCH_STATUS_SUCCESS;
}

static void *SWITCH_THREAD_FUNC outbound_agent_thread_run(switch_thread_t *thread, void *obj){
	switch_mutex_lock(globals.mutex);
	globals.threads++;
	switch_mutex_unlock(globals.mutex);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AGENT_DISPATCH_THREAD_RUN");
	
	while(globals.running == 1) {
		if(globals.print)
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Runtime!\n");
		switch_yield(100000);
		
		if(globals.running == 0) {
			break;
		}
	}
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Exit Runtime!\n");
	switch_mutex_lock(globals.mutex);
	globals.threads--;
	switch_mutex_unlock(globals.mutex);
	switch_core_destroy_memory_pool(&globals.pool);

	return NULL;
}

SWITCH_MODULE_LOAD_FUNCTION(example_load)
{
		switch_api_interface_t *api_interface;
		switch_application_interface_t *app_interface;

		*module_interface = switch_loadable_module_create_module_interface(pool, modname);

		memset(&globals, 0, sizeof(globals));
		globals.pool = pool;
	
		do_config(SWITCH_FALSE);
		
		switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);
		
		switch_mutex_lock(globals.mutex);
		globals.looping = 1;
		globals.running = 1;
		globals.print = 1;
		switch_mutex_unlock(globals.mutex);
			
		{
			switch_thread_t *thread;
			switch_threadattr_t *thd_attr = NULL;
			switch_memory_pool_t *pool;
			
			switch_core_new_memory_pool(&pool);
			switch_threadattr_create(&thd_attr, pool);
			switch_threadattr_detach_set(thd_attr, 1);
			switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
			switch_thread_create(&thread, thd_attr, outbound_agent_thread_run, NULL, pool);
		}
	
		SWITCH_ADD_API(api_interface, "example", "this command fires an event of type TRAP and Toggle printing on/off from module's runtime function", example_api, "");
		SWITCH_ADD_APP(app_interface, "example", "prints <num> log lines prints <num> log lines,", "5 as default, min 0, max 1024", example_app, "<num>", SAF_NONE);
		
		switch_event_bind("mod_example", SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, example_event_handler, NULL);
		
		switch_core_add_state_handler(&example_state_handler);
		
		return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(example_shutdown)
{
	int sanity = 0;
	switch_mutex_lock(globals.mutex);
	globals.running = 0;
	globals.looping = 0;
	switch_mutex_unlock(globals.mutex);

	switch_core_remove_state_handler(&example_state_handler);
	switch_event_unbind_callback(example_event_handler);
	
	while (globals.threads) {
		switch_cond_next();
		if (++sanity >= 6000) {
			break;
		}
	}
	
	return SWITCH_STATUS_SUCCESS;
}

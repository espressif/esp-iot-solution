# Component: weekly_timer

* This component defines an weekly_timer as a well encapsulated object.

* To use the weekly_timer, you need to:
    * call iot_weekly_timer_init() to initialize the timer module
    * call iot_weekly_timer_add() to create a timer object

* An weekly_timer can provide:
    * iot_weekly_timer_start(weekly_timer_handle_t) api to start a timer, the argument is handle returned by iot_weekly_timer_add()
    * iot_weekly_timer_stop(weekly_timer_handle_t) api to stop a timer
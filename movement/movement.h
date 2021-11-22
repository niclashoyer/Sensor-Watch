#ifndef MOVEMENT_H_
#define MOVEMENT_H_
#include <stdio.h>
#include <stdbool.h>

// TODO: none of this is implemented
typedef union {
    struct {
        uint32_t reserved : 14;
        uint32_t button_should_sound : 1;   // if true, pressing a button emits a sound.
        uint32_t to_interval : 2;           // an inactivity interval for asking the active face to resign.
        uint32_t le_interval : 3;           // 0 to disable low energy mode, or an inactivity interval for going into low energy mode.
        uint32_t led_duration : 2;          // how many seconds to shine the LED for (x2), or 0 to disable it.
        uint32_t led_red_color : 4;         // for general purpose illumination, the red LED value (0-15)
        uint32_t led_green_color : 4;       // for general purpose illumination, the green LED value (0-15)

        // while Movement itself doesn't implement a clock or display units, it may make sense to include some
        // global settings for watch faces to check. The 12/24 hour preference could inform a clock or a
        // time-oriented complication like a sunrise/sunset timer, and a simple locale preference could tell an
        // altimeter to display feet or meters as easily as it tells a thermometer to display degrees in F or C.
        uint32_t clock_mode_24h : 1;        // indicates whether clock should use 12 or 24 hour mode.
        uint32_t use_imperial_units : 1;    // indicates whether to use metric units (the default) or imperial.
    } bit;
    uint32_t value;
} movement_settings_t;

typedef enum {
    EVENT_NONE = 0,             // There is no event to report.
    EVENT_ACTIVATE,             // Your watch face is entering the foreground.
    EVENT_TICK,                 // Most common event type. Your watch face is being called from the tick callback.
    EVENT_LOW_ENERGY_UPDATE,    // If the watch is in low energy mode and you are in the foreground, you will get a chance to update the display once per minute.
    EVENT_BACKGROUND_TASK,      // Your watch face is being invoked to perform a background task. Don't update the display here; you may not be in the foreground.
    EVENT_TIMEOUT,              // Your watch face has been inactive for a while. You may want to resign, depending on your watch face's intended use case.
    EVENT_LIGHT_BUTTON_DOWN,    // The light button has been pressed, but not yet released.
    EVENT_LIGHT_BUTTON_UP,      // The light button was pressed and released.
    EVENT_LIGHT_LONG_PRESS,     // The light button was held for >2 seconds, and released.
    EVENT_MODE_BUTTON_DOWN,     // The mode button has been pressed, but not yet released.
    EVENT_MODE_BUTTON_UP,       // The mode button was pressed and released.
    EVENT_MODE_LONG_PRESS,      // The mode button was held for >2 seconds, and released.
    EVENT_ALARM_BUTTON_DOWN,    // The alarm button has been pressed, but not yet released.
    EVENT_ALARM_BUTTON_UP,      // The alarm button was pressed and released.
    EVENT_ALARM_LONG_PRESS,     // The alarm button was held for >2 seconds, and released.
} movement_event_type_t;

typedef struct {
    uint8_t event_type;
    uint8_t subsecond;
} movement_event_t;

/** @brief Perform setup for your watch face.
  * @details It's tempting to say this is 'one-time' setup, but technically this function is called more than
  *          once. When the watch first boots, this function is called with a NULL context_ptr, indicating
  *          that it is the first run. At this time you should set context_ptr to something non-NULL if you
  *          need to keep track of any state in your watch face. If your watch face requires any other setup,
  *          like configuring a pin mode or a peripheral, you may want to do that here too.
  *          This function will be called again after waking from sleep mode, since sleep mode disables all
  *          of the device's pins and peripherals.
  * @param settings A pointer to the global Movement settings. You can use this to inform how you present your
  *                 display to the user (i.e. taking into account whether they have silenced the buttons, or if
  *                 they prefer 12 or 24-hour mode). You can also change these settings if you like.
  * @param context_ptr A pointer to a pointer; at first invocation, this value will be NULL, and you can set it
  *                    to any value you like. Subsequent invocations will pass in whatever value you previously
  *                    set. You may want to check if this is NULL and if so, allocate some space to store any
  *                    data required for your watch face.
  *
  */
typedef void (*watch_face_setup)(movement_settings_t *settings, void ** context_ptr);

/** @brief Prepare to go on-screen.
  * @details This function is called just before your watch enters the foreground. If your watch face has any
  *          segments or text that is always displayed, you may want to set that here. In addition, if your
  *          watch face depends on data from a peripheral (like an I2C sensor), you will likely want to enable
  *          that peripheral here. In addition, if your watch face requires an update frequncy other than 1 Hz,
  *          you may want to request that here using the movement_request_tick_frequency function.
  * @param settings A pointer to the global Movement settings. @see watch_face_setup.
  * @param context A pointer to your watch face's context. @see watch_face_setup.
  *
  */
typedef void (*watch_face_activate)(movement_settings_t *settings, void *context);

/** @brief Handle events and update the display.
  * @details This function is called in response to an event. You should set up a switch statement that handles,
  *          at the very least, the EVENT_TICK and EVENT_MODE_BUTTON_UP event types. The tick event happens once
  *          per second (or more frequently if you asked for a faster tick with movement_request_tick_frequency).
  *          The mode button up event occurs when the user presses the MODE button. **Your loop function SHOULD
  *          call the movement_move_to_next_face function in response to this event.** If you have a good reason
  *          to override this behavior (e.g. your user interface requires all three buttons), your watch face MUST
  *          call the movement_move_to_next_face function in response to the EVENT_MODE_LONG_PRESS event. If you
  *          fail to do this, the user will become stuck on your watch face.
  * @param event A struct containing information about the event, including its type. @see movement_event_type_t
  *              for a list of all possible event types.
  * @param settings A pointer to the global Movement settings. @see watch_face_setup.
  * @param context A pointer to your application's context. @see watch_face_setup.
  * @return true if Movement can enter STANDBY mode; false to keep it awake. You should almost always return true.
  * @note There are two event types that require some extra thought:
          The EVENT_LOW_ENERGY_UPDATE event type is a special case. If you are in the foreground when the watch
          goes into low energy mode, you will receive this tick once a minute (at the top of the minute) so that
          you can update the screen. Great! But! When you receive this event, all pins and peripherals other than
          the RTC will have been disabled to save energy. If your display is clock or calendar oriented, this is
          fine. But if your display requires polling an I2C sensor or reading a value with the ADC, you won't be
          able to do this. You should either display the name of the watch face in response to the low power tick,
          or ensure that you resign before low power mode triggers, (e.g. by calling movement_move_to_face(0)).
          **Your watch face MUST NOT wake up peripherals in response to a low power tick.** The purpose of this
          mode is to consume as little energy as possible during the (potentially long) intervals when it's
          unlikely the user is wearing or looking at the watch.
          EVENT_BACKGROUND_TASK is also a special case. @see watch_face_wants_background_task for details.
  */
typedef bool (*watch_face_loop)(movement_event_t event, movement_settings_t *settings, void *context);

/** @brief Prepare to go off-screen.
  * @details This function is called before your watch face enters the background. If you requested a tick
  *          frequency other than the standard 1 Hz, **you must call movement_request_tick_frequency(1) here**
  *          to reset to 1 Hz. You should also disable any peripherals you enabled when you entered the foreground.
  * @param settings A pointer to the global Movement settings. @see watch_face_setup.
  * @param context A pointer to your application's context. @see watch_face_setup.
  */
typedef void (*watch_face_resign)(movement_settings_t *settings, void *context);

/** @brief OPTIONAL. Request an opportunity to run a background task.
  * @warning NOT YET IMPLEMENTED.
  * @details Most apps will not need this function, but if you provide it, Movement will call it once per minute in
  *          both active and low power modes, regardless of whether your app is in the foreground. You can check the
  *          current time to determine whether you require a background task. If you return true here, Movement will
  *          immediately call your loop function with an EVENT_BACKGROUND_TASK event. Note that it will not call your
  *          activate or deactivate functions, since you are not going on screen.
  *
  *          Examples of background tasks:
  *           - Wake and play a sound when an alarm or timer has been triggered.
  *           - Check the state of an RTC interrupt pin or the timestamp of an RTC interrupt event.
  *           - Log a data point from a sensor, and then return to sleep mode.
  *
  *          Guidelines for background tasks:
  *           - Assume all peripherals and pins other than the RTC will be disabled when you get an EVENT_BACKGROUND_TASK.
  *           - Even if your background task involves only the RTC peripheral, try to request background tasks sparingly.
  *           - If your background task involves an external pin or peripheral, request background tasks no more than once per hour.
  *           - If you need to enable a pin or a peripheral to perform your task, return it to its original state afterwards.
  *
  * @param settings A pointer to the global Movement settings. @see watch_face_setup.
  * @param context A pointer to your application's context. @see watch_face_setup.
  * @return true to request a background task; false otherwise.
  */
typedef bool (*watch_face_wants_background_task)(movement_settings_t *settings, void *context);

typedef struct {
    watch_face_setup setup;
    watch_face_activate activate;
    watch_face_loop loop;
    watch_face_resign resign;
    watch_face_wants_background_task wants_background_task;
} watch_face_t;

typedef struct {
    // properties stored in BACKUP register
    movement_settings_t settings;

    // transient properties
    int16_t current_watch_face;
    int16_t next_watch_face;
    bool watch_face_changed;

    // LED stuff
    uint8_t light_ticks;
    bool led_on;
    
    // button tracking for long press
    uint8_t light_down_timestamp;
    uint8_t mode_down_timestamp;
    uint8_t alarm_down_timestamp;

    // background task handling
    bool needs_background_tasks_handled;

    // low energy mode countdown
    int32_t le_mode_ticks;

    // app resignation countdown (TODO: consolidate with LE countdown?)
    int16_t timeout_ticks;

    // stuff for subsecond tracking
    uint8_t tick_frequency;
    uint8_t last_second;
    uint8_t subsecond;
} movement_state_t;

void movement_move_to_face(uint8_t watch_face_index);
void movement_move_to_next_face();
void movement_illuminate_led();
void movement_request_tick_frequency(uint8_t freq);

#endif // MOVEMENT_H_
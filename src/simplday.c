/****************************************************************************/
/**
* Pebble watch face that displays day, month, date, and time.  Modified for
* display of sunrise/set times as well.
*
* @file   simplday.c
*
* @author Bob Hauck <bobh@haucks.org>
* @author Modified by Andrew Barry <abarry@gmail.com> to display
*   time until sunrise / sunset
*
* This code may be used or modified in any way whatsoever without permission
* from the author.
*
*****************************************************************************/
#include "pebble.h"


#define MOVE_AMOUNT (-15)

#define SUNRISE_STORAGE_KEY 1
#define SUNSET_STORAGE_KEY 2

#define DEFAULT_SUN_TIME (-1)

Window *window;
TextLayer *text_day_layer;
TextLayer *text_date_layer;
TextLayer *text_time_layer;
TextLayer *text_sun_layer;
Layer *line_layer;
GFont *font_date;
GFont *font_time;

int sunset_time = -1;
int sunrise_time = -1;

static AppSync sync;
static uint8_t sync_buffer[64];

enum WeatherKey {
  SUNRISE_TIME_KEY = 0x0,         // TUPLE_INT
  SUNSET_TIME_KEY = 0x1,  // TUPLE_INT
};


static void send_cmd(void) {
  Tuplet value = TupletInteger(1, 1);

  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  if (iter == NULL) {
    return;
  }

  dict_write_tuplet(iter, &value);
  dict_write_end(iter);

  app_message_outbox_send();
}


/* Draw the dividing line.
*/
void line_layer_update_callback(Layer *l, GContext *ctx)
{
    (void)l;

    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_line(ctx, GPoint(8, 97 + MOVE_AMOUNT), GPoint(131, 97 + MOVE_AMOUNT));
    graphics_draw_line(ctx, GPoint(8, 98 + MOVE_AMOUNT), GPoint(131, 98 + MOVE_AMOUNT));
}

void update_sun_time(struct tm *tick_time) {
    // compute the delta times
    
    static char sun_text[] = " --:--";
    
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Rise %d, Set: %d", sunrise_time, sunset_time);
    
    if (sunrise_time > 0 && sunset_time > 0) {
        
        // in minutes from now
        int delta_sunrise = tick_time->tm_min + tick_time->tm_hour*60 - sunrise_time;
        
        int delta_sunset = tick_time->tm_min + tick_time->tm_hour*60 - sunset_time;
        
        int delta_display;
        
        // figure out which one to display: if we're within two hours, display
        // that one, otherwise, display the next event
        if (delta_sunrise < 120) {
            delta_display = delta_sunrise;
        } else if (delta_sunset < 120) {
            delta_display = delta_sunset;
        } else if (delta_sunrise < 0) {
            // it's before sunrise, display sunrise
            delta_display = delta_sunrise;
        } else if (delta_sunset < 0) {
            // it's before sunset, display sunset
            delta_display = delta_sunset;
        } else {
            // we're after sunrise and sunset, so display time until
            // next sunrise
            
            delta_display = delta_sunrise - 1440; // 1440 = number of min in a day
        }
        
        // now format and display the time
        char plus_minus;
        if (delta_display > 0) {
            plus_minus = '+';
        } else {
            plus_minus = ' ';
            delta_display = -delta_display;
        }
        
        int hours = delta_display / 60;
        int mins = delta_display % 60;
        
        
        
        // display on screen

        
        
        snprintf(sun_text, sizeof(sun_text), "%c%d:%02d", plus_minus, hours, mins);
    }
    
    // put sun text on the screen
    text_layer_set_text(text_sun_layer, sun_text);
}

/* Called by the OS once per minute to update date and time.
*/
void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed)
{
    /* Need to be static because they're used by the system later. */
    static char date_text[] = "Xxxxxxxxx 00";
    static char day_text[] = "Xxxxxxxxx";
    static char time_text[] = "00:00";
    
    
    char *time_format;

    if (units_changed & DAY_UNIT) // update day text if day has changed
    {
        strftime(day_text, sizeof(day_text), "%A", tick_time);
        text_layer_set_text(text_day_layer, day_text);

        strftime(date_text, sizeof(date_text), "%B %e", tick_time);
        text_layer_set_text(text_date_layer, date_text);
        
        // ask the phone for new sunrise sunset times
        send_cmd();
    }

    time_format = clock_is_24h_style() ? "%R" : "%I:%M";
    strftime(time_text, sizeof(time_text), time_format, tick_time);
    
    // Kludge to handle lack of non-padded hour format string
    // for twelve hour clock.
    if (!clock_is_24h_style() && (time_text[0] == '0'))
    {
        memmove(time_text, &time_text[1], sizeof(time_text) - 1);
        
    }
    
    update_sun_time(tick_time);

    text_layer_set_text(text_time_layer, time_text);
    
}


/* Callback functions for watch <--> phone comms
 */
static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d", app_message_error);
}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
    
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Key: %u", (unsigned int) key);
    
  time_t t = time(NULL);
  struct tm *tick_time = localtime(&t);
    
  switch (key) {

    case SUNRISE_TIME_KEY:
      // App Sync keeps new_tuple in sync_buffer, so we may use it directly
      //text_layer_set_text(text_sun_layer, "sunrise");
      
      sunrise_time = (int)new_tuple->value->int32;
      APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync value: %d", (int) new_tuple->value->int32);
      
      // update persistent storage for sunrise
      persist_write_int(SUNRISE_STORAGE_KEY, sunrise_time);
      
      
      update_sun_time(tick_time);
      
      break;

    case SUNSET_TIME_KEY:
      APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync value: %d", (int) new_tuple->value->int32);
      
      // store the time
      sunset_time = (int)new_tuple->value->int32;
      
      // update persistent storage for sunset
      persist_write_int(SUNSET_STORAGE_KEY, sunset_time);
      
      update_sun_time(tick_time);
      
      break;
  }
}



/* Initialize the application.
*/
void app_init(void)
{
    
    const int inbound_size = 64;
    const int outbound_size = 64;
    app_message_open(inbound_size, outbound_size);
    
    // restore values from persistent storage in case the phone isn't around
    sunrise_time = persist_exists(SUNRISE_STORAGE_KEY) ? persist_read_int(SUNRISE_STORAGE_KEY) : DEFAULT_SUN_TIME;
    
    sunset_time = persist_exists(SUNSET_STORAGE_KEY) ? persist_read_int(SUNSET_STORAGE_KEY) : DEFAULT_SUN_TIME;
    
    Tuplet initial_values[] = {
        TupletInteger(SUNRISE_TIME_KEY, sunrise_time),
        TupletInteger(SUNSET_TIME_KEY, sunset_time),
    };
    
    app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values, ARRAY_LENGTH(initial_values),
      sync_tuple_changed_callback, sync_error_callback, NULL);
    
    
    
    
    // get sunrise/set times from the phone
    send_cmd();
    
    time_t t = time(NULL);
    struct tm *tick_time = localtime(&t);
    TimeUnits units_changed = SECOND_UNIT | MINUTE_UNIT | HOUR_UNIT | DAY_UNIT;
    ResHandle res_d;
    ResHandle res_t;
    Layer *window_layer;

    window = window_create();
    window_layer = window_get_root_layer(window);
    window_set_background_color(window, GColorBlack);

    res_d = resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_21);
    res_t = resource_get_handle(RESOURCE_ID_FONT_ROBOTO_BOLD_SUBSET_49);
    font_date = fonts_load_custom_font(res_d);
    font_time = fonts_load_custom_font(res_t);

    text_day_layer = text_layer_create(GRect(8, 44 + MOVE_AMOUNT, 144-8, 168-44- MOVE_AMOUNT));
    text_layer_set_text_color(text_day_layer, GColorWhite);
    text_layer_set_background_color(text_day_layer, GColorClear);
    text_layer_set_font(text_day_layer, font_date);
    layer_add_child(window_layer, text_layer_get_layer(text_day_layer));

    text_date_layer = text_layer_create(GRect(8, 68 + MOVE_AMOUNT, 144-8, 168-68- MOVE_AMOUNT));
    text_layer_set_text_color(text_date_layer, GColorWhite);
    text_layer_set_background_color(text_date_layer, GColorClear);
    text_layer_set_font(text_date_layer, font_date);
    layer_add_child(window_layer, text_layer_get_layer(text_date_layer));

    text_time_layer = text_layer_create(GRect(7, 92 + MOVE_AMOUNT, 144-7, 168-92- MOVE_AMOUNT));
    text_layer_set_text_color(text_time_layer, GColorWhite);
    text_layer_set_background_color(text_time_layer, GColorClear);
    text_layer_set_font(text_time_layer, font_time);
    layer_add_child(window_layer, text_layer_get_layer(text_time_layer));
    
    text_sun_layer = text_layer_create(GRect(80, 138, 144-45, 168-80));
    text_layer_set_text_color(text_sun_layer, GColorWhite);
    text_layer_set_background_color(text_sun_layer, GColorClear);
    text_layer_set_font(text_sun_layer, font_date);
    layer_add_child(window_layer, text_layer_get_layer(text_sun_layer));
    
    line_layer = layer_create(GRect(0, 0, 144, 168));
    layer_set_update_proc(line_layer, line_layer_update_callback);
    layer_add_child(window_layer, line_layer);

    handle_minute_tick(tick_time, units_changed);
    update_sun_time(tick_time);
    
    window_stack_push(window, true /* Animated */);
    tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
    
}



/* Shut down the application
*/
void app_term(void)
{
    tick_timer_service_unsubscribe();
    text_layer_destroy(text_time_layer);
    text_layer_destroy(text_date_layer);
    text_layer_destroy(text_day_layer);
    text_layer_destroy(text_sun_layer);
    layer_destroy(line_layer);

    window_destroy(window);

    fonts_unload_custom_font(font_date);
    fonts_unload_custom_font(font_time);
    
    app_sync_deinit(&sync);
}


/********************* Main Program *******************/

int main(void)
{
    app_init();
    app_event_loop();
    app_term();

    return 0;
}

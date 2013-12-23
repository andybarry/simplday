/****************************************************************************/
/**
* Pebble watch face that displays day, month, date, and time.
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


#define MOVE_AMOUNT (-10)

Window *window;
TextLayer *text_day_layer;
TextLayer *text_date_layer;
TextLayer *text_time_layer;
TextLayer *text_sun_layer;
Layer *line_layer;
GFont *font_date;
GFont *font_time;


/* Draw the dividing line.
*/
void line_layer_update_callback(Layer *l, GContext *ctx)
{
    (void)l;

    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_line(ctx, GPoint(8, 97 + MOVE_AMOUNT), GPoint(131, 97 + MOVE_AMOUNT));
    graphics_draw_line(ctx, GPoint(8, 98 + MOVE_AMOUNT), GPoint(131, 98 + MOVE_AMOUNT));
}


/* Called by the OS once per minute to update date and time.
*/
void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed)
{
    /* Need to be static because they're used by the system later. */
    static char date_text[] = "Xxxxxxxxx 00";
    static char day_text[] = "Xxxxxxxxx";
    static char time_text[] = "00:00";
    static char sun_text[] = "00:00";
    
    char *time_format;
    char *sun_format;

    if (units_changed & DAY_UNIT) // update day text if day has changed
    {
        strftime(day_text, sizeof(day_text), "%A", tick_time);
        text_layer_set_text(text_day_layer, day_text);

        strftime(date_text, sizeof(date_text), "%B %e", tick_time);
        text_layer_set_text(text_date_layer, date_text);
    }

    time_format = clock_is_24h_style() ? "%R" : "%I:%M";
    strftime(time_text, sizeof(time_text), time_format, tick_time);
    
    // update text for time until sunset
    strftime(sun_text, sizeof(time_text), time_format, tick_time);

    // Kludge to handle lack of non-padded hour format string
    // for twelve hour clock.
    if (!clock_is_24h_style() && (time_text[0] == '0'))
    {
        memmove(time_text, &time_text[1], sizeof(time_text) - 1);
    }

    text_layer_set_text(text_time_layer, time_text);
    
    
    // put sun text on the screen
    text_layer_set_text(text_sun_layer, "+00:00");
}


/* Initialize the application.
*/
void app_init(void)
{
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
    
    text_sun_layer = text_layer_create(GRect(75, 138, 144-45, 168-75));
    text_layer_set_text_color(text_sun_layer, GColorWhite);
    text_layer_set_background_color(text_sun_layer, GColorClear);
    text_layer_set_font(text_sun_layer, font_date);
    layer_add_child(window_layer, text_layer_get_layer(text_sun_layer));
    
    line_layer = layer_create(GRect(0, 0, 144, 168));
    layer_set_update_proc(line_layer, line_layer_update_callback);
    layer_add_child(window_layer, line_layer);

    handle_minute_tick(tick_time, units_changed);
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
}


/********************* Main Program *******************/

int main(void)
{
    app_init();
    app_event_loop();
    app_term();

    return 0;
}

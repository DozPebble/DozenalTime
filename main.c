#include <pebble.h>
#include <math.h>
#define KEY_CLOCK        0
#define KEY_SCALE        1
#define KEY_DATE         2
#define KEY_TEMPERATURE  3
#define KEY_CONDITIONS   4
#define KEY_SCALE_CHOICE 5
#define KEY_CLOCK_FORMAT 6
#define KEY_DATE_FORMAT  7

/**
 *    Author:    Kyle Lanmon
 *
 *    TODO:
 *        - font
 *        - semidiurnal time is not right
 */

Window *window;
TextLayer *time_layer, *date_layer, *condition_layer, *temperature_layer;
GBitmap *battery;
BitmapLayer *battery_layer;
Layer *line;
static int scale = 0, clock_format = 0, date_format = 0;

static const int BATTERY[] = {
    RESOURCE_ID_BATT_00,
    RESOURCE_ID_BATT_01,
    RESOURCE_ID_BATT_02,
    RESOURCE_ID_BATT_03,
    RESOURCE_ID_BATT_04,
    RESOURCE_ID_BATT_05,
    RESOURCE_ID_BATT_06,
    RESOURCE_ID_BATT_07,
    RESOURCE_ID_BATT_08,
    RESOURCE_ID_BATT_09,
    RESOURCE_ID_BATT_10
};

static char DOZ_DIGITS[] = {
    '0','1','2','3','4','5','6','7','8','9','X','E'
};

static char* DOZ_PAIRS[] = {
    "00","01","02","03","04","05","06","07","08","09","0X","0E", 
    "10","11","12","13","14","15","16","17","18","19","1X","1E",
    "20","21","22","23","24","25","26","27","28","29","2X","2E",
    "30","31","32","33","34","35","36","37","38","39","3X","3E",
    "40","41","42","43","44","45","46","47","48","49","4X","4E",
    "50","51","52","53","55","55","56","57","58","59","5X","5E",
    "60","61","62","63","64","65","66","67","68","69","6X","6E",
    "70","71","72","73","74","75","76","77","78","79","7X","7E",
    "80","81","82","83","84","85","86","87","88","89","8X","8E",
    "90","91","92","93","94","95","96","97","98","99","9X","9E",
    "X0","X1","X2","X3","X4","X5","X6","X7","X8","X9","XX","XE",
    "E0","E1","E2","E3","E5","E5","E6","E7","E8","E9","EX","EE",
    "100"
};

static char* DOZ_GROUPS[] = {
    "0","1","2","3","4","5","6","7","8","9","X","E"
};

void write_settings_to_memory() {
    persist_write_int(KEY_SCALE_CHOICE, scale);
    persist_write_int(KEY_CLOCK_FORMAT, clock_format);
    persist_write_int(KEY_DATE_FORMAT, date_format);
}

void read_settings_from_memory() {
    if (persist_exists(KEY_SCALE_CHOICE))
        scale = persist_read_int(KEY_SCALE_CHOICE);
    if (persist_exists(KEY_CLOCK_FORMAT))
        clock_format = persist_read_int(KEY_CLOCK_FORMAT);
    if (persist_exists(KEY_DATE_FORMAT))
        date_format = persist_read_int(KEY_DATE_FORMAT);
}

char* lastTwo(char* year) {
    static char result[3];
    
    result[0] = year[2];
    result[1] = year[3];
    result[2] = year[4];
    
    return result;
}

char* dec_to_doz(int dec_val) {
    static char result[6];
    int temp[6];
    int i = 0, j = 0;
    bool neg = false;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "input to dec_to_doz: %d", dec_val);
    if (abs(dec_val) >= 11) {
        while (dec_val != 0) {
            temp[i] = dec_val % 12;
            dec_val /= 12;
            i++;
        }
        
        i--;
        
        while (i >= 0) {
            if (temp[i] < 0 && !neg)
                neg = true;
            result[j] = DOZ_DIGITS[abs(temp[i])];
            j++;
            i--;
        }
        
        if (neg) {
            for (int k = 0; k < j; k++) {
                char foo = result[k];
                result[k+1] = foo;
            }
            result[0] = '-';
            j++;
        }
        result[j] = '\0';
    } else {
        if (dec_val < 0) {
            result[0] = '-';
            result[1] = DOZ_DIGITS[-1*dec_val];
            result[2] = '\0';
        } else {
            result[0] = DOZ_DIGITS[dec_val];
            result[1] = '\0';
        }
    }
    APP_LOG(APP_LOG_LEVEL_DEBUG, "result: %s", result);
    return result;
}

void draw_line(Layer *layer, GContext* ctx) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

void handle_weather() {
    read_settings_from_memory();
    // Begin dictionary
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);

    // Add a key-value pair
    dict_write_uint8(iter, 0, 0);

    // Send the message!
    app_message_outbox_send();
}

void update_scale(double temp) {
    read_settings_from_memory();
    static char temperature_buffer[10];
    temp -= 273.15;
    
    switch (scale) {
        case 0:    // Fahrenheit
            temp = (temp * 1.8) + 32;
            snprintf(temperature_buffer, sizeof(temperature_buffer), "%s °F", dec_to_doz(round(temp)));
            break;
        case 1:    // Celsius
            snprintf(temperature_buffer, sizeof(temperature_buffer), "%s °C", dec_to_doz((int)round(temp)));
            break;
        case 2:    // Q Celsius
            temp = temp * 2.541;
            snprintf(temperature_buffer, sizeof(temperature_buffer), "%s Q°C", dec_to_doz((int)round(temp)));
            break;
        case 3:    // Q Fahrenheit
            temp = (temp * 2.541) + 48;
            snprintf(temperature_buffer, sizeof(temperature_buffer), "%s Q°F", dec_to_doz(round(temp)));
            break;
    }
    text_layer_set_text(temperature_layer, temperature_buffer);
}

static void handle_battery() {
    BatteryChargeState charge_state = battery_state_service_peek();
    
    if (battery)
        gbitmap_destroy(battery);
    
    if (charge_state.charge_percent == 100)
        battery = gbitmap_create_with_resource(BATTERY[0]);
    else if (charge_state.charge_percent == 90)
        battery = gbitmap_create_with_resource(BATTERY[1]);
    else if (charge_state.charge_percent == 80)
        battery = gbitmap_create_with_resource(BATTERY[2]);
    else if (charge_state.charge_percent == 70)
        battery = gbitmap_create_with_resource(BATTERY[3]);
    else if (charge_state.charge_percent == 60)
        battery = gbitmap_create_with_resource(BATTERY[4]);
    else if (charge_state.charge_percent == 50)
        battery = gbitmap_create_with_resource(BATTERY[5]);
    else if (charge_state.charge_percent == 40)
        battery = gbitmap_create_with_resource(BATTERY[6]);
    else if (charge_state.charge_percent == 30)
        battery = gbitmap_create_with_resource(BATTERY[7]);
    else if (charge_state.charge_percent == 20)
        battery = gbitmap_create_with_resource(BATTERY[8]);
    else if (charge_state.charge_percent == 10)
        battery = gbitmap_create_with_resource(BATTERY[9]);
    else
        battery = gbitmap_create_with_resource(BATTERY[10]);
    
    bitmap_layer_set_bitmap(battery_layer, battery);
}

static void handle_date() {
    read_settings_from_memory();
    time_t cur_time = time(NULL);
    struct tm *tick_time = localtime(&cur_time);
    char *year = lastTwo(dec_to_doz(tick_time->tm_year+1900));
    char wkDay[5];
    strftime(wkDay, sizeof(wkDay), "%a", tick_time);
    
    static char date_buff[14];
    switch (date_format) {
        case 0:
            snprintf(date_buff, sizeof(date_buff), "%s.%s.%s %s",
                     DOZ_PAIRS[tick_time->tm_mday],
                     DOZ_PAIRS[tick_time->tm_mon+1],
                     year, wkDay);
            break;
        case 1:
            snprintf(date_buff, sizeof(date_buff), "%s/%s/%s %s",
                     DOZ_PAIRS[tick_time->tm_mon+1],
                     DOZ_PAIRS[tick_time->tm_mday],
                     year, wkDay);
            break;
        case 2:
            snprintf(date_buff, sizeof(date_buff), "%s-%s-%s %s",
                     year,
                     DOZ_PAIRS[tick_time->tm_mon+1],
                     DOZ_PAIRS[tick_time->tm_mday],
                     wkDay);
            break;
    }
    
    text_layer_set_text(date_layer, date_buff);
}

static void update_time() {
    read_settings_from_memory();
    time_t cur_time = time(NULL);
    struct tm *tick_time = localtime(&cur_time);
    
    static char time_buff[10];
    int minutes = ((tick_time->tm_min)*60+tick_time->tm_sec)/50;
    
    if (tick_time->tm_hour%2 != 0)
        minutes = ((tick_time->tm_min)*60+(tick_time->tm_sec))/50+72;
    
    switch (clock_format) {
        case 0:    // diurnal
            snprintf(time_buff, sizeof(time_buff), "%s%s", DOZ_GROUPS[tick_time->tm_hour/2], DOZ_PAIRS[minutes]);
            break;
        case 1:    // semidiurnal
            snprintf(time_buff, sizeof(time_buff), "%s.%s", DOZ_GROUPS[tick_time->tm_hour], DOZ_PAIRS[minutes]);
            break;
    }
        
    text_layer_set_text(time_layer, time_buff);
}

static void window_load(Window *window) {
    read_settings_from_memory();
    GRect bounds = layer_get_bounds(window_get_root_layer(window));
    
    time_layer = text_layer_create(GRect(0, PBL_IF_ROUND_ELSE(7, 0), bounds.size.w, 55));
    text_layer_set_background_color(time_layer, GColorClear);
    text_layer_set_text_color(time_layer, COLOR_FALLBACK(GColorFromHEX(0x1200ff), GColorBlack));
    text_layer_set_text(time_layer, "nnn");
    text_layer_set_font(time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
    text_layer_set_text_alignment(time_layer, GTextAlignmentCenter);
    
    date_layer = text_layer_create(GRect(0, 55, bounds.size.w, 20));
    text_layer_set_background_color(date_layer, GColorClear);
    text_layer_set_text_color(date_layer, COLOR_FALLBACK(GColorFromHEX(0x9b0000), GColorBlack));
    text_layer_set_text(date_layer, "mm-dd-yyyy");
    text_layer_set_font(date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_text_alignment(date_layer, GTextAlignmentCenter);
    
    line = layer_create(GRect(PBL_IF_ROUND_ELSE(12, 8), 90, PBL_IF_ROUND_ELSE(155, 128), 2));
	layer_set_update_proc(line, draw_line);
    
    condition_layer = text_layer_create(GRect(PBL_IF_ROUND_ELSE(5, 0), 105, bounds.size.w/2, 168));
    text_layer_set_background_color(condition_layer, GColorClear);
    text_layer_set_text_color(condition_layer, COLOR_FALLBACK(GColorFromHEX(0x666159), GColorBlack));
    text_layer_set_text_alignment(condition_layer, GTextAlignmentCenter);
    text_layer_set_font(condition_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_text(condition_layer, "Loading");
    
    temperature_layer = text_layer_create(GRect(PBL_IF_ROUND_ELSE(5, 0), 125, bounds.size.w/2, 168));
    text_layer_set_text_alignment(temperature_layer, GTextAlignmentCenter);
    text_layer_set_background_color(temperature_layer, GColorClear);
    text_layer_set_text_color(temperature_layer, COLOR_FALLBACK(GColorFromHEX(0x666159), GColorBlack));
    text_layer_set_font(temperature_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_text(temperature_layer, "---");
    
    battery = gbitmap_create_with_resource(BATTERY[0]);
    battery_layer = bitmap_layer_create(GRect(PBL_IF_ROUND_ELSE(72, 67), 90, bounds.size.w/2, 78));
    bitmap_layer_set_compositing_mode(battery_layer, GCompOpSet);
    bitmap_layer_set_bitmap(battery_layer, battery);
    
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(time_layer));
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(date_layer));
    layer_add_child(window_get_root_layer(window), line);
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(condition_layer));
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(temperature_layer));
    layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(battery_layer));
    
    update_time();
    handle_date();
    handle_battery();
}

static void window_unload(Window *window) {
    text_layer_destroy(time_layer);
    text_layer_destroy(date_layer);
    text_layer_destroy(condition_layer);
    text_layer_destroy(temperature_layer);
}

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
    handle_battery();
    handle_date();
    
    if(((tick_time->tm_min) * 60 + tick_time->tm_sec) % 50 == 0)
		update_time();
    
    // Get weather update every 30 minutes
    if(tick_time->tm_min % 30 == 0)
        handle_weather();
}
static void process_tuple(Tuple* t) {
    // Store incoming information
    static char conditions_buffer[16];
    
    switch(t->key) {    
        case KEY_SCALE:
            if (strcmp(t->value->cstring, "0") == 0)
                scale = 0;
            else if (strcmp(t->value->cstring, "1") == 0)
                scale = 1;
            else if (strcmp(t->value->cstring, "2") == 0)
                scale = 2;
            else if (strcmp(t->value->cstring, "3") == 0)
                scale = 3;
            write_settings_to_memory();
            handle_weather();
            break;
        case KEY_CLOCK:
            if (strcmp(t->value->cstring, "0") == 0)
                clock_format = 0;
            else if (strcmp(t->value->cstring, "1") == 0)
                clock_format = 1;
            write_settings_to_memory();
            update_time();
            break;
        case KEY_DATE:
            if (strcmp(t->value->cstring, "0") == 0)
                date_format = 0;
            else if (strcmp(t->value->cstring, "1") == 0)
                date_format = 1;
            else if (strcmp(t->value->cstring, "2") == 0)
                date_format = 2;
            write_settings_to_memory();
            handle_date();
            break;
        case KEY_TEMPERATURE:
            update_scale((int)t->value->int32);
            break;
        case KEY_CONDITIONS:
            snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", t->value->cstring);
            text_layer_set_text(condition_layer, conditions_buffer);
            break;
        default:
            APP_LOG(APP_LOG_LEVEL_ERROR, "Key %d not recognized!", (int)t->key);
            break;
    }
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
    // Read first item
    Tuple *t = dict_read_first(iterator);
    
    // For all items
    while(t != NULL) {
        // Which key was received?
        process_tuple(t);

        // Look for next item
        t = dict_read_next(iterator);
    }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void init(void) {
    window = window_create();
    window_set_window_handlers(window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload
    });
    window_set_background_color(window, GColorFromHEX(0xd9c8ac));
    window_stack_push(window, true);

    battery_state_service_subscribe(handle_battery);
    tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
    
    app_message_register_inbox_received(inbox_received_callback);
    app_message_register_inbox_dropped(inbox_dropped_callback);
    app_message_register_outbox_failed(outbox_failed_callback);
    app_message_register_outbox_sent(outbox_sent_callback);
    
    #ifdef PBL_PLATFORM_APLITE
        app_message_open(2000, 2000);
    #else
        app_message_open(app_message_inbox_size_maximum(), APP_MESSAGE_OUTBOX_SIZE_MINIMUM);
    #endif
}

static void deinit(void) {
    tick_timer_service_unsubscribe();
    window_destroy(window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}

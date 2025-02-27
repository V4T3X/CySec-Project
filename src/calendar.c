#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>
#include <hiredis/hiredis.h>
#include "common.h"

// event parameter
#define MAX_EVENTS 2
#define MAX_NAME_LENGTH 50
#define MAX_DESC_LENGTH 256
#define MAX_DATE_LENGTH 12

// structure for an event
typedef struct
{
    int id;
    int visibility;
    char *date;
    char *name;
    char *description;
} Event;

// global event variables
Event *events[MAX_EVENTS];
int event_count;
int next_event_id;

// golbal view variables
int view_mode;
int view_day;
int view_month;
int view_year;

// CALENDAR VIEW --------------------
// function to calculate the number of days in the current month
int get_days_in_month(int month, int year)
{
    if (month == 2)
    { // february
        if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
            return 29; // leap year
        return 28;
    }
    // months with 31 days
    if (month == 1 || month == 3 || month == 5 || month == 7 ||
        month == 8 || month == 10 || month == 12)
        return 31;
    return 30; // months with 30 days
}

// function to get the current day, month and year
void get_current_day_month_year(int *day, int *month, int *year)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    *day = tm_info->tm_mday;
    *month = tm_info->tm_mon + 1;
    *year = tm_info->tm_year + 1900;
}

// function to check if a day/month has an event (for highlighting in view)
int has_event(int day, int month, int year)
{
    if (day == 0)
    { // check if there is any event in the given month
        for (int i = 0; i < event_count; i++)
        {
            int event_month = atoi(events[i]->date + 5); // extract the month from the date
            int event_year = atoi(events[i]->date);      // extract the year from the date
            if (event_month == month && event_year == year)
            {
                return 1; // event found in this month
            }
        }
        return 0; // no events found in this month
    }

    char date[MAX_DATE_LENGTH];
    snprintf(date, sizeof(date), "%04d-%02d-%02d", year, month, day);

    // search for an event with the same date in the events array
    for (int i = 0; i < event_count; i++)
    {
        if (strcmp(events[i]->date, date) == 0)
        {
            return 1; // event found on this date
        }
    }
    return 0; // no event found on this date
}

// function to display a month
void display_day_view()
{
    int month = view_month;
    int year = view_year;

    int days_in_month = get_days_in_month(month, year);

    printf("Calendar for " RED_COLOR "%02d/%04d" RESET_COLOR ": \n\n", month, year);
    printf(" Mo  Tu  We  Th  Fr  Sa  Su\n");

    // determine the start day of the week for the first day of the month
    struct tm first_day = {.tm_year = year - 1900, .tm_mon = month - 1, .tm_mday = 1};
    mktime(&first_day);                                             // normalize the structure
    int start_day = first_day.tm_wday == 0 ? 7 : first_day.tm_wday; // sunday -> 7

    // empty spaces before the first day of the month
    for (int i = 1; i < start_day; i++)
    {
        printf("    ");
    }

    int current_day, current_month, current_year;
    get_current_day_month_year(&current_day, &current_month, &current_year);
    // display the days of the month
    for (int day_i = 1; day_i <= days_in_month; day_i++)
    {
        if (has_event(day_i, month, year))
        {
            if (year == current_year && month == current_month && day_i == current_day)
            {
                printf(MAGENTA_COLOR " %2d" RESET_COLOR, day_i); // marks current day with event
            }
            else if ((year < current_year) ||
                     (year == current_year && month < current_month) ||
                     (year == current_year && month == current_month && day_i < current_day))
            {
                printf(GRAY_COLOR " %2d" RESET_COLOR, day_i); // marks past events
            }
            else
            {
                printf(BLUE_COLOR " %2d" RESET_COLOR, day_i); // marks future events
            }
        }
        else
        {
            if (year == current_year && month == current_month && day_i == current_day)
            {
                printf(RED_COLOR " %2d" RESET_COLOR, day_i); // marks current day
            }
            else
            {
                printf(" %2d", day_i); // marks other days
            }
        }

        if ((day_i + start_day - 1) % 7 == 0)
        { // new line after Sunday
            printf("\n");
        }
        else
        {
            printf(" ");
        }
    }
    printf("\n");
}

// display the months of the year
void display_month_view()
{
    int year = view_year;

    const char *month_names[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"};

    int current_day, current_month, current_year;
    get_current_day_month_year(&current_day, &current_month, &current_year);
    printf("Calendar for " RED_COLOR "%d" RESET_COLOR ":\n\n", year);
    for (int month_i = 1; month_i <= 12; month_i++)
    {
        if (has_event(0, month_i, year))
        {
            if (year == current_year && month_i == current_month)
            {
                printf(MAGENTA_COLOR " %-9s" RESET_COLOR, month_names[month_i - 1]); // marks current month with event
            }
            else if ((year < current_year) ||
                     (year == current_year && month_i < current_month))
            {
                printf(GRAY_COLOR " %-9s" RESET_COLOR, month_names[month_i - 1]); // marks past events
            }
            else
            {
                printf(BLUE_COLOR " %-9s" RESET_COLOR, month_names[month_i - 1]); // marks future events
            }
        }
        else
        {
            if (year == current_year && month_i == current_month)
            {
                printf(RED_COLOR " %-9s" RESET_COLOR, month_names[month_i - 1]); // marks current month
            }
            else
            {
                printf(" %-9s", month_names[month_i - 1]); // marks other months
            }
        }

        if (month_i % 3 == 0)
        {
            printf("\n");
        }
    }
}

// sets the view to the current month
void initialize_view()
{
    view_mode = 0;
    get_current_day_month_year(&view_day, &view_month, &view_year);
}

// EVENTS --------------------
// function to validate every input during the addEvent process
int input_validation_addEvent(char *date, int length)
{
    if (fgets(date, length, stdin) == NULL)
    {
        printf("%s\nFailed to read input.%s\n\n", RED_COLOR, RESET_COLOR);
        return 0;
    }

    if (strchr(date, '\n') == NULL)
    {
        empty_input_buffer();
        printf("%s\nInput too long.%s\n\n", RED_COLOR, RESET_COLOR);
        return 0;
    }

    date[strcspn(date, "\n")] = '\0';

    if (strlen(date) == 0)
    {
        printf("%s\nInput cannot be empty.%s\n\n", RED_COLOR, RESET_COLOR);
        return 0;
    }

    return 1;
}

// allocates heap memory for a event
Event *create_event(int id, int visibility, const char *date, const char *name, const char *description)
{
    Event *new_event = (Event *)malloc(sizeof(Event));
    if (new_event == NULL)
    {
        printf("%sError: Memory could not be allocated.\n%s", RED_COLOR, RESET_COLOR);
        return NULL;
    }

    new_event->id = id;
    new_event->visibility = visibility;
    new_event->date = strdup(date);
    new_event->name = strdup(name);
    new_event->description = strdup(description);

    return new_event;
}

// stores added event in the redis db
void add_event_to_redis(redisContext *c, const char *user, int id, int visibility, const char *date, const char *name, const char *description)
{
    redisReply *reply = redisCommand(c, "HSET user:%s:event:%d visibility %d date %s name %s description %s", user, id, visibility, date, name, description);
    if (reply == NULL)
    {
        printf("%sError adding the event to Redis.\n%s", RED_COLOR, RESET_COLOR);
        freeReplyObject(reply);
        return;
    }
    freeReplyObject(reply);
}

// function for adding a new event (heap + redis)
void add_event(redisContext *c, const char *user)
{
    if (event_count >= MAX_EVENTS)
    {
        printf("%sEvent limit reached. Cannot add more events.\n%s", RED_COLOR, RESET_COLOR);
        return;
    }

    char visibility;
    char date[MAX_DATE_LENGTH];
    char name[MAX_NAME_LENGTH];
    char description[MAX_DESC_LENGTH];

    // get the current date
    int current_day, current_month, current_year;
    get_current_day_month_year(&current_day, &current_month, &current_year);

    int year, month, day;

    while (1)
    {
        printf("Should the event be public? (y/n): ");
        char ch = getchar();
        empty_input_buffer();
        if (ch == 'y' || ch == 'Y')
        {
            visibility = 1;
            break;
        }
        else if (ch == 'n' || ch == 'N')
        {
            visibility = 0;
            break;
        }
        else
        {
            printf("%s\nInvalid input. Please enter 'y' or 'n'.\n\n%s", RED_COLOR, RESET_COLOR);
            continue;
        }
    }

    while (1)
    {
        printf("Enter the event date (YYYY-MM-DD): ");
        if (input_validation_addEvent(date, MAX_DATE_LENGTH))
        {
            if (sscanf(date, "%d-%d-%d", &year, &month, &day) != 3 || month < 1 || month > 12 || day < 1 || day > get_days_in_month(month, year) || year < 1 || year > 9999)
            {
                printf("%s\nInvalid date. Please enter a valid date.\n\n%s", RED_COLOR, RESET_COLOR);
                continue;
            }
            break;
        }
    }

    // validate and format the entered date
    /*
    if (sscanf(date, "%d-%d-%d", &year, &month, &day) != 3 ||
        year < current_year || year > 2100 ||
        (year == current_year && (month < current_month || (month == current_month && day < current_day))) ||
        month < 1 || month > 12 || day < 1 || day > 31)
    {
        printf("%s\nInvalid date. Please enter a valid date between today and the year 2100.\n%s", RED_COLOR, RESET_COLOR);
        return;
    }
    */

    // format the date to ensure two digits for month and day
    snprintf(date, sizeof(date), "%04d-%02d-%02d", year, month, day);

    while (1)
    {
        printf("Enter the event name (maximum %d characters): ", MAX_NAME_LENGTH);
        if (input_validation_addEvent(name, MAX_NAME_LENGTH))
        {
            break;
        }
    }

    while (1)
    {
        printf("Enter the event description (maximum %d characters): ", MAX_DESC_LENGTH);
        if (input_validation_addEvent(description, MAX_DESC_LENGTH))
        {
            break;
        }
    }

    // add the event to the array and redis
    events[event_count++] = create_event(next_event_id, visibility, date, name, description);
    add_event_to_redis(c, user, next_event_id, visibility, date, name, description);
    next_event_id++;

    printf("%s\nEvent added successfully!\n%s", GREEN_COLOR, RESET_COLOR);
}

// loads the pre-existing events from redis when the calendar is opened
void load_events_from_redis(redisContext *c, const char *user)
{
    redisReply *keys_reply = redisCommand(c, "KEYS user:%s:event:*", user);
    if (keys_reply == NULL || keys_reply->type != REDIS_REPLY_ARRAY)
    {
        printf("%sError retrieving event keys from Redis.\n%s", RED_COLOR, RESET_COLOR);
        exit(1);
    }

    event_count = 0;
    next_event_id = 1;

    // iterate over all found event keys
    for (size_t i = 0; i < keys_reply->elements; i++)
    {
        char *event_key = keys_reply->element[i]->str;

        // extract event ID from the key
        char *event_id_str = strrchr(event_key, ':');
        if (!event_id_str || strlen(event_id_str) < 2)
            continue;
        int event_id = atoi(event_id_str + 1);

        // retrieve event data
        redisReply *event_reply = redisCommand(c, "HGETALL %s", event_key);
        if (event_reply == NULL || event_reply->type != REDIS_REPLY_ARRAY || event_reply->elements % 2 != 0)
        {
            printf("%sError retrieving event data for %s.\n%s", RED_COLOR, event_key, RESET_COLOR);
            continue;
        }

        char *date = NULL, *name = NULL, *description = NULL;
        int visibility = -1;

        // extract event data from the hash structure
        for (size_t j = 0; j < event_reply->elements; j += 2)
        {
            char *field = event_reply->element[j]->str;
            char *value = event_reply->element[j + 1]->str;

            if (strcmp(field, "visibility") == 0)
                visibility = atoi(value);
            else if (strcmp(field, "date") == 0)
                date = value;
            else if (strcmp(field, "name") == 0)
                name = value;
            else if (strcmp(field, "description") == 0)
                description = value;
        }

        // if all fields are present, save the event
        if (visibility != -1 && date && name && description && event_count < MAX_EVENTS)
        {
            events[event_count] = create_event(event_id, visibility, date, name, description);
            event_count++;

            if (event_id >= next_event_id)
            {
                next_event_id = event_id + 1;
            }
        }

        freeReplyObject(event_reply);
    }

    freeReplyObject(keys_reply);
}

// strict input validation to get unsigned int id to remove event
unsigned int get_valid_unsigned_integer()
{
    char input[16];
    int temp_id;
    unsigned int id;
    char extra;

    while (1)
    {
        printf("Enter the ID of the event to remove: ");
        if (fgets(input, sizeof(input), stdin) != NULL)
        {
            if (strchr(input, '\n') == NULL)
            {
                empty_input_buffer();
                printf("%s\nEnter a valid number.%s\n\n", RED_COLOR, RESET_COLOR);
                continue;
            }
            if (sscanf(input, "%d %c", &temp_id, &extra) == 1)
            {
                if (temp_id < 0)
                {
                    printf("%s\nNegative numbers are not allowed.%s\n\n", RED_COLOR, RESET_COLOR);
                    continue;
                }

                id = (unsigned int)temp_id;
                return id;
            }
            else
            {
                printf("%s\nEnter a valid number.%s\n\n", RED_COLOR, RESET_COLOR);
            }
        }
        else
        {
            printf("%s\nInput error.\n\n%s", RED_COLOR, RESET_COLOR);
        }
    }
}

// function to free an event
void free_event(Event *event)
{
    free(event->date);
    free(event->name);
    free(event->description);
    free(event);
}

// free all events
void free_events()
{
    for (int i = 0; i < event_count; i++)
    {
        free_event(events[i]);
    }
}

// removes event from the redis db
void delete_event_from_redis(redisContext *c, const char *user, unsigned int id)
{
    redisReply *reply = redisCommand(c, "DEL user:%s:event:%d", user, id);
    if (reply == NULL)
    {
        printf("%sError deleting the event from Redis.\n%s", RED_COLOR, RESET_COLOR);
        freeReplyObject(reply);
        return;
    }
    freeReplyObject(reply);
}

// function to remove an event from the event array
void remove_event(redisContext *c, const char *user)
{
    if (event_count == 0)
    {
        printf("%sNo events to remove.\n%s", RED_COLOR, RESET_COLOR);
        return;
    }

    unsigned int id = get_valid_unsigned_integer();

    // find and remove the event
    for (int i = 0; i < event_count; i++)
    {
        if (events[i]->id == id)
        {
            free_event(events[i]);
            delete_event_from_redis(c, user, id);
            printf("%s\nEvent removed successfully!\n%s", GREEN_COLOR, RESET_COLOR);

            // shift remaining events
            for (int j = i; j < event_count - 1; j++)
            {
                events[j] = events[j + 1];
            }
            event_count--;
            return;
        }
    }

    printf("%s\nNo event found with ID %u.\n%s", RED_COLOR, id, RESET_COLOR);
}

// helper function for sorting events by date
int compare_events(const void *a, const void *b)
{
    const Event *event_a = *(const Event **)a;
    const Event *event_b = *(const Event **)b;
    return strcmp(event_a->date, event_b->date);
}

// visibility print
char *print_visibility(int vis)
{
    if (vis)
    {
        return "public";
    }
    else
    {
        return "private";
    }
}

// function to view all events
void view_events()
{
    if (event_count == 0)
    {
        printf("-----------------------------\n");
        printf("No events found.\n");
        printf("-----------------------------\n");
        return;
    }

    // get the current date
    int current_day, current_month, current_year;
    get_current_day_month_year(&current_day, &current_month, &current_year);

    // prepare a structure for current time comparison
    struct tm current_time = {.tm_year = current_year - 1900, .tm_mon = current_month - 1, .tm_mday = current_day};
    time_t now = mktime(&current_time);

    // arrays to hold past and future events
    Event *past_events[MAX_EVENTS];
    Event *future_events[MAX_EVENTS];
    int past_count = 0, future_count = 0;

    // categorize events into past and future (including today)
    for (int i = 0; i < event_count; i++)
    {
        int event_year, event_month, event_day;
        sscanf(events[i]->date, "%d-%d-%d", &event_year, &event_month, &event_day);

        struct tm event_time = {.tm_year = event_year - 1900, .tm_mon = event_month - 1, .tm_mday = event_day};
        time_t event_timestamp = mktime(&event_time);

        if (event_timestamp < now)
        {
            past_events[past_count++] = events[i];
        }
        else
        {
            future_events[future_count++] = events[i];
        }
    }

    // sort events
    qsort(past_events, past_count, sizeof(Event *), compare_events);
    qsort(future_events, future_count, sizeof(Event *), compare_events);

    // display past events
    printf("======== Past Events ========\n");
    for (int i = 0; i < past_count; i++)
    {
        printf("ID: %d\nVisibility: %s\nDate: %s%s%s\nName: %s\nDescription: %s\n",
               past_events[i]->id, print_visibility(past_events[i]->visibility), GRAY_COLOR, past_events[i]->date, RESET_COLOR, past_events[i]->name, past_events[i]->description);
        printf("-----------------------------\n");
    }

    // display future events (including today)
    printf("\n\n======= Future Events =======\n");
    for (int i = 0; i < future_count; i++)
    {
        int event_year, event_month, event_day;
        sscanf(future_events[i]->date, "%d-%d-%d", &event_year, &event_month, &event_day);

        const char *color = (event_year == current_year && event_month == current_month && event_day == current_day)
                                ? MAGENTA_COLOR
                                : BLUE_COLOR;

        printf("ID: %d\nVisibility: %s\nDate: %s%s%s\nName: %s\nDescription: %s\n",
               future_events[i]->id, print_visibility(future_events[i]->visibility), color, future_events[i]->date, RESET_COLOR, future_events[i]->name, future_events[i]->description);
        printf("-----------------------------\n");
    }
}

// MENU --------------------
// Function to navigate between views
void navigate()
{
    char choice;

    while (1)
    {
        clear();

        if (view_mode == 0)
        { // Month View
            display_day_view();
            printf("\nUse 'n' for next month, 'p' for previous month, 'y' for year view, 'q' to quit navigator.\n");
        }
        else if (view_mode == 1)
        { // Year View
            display_month_view();
            printf("\nUse 'n' for next year, 'p' for previous year, 'm' for month view, 'q' to quit navigator.\n");
        }

        scanf(" %c", &choice);
        empty_input_buffer();

        if (view_mode == 0)
        { // Month View Navigation
            switch (choice)
            {
            case 'n':
                view_month++;
                if (view_month > 12)
                {
                    view_month = 1;
                    view_year++;
                }
                break;
            case 'p':
                view_month--;
                if (view_month < 1)
                {
                    view_month = 12;
                    view_year--;
                }
                break;
            case 'y': // Switch to Year View
                view_mode = 1;
                break;
            case 'q':
                return;
            default:;
            }
        }
        else if (view_mode == 1)
        { // Year View Navigation
            switch (choice)
            {
            case 'n':
                view_year++;
                break;
            case 'p':
                view_year--;
                break;
            case 'm': // Switch to Month View
                view_mode = 0;
                break;
            case 'q':
                return;
            default:;
            }
        }
    }
}

int logged_in(const char *user)
{
    return user != NULL && user[0] != '\0';
}

// function to display the menu
void show_menu(const char *user)
{
    if (!view_mode)
    {
        display_day_view();
    }
    else
    {
        display_month_view();
    }

    printf("\n============================\n");
    if (logged_in(user))
    {
        printf("%s\nLogged in as: %s%s%s\n", BOLD, YELLOW_COLOR, user, RESET_COLOR);
    }
    else
    {
        printf("%s\nYou are not logged in.\n%s", BOLD, RESET_COLOR);
    }

    printf("\n----- %sEvent Management%s -----\n\n", BOLD, RESET_COLOR);
    if (logged_in(user))
    {
        printf("%s1.%s Add Event\n", RED_COLOR, RESET_COLOR);
        printf("%s2.%s Remove Event\n", RED_COLOR, RESET_COLOR);
    }
    else
    {
        printf("%s1. %sAdd Event\n%s", RED_COLOR, GREY, RESET_COLOR);
        printf("%s2. %sRemove Event\n%s", RED_COLOR, GREY, RESET_COLOR);
    }
    printf("%s3.%s View Events\n", RED_COLOR, RESET_COLOR);

    printf("\n------ %sCalendar View%s -------\n\n", BOLD, RESET_COLOR);
    printf("%s4.%s Reset View\n", RED_COLOR, RESET_COLOR);
    printf("%s5.%s Navigate\n\n", RED_COLOR, RESET_COLOR);

    printf("%s6.%s Exit\n", RED_COLOR, RESET_COLOR);

    printf("\n============================\n");
    printf("Choose an option: ");
}

// MAIN ----------
int main(int argc, char *argv[])
{
    clear();
    redisContext *c = connect_redis();

    if (argc < 2)
    {
        printf("No user provided.\n");
        return 1;
    }
    char *user = argv[1];
    if (logged_in(user))
    {
        load_events_from_redis(c, user);
    }

    char choice;
    initialize_view();

    do
    {
        show_menu(user);
        choice = getchar();
        empty_input_buffer();

        switch (choice)
        {
        case '1':
            clear();
            if (logged_in(user))
            {
                add_event(c, user);
            }
            else
            {
                printf("%sYou must be logged in to do this!%s\n", RED_COLOR, RESET_COLOR);
            }
            press_enter_to_continue();
            break;
        case '2':
            clear();
            if (logged_in(user))
            {
                remove_event(c, user);
            }
            else
            {
                printf("%sYou must be logged in to do this!%s\n", RED_COLOR, RESET_COLOR);
            }
            press_enter_to_continue();
            break;
        case '3':
            clear();
            view_events();
            press_enter_to_continue();
            break;
        case '4':
            initialize_view();
            break;
        case '5':
            navigate();
            break;
        default:;
        }
        clear();
    } while (choice != '6');

    free_events();
    redisFree(c);
    return 0;
}

// Compile the program: gcc -o calendar calendar.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>

#define RESET_COLOR "\033[0m"
#define BOLD "\033[1m"
#define RED_COLOR "\033[31m"
#define GREEN_COLOR "\033[32m"
#define BLUE_COLOR "\033[34m"
#define MAGENTA_COLOR "\033[35m"
#define GRAY_COLOR "\033[90m"
#define YELLOW_COLOR "\033[0;33m"
#define GREY "\033[38;5;245m"

#define MAX_EVENTS 20
#define MAX_NAME_LENGTH 50
#define MAX_DESC_LENGTH 256
#define MAX_DATE_LENGTH 12

// Structure for an event
typedef struct
{
    int id;
    char date[MAX_DATE_LENGTH];
    char name[MAX_NAME_LENGTH];
    char description[MAX_DESC_LENGTH];
} Event;

// Global variables
Event events[MAX_EVENTS];
int event_count = 0;
int next_event_id = 1;

int view_mode;
int view_day;
int view_month;
int view_year;

void empty_input_buffer()
{
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF)
        ;
}

// CALENDAR VIEW --------------------
// Function to calculate the number of days in the current month
int get_days_in_month(int month, int year)
{
    if (month == 2)
    { // February
        if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
            return 29; // Leap year
        return 28;
    }
    // Months with 31 days
    if (month == 1 || month == 3 || month == 5 || month == 7 ||
        month == 8 || month == 10 || month == 12)
        return 31;
    return 30; // Months with 30 days
}

// Function to get the current day, month and year
void get_current_day_month_year(int *day, int *month, int *year)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    *day = tm_info->tm_mday;
    *month = tm_info->tm_mon + 1;
    *year = tm_info->tm_year + 1900;
}

// Function to check if a day has an event
int has_event(int day, int month, int year)
{
    if (day == 0)
    { // Check if there is any event in the given month
        for (int i = 0; i < event_count; i++)
        {
            int event_month = atoi(events[i].date + 5); // Extract the month from the date
            int event_year = atoi(events[i].date);      // Extract the year from the date
            if (event_month == month && event_year == year)
            {
                return 1; // Event found in this month
            }
        }
        return 0; // No events found in this month
    }

    char date[MAX_DATE_LENGTH];
    snprintf(date, sizeof(date), "%04d-%02d-%02d", year, month, day);

    // Search for an event with the same date in the events array
    for (int i = 0; i < event_count; i++)
    {
        if (strcmp(events[i].date, date) == 0)
        {
            return 1; // Event found on this date
        }
    }
    return 0; // No event found on this date
}

// Function to display a month
void display_day_view()
{
    int day = view_day;
    int month = view_month;
    int year = view_year;

    int days_in_month = get_days_in_month(month, year);

    printf("Calendar for " RED_COLOR "%02d/%04d" RESET_COLOR ": \n\n", month, year);
    printf(" Mo  Tu  We  Th  Fr  Sa  Su\n");

    // Determine the start day of the week for the first day of the month
    struct tm first_day = {.tm_year = year - 1900, .tm_mon = month - 1, .tm_mday = 1};
    mktime(&first_day);                                             // Normalize the structure
    int start_day = first_day.tm_wday == 0 ? 7 : first_day.tm_wday; // Sunday -> 7

    // Empty spaces before the first day of the month
    for (int i = 1; i < start_day; i++)
    {
        printf("    ");
    }

    int current_day, current_month, current_year;
    get_current_day_month_year(&current_day, &current_month, &current_year);
    // Display the days of the month
    for (int day_i = 1; day_i <= days_in_month; day_i++)
    {
        if (has_event(day_i, month, year))
        {
            if (year == current_year && month == current_month && day_i == current_day)
            {
                printf(MAGENTA_COLOR " %2d" RESET_COLOR, day_i); // Marks current day with event
            }
            else if ((year < current_year) ||
                     (year == current_year && month < current_month) ||
                     (year == current_year && month == current_month && day_i < current_day))
            {
                printf(GRAY_COLOR " %2d" RESET_COLOR, day_i); // Marks past events
            }
            else
            {
                printf(BLUE_COLOR " %2d" RESET_COLOR, day_i); // Marks future events
            }
        }
        else
        {
            if (year == current_year && month == current_month && day_i == current_day)
            {
                printf(RED_COLOR " %2d" RESET_COLOR, day_i); // Marks current day
            }
            else
            {
                printf(" %2d", day_i); // Marks other days
            }
        }

        if ((day_i + start_day - 1) % 7 == 0)
        { // New line after Sunday
            printf("\n");
        }
        else
        {
            printf(" ");
        }
    }
    printf("\n");
}

// Display the months of the year
void display_month_view()
{
    int month = view_month;
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
                printf(MAGENTA_COLOR " %-9s" RESET_COLOR, month_names[month_i - 1]);
            }
            else if ((year < current_year) ||
                     (year == current_year && month_i < current_month))
            {
                printf(GRAY_COLOR " %-9s" RESET_COLOR, month_names[month_i - 1]);
            }
            else
            {
                printf(BLUE_COLOR " %-9s" RESET_COLOR, month_names[month_i - 1]);
            }
        }
        else
        {
            if (year == current_year && month_i == current_month)
            {
                printf(RED_COLOR " %-9s" RESET_COLOR, month_names[month_i - 1]);
            }
            else
            {
                printf(" %-9s", month_names[month_i - 1]);
            }
        }

        if (month_i % 3 == 0)
        {
            printf("\n");
        }
    }
}

void initialize_view()
{
    view_mode = 0;
    get_current_day_month_year(&view_day, &view_month, &view_year);
}

void clear()
{
    system("clear");
}

void press_enter_to_continue()
{
    printf("\nPress Enter to continue...");
    empty_input_buffer();
}

int input_validation_addDate(char *date, int length)
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

// EVENTS --------------------
// Function to add a new event
void add_event()
{
    if (event_count >= MAX_EVENTS)
    {
        printf("%sEvent limit reached. Cannot add more events.\n%s", RED_COLOR, RESET_COLOR);
        return;
    }

    char date[MAX_DATE_LENGTH];
    char name[MAX_NAME_LENGTH];
    char description[MAX_DESC_LENGTH];

    // Get the current date
    int current_day, current_month, current_year;
    get_current_day_month_year(&current_day, &current_month, &current_year);

    int year, month, day;

    while (1)
    {
        printf("Enter the event date (YYYY-MM-DD): ");
        if (input_validation_addDate(date, MAX_DATE_LENGTH))
        {
            if (sscanf(date, "%d-%d-%d", &year, &month, &day) != 3 || month < 1 || month > 12 || day < 1 || day > get_days_in_month(month, year) || year < 1 || year > 9999)
            {
                printf("%s\nInvalid date. Please enter a valid date.\n\n%s", RED_COLOR, RESET_COLOR);
                continue;
            }
            break;
        }
    }

    // Validate and format the entered date
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

    // Format the date to ensure two digits for month and day
    snprintf(date, sizeof(date), "%04d-%02d-%02d", year, month, day);

    while (1)
    {
        printf("Enter the event name (maximum %d characters): ", MAX_NAME_LENGTH);
        if (input_validation_addDate(name, MAX_NAME_LENGTH))
        {
            break;
        }
    }

    while (1)
    {
        printf("Enter the event description (maximum %d characters): ", MAX_DESC_LENGTH);
        if (input_validation_addDate(description, MAX_DESC_LENGTH))
        {
            break;
        }
    }

    // Add the event to the array
    events[event_count].id = next_event_id++;
    strncpy(events[event_count].date, date, MAX_DATE_LENGTH);
    strncpy(events[event_count].name, name, MAX_NAME_LENGTH);
    strncpy(events[event_count].description, description, MAX_DESC_LENGTH);
    event_count++;

    printf("%s\nEvent added successfully!\n%s", GREEN_COLOR, RESET_COLOR);
}

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

// Function to remove an event
void remove_event()
{
    if (event_count == 0)
    {
        printf("%sNo events to remove.\n%s", RED_COLOR, RESET_COLOR);
        return;
    }

    unsigned int id = get_valid_unsigned_integer();

    // Find and remove the event
    for (int i = 0; i < event_count; i++)
    {
        if (events[i].id == id)
        {
            printf("%s\nEvent removed successfully!\n%s", GREEN_COLOR, RESET_COLOR);

            // Shift remaining events
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

// Helper function for sorting events by date
int compare_events(const void *a, const void *b)
{
    const Event *event_a = (const Event *)a;
    const Event *event_b = (const Event *)b;
    return strcmp(event_a->date, event_b->date);
}

// Function to view all events
void view_events()
{
    if (event_count == 0)
    {
        printf("-----------------------------\n");
        printf("No events found.\n");
        printf("-----------------------------\n");
        return;
    }

    // Get the current date
    int current_day, current_month, current_year;
    get_current_day_month_year(&current_day, &current_month, &current_year);

    // Prepare a structure for current time comparison
    struct tm current_time = {.tm_year = current_year - 1900, .tm_mon = current_month - 1, .tm_mday = current_day};
    time_t now = mktime(&current_time);

    // Arrays to hold past and future events
    Event past_events[MAX_EVENTS];
    Event future_events[MAX_EVENTS];
    int past_count = 0, future_count = 0;

    // Categorize events into past and future (including today)
    for (int i = 0; i < event_count; i++)
    {
        int event_year, event_month, event_day;
        sscanf(events[i].date, "%d-%d-%d", &event_year, &event_month, &event_day);

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

    // Sort events
    qsort(past_events, past_count, sizeof(Event), compare_events);
    qsort(future_events, future_count, sizeof(Event), compare_events);

    // Display past events
    printf("======== Past Events ========\n");
    for (int i = 0; i < past_count; i++)
    {
        printf("ID: %d\nDate: %s%s%s\nName: %s\nDescription: %s\n",
               past_events[i].id, GRAY_COLOR, past_events[i].date, RESET_COLOR, past_events[i].name, past_events[i].description);
        printf("-----------------------------\n");
    }

    // Display future events (including today)
    printf("======= Future Events =======\n");
    for (int i = 0; i < future_count; i++)
    {
        int event_year, event_month, event_day;
        sscanf(future_events[i].date, "%d-%d-%d", &event_year, &event_month, &event_day);

        const char *color = (event_year == current_year && event_month == current_month && event_day == current_day)
                                ? MAGENTA_COLOR
                                : BLUE_COLOR;

        printf("ID: %d\nDate: %s%s%s\nName: %s\nDescription: %s\n",
               future_events[i].id, color, future_events[i].date, RESET_COLOR, future_events[i].name, future_events[i].description);
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

// Function to display the menu
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

    printf("\n\n=========== Menu ===========\n");

    if (logged_in(user))
    {
        printf("%s\nLogged in as: %s%s%s\n", BOLD, YELLOW_COLOR, user, RESET_COLOR);
    }
    else
    {
        printf("%s\nNot logged in.\n%s", BOLD, RESET_COLOR);
    }

    printf("\n--- Event Management ---\n");
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

    printf("\n--- Calendar View ---\n");
    printf("%s4.%s Reset View\n", RED_COLOR, RESET_COLOR);
    printf("%s5.%s Navigate\n\n", RED_COLOR, RESET_COLOR);

    printf("%s6.%s Exit\n", RED_COLOR, RESET_COLOR);

    printf("\n============================\n");
    printf("Choose an option: ");
}

int main(int argc, char *argv[])
{
    clear();

    if (argc < 2)
    {
        printf("No user provided.\n");
        return 1;
    }
    char *user = argv[1];

    char choice;
    initialize_view();

    while (1)
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
                add_event();
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
                remove_event();
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
        case '6':
            clear();
            return 0;
        default:;
        }
        clear();
    }
    return 0;
}

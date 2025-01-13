#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/sha.h>

#define RESET_COLOR   "\033[0m"
#define BOLD          "\033[1m"
#define RED_COLOR     "\033[31m"
#define GREEN_COLOR   "\033[32m"
#define BLUE_COLOR    "\033[34m"
#define MAGENTA_COLOR "\033[35m"
#define GRAY_COLOR    "\033[90m"


#define MAX_EVENTS 100
#define MAX_NAME_LENGTH 50
#define MAX_DESC_LENGTH 256
#define MAX_DATE_LENGTH 20

#define user "admin"
#define SHA256_DIGEST_LENGTH 32


// Structure for an event
typedef struct {
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

int priviliged = 0;


// CALENDAR VIEW --------------------
// Function to calculate the number of days in the current month
int get_days_in_month(int month, int year) {
    if (month == 2) { // February
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
void get_current_day_month_year(int* day, int* month, int* year) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);

    *day = tm_info->tm_mday;
    *month = tm_info->tm_mon + 1;
    *year = tm_info->tm_year + 1900;
}

// Function to check if a day has an event
int has_event(int day, int month, int year) {
    if (day == 0) { // Check if there is any event in the given month
        for (int i = 0; i < event_count; i++) {
            int event_month = atoi(events[i].date + 5); // Extract the month from the date
            int event_year = atoi(events[i].date); // Extract the year from the date
            if (event_month == month && event_year == year) {
                return 1; // Event found in this month
            }
        }
        return 0; // No events found in this month
    }

    char date[MAX_DATE_LENGTH];
    snprintf(date, sizeof(date), "%04d-%02d-%02d", year, month, day);

    // Search for an event with the same date in the events array
    for (int i = 0; i < event_count; i++) {
        if (strcmp(events[i].date, date) == 0) {
            return 1; // Event found on this date
        }
    }
    return 0; // No event found on this date
}

// Function to display a month
void display_day_view() {
    int day = view_day;
    int month = view_month;
    int year = view_year;

    int days_in_month = get_days_in_month(month, year);

    printf("Calendar for " RED_COLOR "%02d/%04d" RESET_COLOR ": \n\n", month, year);
    printf(" Mo  Tu  We  Th  Fr  Sa  Su\n");

    // Determine the start day of the week for the first day of the month
    struct tm first_day = { .tm_year = year - 1900, .tm_mon = month - 1, .tm_mday = 1 };
    mktime(&first_day); // Normalize the structure
    int start_day = first_day.tm_wday == 0 ? 7 : first_day.tm_wday; // Sunday -> 7

    // Empty spaces before the first day of the month
    for (int i = 1; i < start_day; i++) {
        printf("    ");
    }

    int current_day, current_month, current_year;
    get_current_day_month_year(&current_day, &current_month, &current_year);
    // Display the days of the month
    for (int day_i = 1; day_i <= days_in_month; day_i++) {
        if (has_event(day_i, month, year)) {
            if (year == current_year && month == current_month && day_i == current_day) {
                printf(MAGENTA_COLOR " %2d" RESET_COLOR, day_i); // Marks current day with event
            }
            else if ((year < current_year) ||
                (year == current_year && month < current_month) ||
                (year == current_year && month == current_month && day_i < current_day)) {
                printf(GRAY_COLOR " %2d" RESET_COLOR, day_i); // Marks past events
            }
            else {
                printf(BLUE_COLOR " %2d" RESET_COLOR, day_i); // Marks future events
            }
        }
        else {
            if (year == current_year && month == current_month && day_i == current_day) {
                printf(RED_COLOR " %2d" RESET_COLOR, day_i); // Marks current day
            }
            else {
                printf(" %2d", day_i); // Marks other days
            }
        }

        if ((day_i + start_day - 1) % 7 == 0) { // New line after Sunday
            printf("\n");
        }
        else {
            printf(" ");
        }
    }
    printf("\n");
}

// Display the months of the year
void display_month_view() {
    int month = view_month;
    int year = view_year;

    const char* month_names[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };

    int current_day, current_month, current_year;
    get_current_day_month_year(&current_day, &current_month, &current_year);
    printf("Calendar for " RED_COLOR "%d" RESET_COLOR ":\n\n", year);
    for (int month_i = 1; month_i <= 12; month_i++) {
        if (has_event(0, month_i, year)) {
            if (year == current_year && month_i == current_month) {
                printf(MAGENTA_COLOR " %-9s" RESET_COLOR, month_names[month_i - 1]);
            }
            else if ((year < current_year) ||
                (year == current_year && month_i < current_month)) {
                printf(GRAY_COLOR " %-9s" RESET_COLOR, month_names[month_i - 1]);
            }
            else {
                printf(BLUE_COLOR " %-9s" RESET_COLOR, month_names[month_i - 1]);
            }
        } else {
            if (year == current_year && month_i == current_month) {
                printf(RED_COLOR " %-9s" RESET_COLOR, month_names[month_i - 1]);
            }
            else {
                printf(" %-9s", month_names[month_i - 1]);
            }
        }

        if (month_i % 3 == 0) {
            printf("\n");
        }
    }
}

void initialize_view() {
    view_mode = 0;
    get_current_day_month_year(&view_day, &view_month, &view_year);
}

void clear() {
    system("cls");
}

void press_enter_to_continue() {
    printf("\nPress Enter to continue...");
    getchar();
}

// EVENTS --------------------
// Function to add a new event
void add_event() {
    if (priviliged) {
        if (event_count >= MAX_EVENTS) {
            printf("%sEvent limit reached. Cannot add more events.\n%s", RED_COLOR, RESET_COLOR);
            return;
        }

        char date[MAX_DATE_LENGTH];
        char name[MAX_NAME_LENGTH];
        char description[MAX_DESC_LENGTH];

        // Get the current date
        int current_day, current_month, current_year;
        get_current_day_month_year(&current_day, &current_month, &current_year);

        printf("Enter the event date (YYYY-MM-DD): ");
        fgets(date, MAX_DATE_LENGTH, stdin);
        date[strcspn(date, "\n")] = '\0'; // Remove the trailing newline

        int year, month, day;

        // Validate and format the entered date
        /*
        if (sscanf(date, "%d-%d-%d", &year, &month, &day) != 3 ||
            year < current_year || year > 2100 ||
            (year == current_year && (month < current_month || (month == current_month && day < current_day))) ||
            month < 1 || month > 12 || day < 1 || day > 31) {
            printf("Invalid date. Please enter a valid date between today and the year 2100.\n");
            return;
        }
        */
        if (sscanf(date, "%d-%d-%d", &year, &month, &day) != 3 || month < 1 || month > 12 || day < 1 || day > 31) {
            printf("%sInvalid date. Please enter a valid date between today and the year 2100.\n%s", RED_COLOR, RESET_COLOR);
            return;
        }

        // Format the date to ensure two digits for month and day
        snprintf(date, sizeof(date), "%04d-%02d-%02d", year, month, day);

        printf("Enter the event name (maximum %d characters): ", MAX_NAME_LENGTH);
        fgets(name, MAX_NAME_LENGTH, stdin);
        name[strcspn(name, "\n")] = '\0'; // Remove the trailing newline

        printf("Enter the event description (maximum %d characters): ", MAX_DESC_LENGTH);
        fgets(description, MAX_DESC_LENGTH, stdin);
        description[strcspn(description, "\n")] = '\0'; // Remove the trailing newline

        // Add the event to the array
        events[event_count].id = next_event_id++;
        strncpy(events[event_count].date, date, MAX_DATE_LENGTH);
        strncpy(events[event_count].name, name, MAX_NAME_LENGTH);
        strncpy(events[event_count].description, description, MAX_DESC_LENGTH);
        event_count++;

        printf("%sEvent added successfully!\n%s", GREEN_COLOR, RESET_COLOR);
    }
    else {
        printf("%sYou are not privileged to do this!\n%s", RED_COLOR, RESET_COLOR);
    }
}


// Function to remove an event
void remove_event() {
    if (priviliged) {
        if (event_count == 0) {
            printf("No events to remove.\n");
            return;
        }

        int id;
        printf("Enter the ID of the event to remove: ");
        scanf("%d", &id);
        getchar(); // Remove '\n'

        // Find and remove the event
        for (int i = 0; i < event_count; i++) {
            if (events[i].id == id) {
                printf("%sEvent removed successfully!\n%s", GREEN_COLOR, RESET_COLOR);

                // Shift remaining events
                for (int j = i; j < event_count - 1; j++) {
                    events[j] = events[j + 1];
                }
                event_count--;
                return;
            }
        }

        printf("%sNo event found with ID %d.\n%s", RED_COLOR, id, RESET_COLOR);
    }
    else {
        printf("%sYou are not privileged to do this!\n%s", RED_COLOR, RESET_COLOR);
    }
}

// Helper function for sorting events by date
int compare_events(const void* a, const void* b) {
    const Event* event_a = (const Event*)a;
    const Event* event_b = (const Event*)b;
    return strcmp(event_a->date, event_b->date);
}

// Function to view all events
void view_events() {
    printf("----------------------------\n");

    if (event_count == 0) {
        printf("No events found.\n");
        printf("----------------------------\n");
        return;
    }

    // Get the current date
    int current_day, current_month, current_year;
    get_current_day_month_year(&current_day, &current_month, &current_year);

    // Prepare a structure for current time comparison
    struct tm current_time = { .tm_year = current_year - 1900, .tm_mon = current_month - 1, .tm_mday = current_day };
    time_t now = mktime(&current_time);

    // Arrays to hold past and future events
    Event past_events[MAX_EVENTS];
    Event future_events[MAX_EVENTS];
    int past_count = 0, future_count = 0;

    // Categorize events into past and future (including today)
    for (int i = 0; i < event_count; i++) {
        int event_year, event_month, event_day;
        sscanf(events[i].date, "%d-%d-%d", &event_year, &event_month, &event_day);

        struct tm event_time = { .tm_year = event_year - 1900, .tm_mon = event_month - 1, .tm_mday = event_day };
        time_t event_timestamp = mktime(&event_time);

        if (event_timestamp < now) {
            past_events[past_count++] = events[i];
        }
        else {
            future_events[future_count++] = events[i];
        }
    }

    // Sort events
    qsort(past_events, past_count, sizeof(Event), compare_events);
    qsort(future_events, future_count, sizeof(Event), compare_events);

    // Display past events
    printf("Past Events:\n");
    if (priviliged) {
        for (int i = 0; i < past_count; i++) {
            printf(GRAY_COLOR "ID: %d\nDate: %s\nName: %s\nDescription: %s\n" RESET_COLOR,
                past_events[i].id, past_events[i].date, past_events[i].name, past_events[i].description);
            printf("----------------------------\n");
        }
    }
    else {
        printf("%s\nYou are not privileged to access this!\n\n%s", RED_COLOR, RESET_COLOR);
        printf("----------------------------\n");
    }

    // Display future events (including today)
    printf("Future Events:\n");
    for (int i = 0; i < future_count; i++) {
        int event_year, event_month, event_day;
        sscanf(future_events[i].date, "%d-%d-%d", &event_year, &event_month, &event_day);

        const char* color = (event_year == current_year && event_month == current_month && event_day == current_day)
            ? MAGENTA_COLOR
            : BLUE_COLOR;

        printf("%sID: %d\nDate: %s\nName: %s\nDescription: %s\n" RESET_COLOR,
            color, future_events[i].id, future_events[i].date, future_events[i].name, future_events[i].description);
        printf("----------------------------\n");
    }
}

// Login
void hashPassword(const char* input, char* output) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)input, strlen(input), hash);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(&output[i * 2], "%02x", hash[i]);
    }
    output[64] = '\0';
}

void login() {
    char inputUsername[16];
    char inputPassword[32];
    char hashedInput[65] = { 0 };
    const char* hashed_password = "ef92b778bafe771e89245b89ecbc08a44a4e166c06659911881f383d4473e94f";

    printf("=====================================\n");
    printf("   Login to gain privileged access    \n");
    printf("=====================================\n\n");

    printf("%sUsername: %s", BOLD, RESET_COLOR);
    scanf("%15s", inputUsername);
    getchar();

    if (strcmp(inputUsername, user) == 0) {
        printf("\n%sPassword: %s", BOLD, RESET_COLOR);
        scanf("%31s", inputPassword);
        getchar();

        hashPassword(inputPassword, hashedInput);

        if (strcmp(hashedInput, hashed_password) == 0) {
            printf("\n%sLogin successful! Privileged access granted.\n%s", GREEN_COLOR, RESET_COLOR);
            priviliged = 1;
        }
        else {
            printf("\n%sInvalid password.%s\n", RED_COLOR, RESET_COLOR);
        }
    }
    else {
        printf("\n%sInvalid username.%s\n", RED_COLOR, RESET_COLOR);
    }

    printf("\n=====================================\n");
}


// MENU --------------------
// Function to navigate between views
void navigate() {
    char choice;

    while (1) {
        clear();

        if (view_mode == 0) { // Month View
            display_day_view();
            printf("\nUse 'n' for next month, 'p' for previous month, 'y' for year view, 'q' to quit navigator.\n");
        }
        else if (view_mode == 1) { // Year View
            display_month_view();
            printf("\nUse 'n' for next year, 'p' for previous year, 'm' for month view, 'q' to quit navigator.\n");
        }

        scanf(" %c", &choice);

        if (view_mode == 0) { // Month View Navigation
            switch (choice) {
            case 'n':
                view_month++;
                if (view_month > 12) {
                    view_month = 1;
                    view_year++;
                }
                break;
            case 'p':
                view_month--;
                if (view_month < 1) {
                    view_month = 12;
                    view_year--;
                }
                break;
            case 'y': // Switch to Year View
                view_mode = 1;
                break;
            case 'q':
                return;
            default:
                ;
            }
        }
        else if (view_mode == 1) { // Year View Navigation
            switch (choice) {
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
            default:
                ;
            }
        }
    }
}


// Function to display the menu
void show_menu() {
    if (!view_mode) {
        display_day_view();
    }
    else {
        display_month_view();
    }

    printf("\n\n=========== Menu ===========\n");

    printf("\n--- Event Management ---\n");
    if (priviliged) {
        printf("1. Add Event\n");
        printf("2. Remove Event\n");
    }
    else {
        printf(GRAY_COLOR "1. Add Event\n" RESET_COLOR);
        printf(GRAY_COLOR "2. Remove Event\n" RESET_COLOR);
    }
    printf("3. View Events\n");

    printf("\n--- Calendar View ---\n");
    printf("4. Reset View\n");
    printf("5. Navigate\n\n");

    printf("6. Login\n");
    printf("7. Exit\n");

    printf("\n============================\n");
    printf("Choose an option: ");
}


int main() {
    int choice;
    initialize_view();

    while (1) {
        show_menu();
        scanf("%d", &choice);
        getchar(); // Remove '\n'
        switch (choice) {
        case 1:
            clear();
            add_event();
            press_enter_to_continue();
            break;
        case 2:
            clear();
            remove_event();
            press_enter_to_continue();
            break;
        case 3:
            clear();
            view_events();
            press_enter_to_continue();
            break;
        case 4:
            initialize_view();
            break;
        case 5:
            navigate();
            break;
        case 6:
            clear();
            login();
            press_enter_to_continue();
            break;
        case 7:
            printf("Program exited.\n");
            return 0;
        default:
            ;
        }
        clear();
    }
    return 0;
}

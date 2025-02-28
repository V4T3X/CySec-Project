#include <hiredis/hiredis.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argon2.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include "common.h"
#include <time.h>

// login parameter
#define HASHED_PWD_LENGTH 32
#define PWD_LENGTH 48
#define USERNAME_LENGTH 32
#define SALT_LENGTH 16

// function to validate input for password and username
int input_validation(char *input, const char *str, int length)
{
    if (fgets(input, length, stdin) == NULL)
    {
        printf("%s\nFailed to read input.%s\n\n", RED_COLOR, RESET_COLOR);
        return 0;
    }

    if (strchr(input, '\n') == NULL)
    {
        empty_input_buffer();
        printf("%s\n%s too long.%s\n\n", RED_COLOR, str, RESET_COLOR);
        return 0;
    }

    input[strcspn(input, "\n")] = '\0';

    if (strlen(input) == 0)
    {
        printf("%s\n%s cannot be empty.%s\n\n", RED_COLOR, str, RESET_COLOR);
        return 0;
    }

    return 1;
}

// strict validation for username
int is_valid_username(const char *username)
{
    const char *allowed_special_chars = "_-@.+";

    for (int i = 0; username[i] != '\0'; i++)
    {
        if (!isalnum(username[i]) && strchr(allowed_special_chars, username[i]) == NULL)
        {
            printf("%s\nInvalid character '%c' in username.\n\n%s", RED_COLOR, username[i], RESET_COLOR);
            return 0;
        }
    }
    return 1;
}

// register new user
void register_user(redisContext *c)
{
    char username[USERNAME_LENGTH] = {0};
    char password[PWD_LENGTH] = {0};
    unsigned char salt[SALT_LENGTH];
    unsigned char hashed_password[HASHED_PWD_LENGTH];
    unsigned char combined[HASHED_PWD_LENGTH + SALT_LENGTH];
    char base64_combined[EVP_ENCODE_LENGTH(sizeof(combined))];

    redisReply *reply;

    while (1)
    {
        printf("Enter username: ");
        if (!input_validation(username, "Username", sizeof(username)) || !is_valid_username(username))
        {
            continue;
        }

        reply = redisCommand(c, "EXISTS user:%s", username);
        if (!reply)
        {
            printf("%s\nError: Redis command failed.\n%s", RED_COLOR, RESET_COLOR);
            continue;
        }

        if (reply->integer == 1)
        {
            printf("%s\nUser '%s' already exists. Please choose a different username.%s\n\n", ORANGE_COLOR, username, RESET_COLOR);
        }
        else
        {
            freeReplyObject(reply);
            break;
        }
        freeReplyObject(reply);
    }

    // password validation
    while (1)
    {
        printf("Enter password: ");
        if (!input_validation(password, "Password", sizeof(password)))
        {
            continue;
        }
        break;
    }

    // generate salt
    if (!RAND_bytes(salt, sizeof(salt)))
    {
        printf("%s\nError: Failed to generate random salt.%s\n", RED_COLOR, RESET_COLOR);
        return;
    }

    // hash password
    if (argon2id_hash_raw(2, 1 << 16, 1, password, strlen(password), salt, sizeof(salt), hashed_password, sizeof(hashed_password)) != ARGON2_OK)
    {
        printf("%s\nError: Failed to hash the password.%s\n", RED_COLOR, RESET_COLOR);
        return;
    }

    memcpy(combined, hashed_password, sizeof(hashed_password));
    memcpy(combined + sizeof(hashed_password), salt, sizeof(salt));

    // base64 encoding
    if (EVP_EncodeBlock((unsigned char *)base64_combined, combined, sizeof(combined)) == -1)
    {
        printf("%s\nError: Base64 encoding failed.%s\n", RED_COLOR, RESET_COLOR);
        return;
    }

    // store in db
    time_t now = time(NULL);
    reply = redisCommand(c, "HSET user:%s password %s created_at %ld", username, base64_combined, now);
    if (!reply || reply->type != REDIS_REPLY_INTEGER)
    {
        printf("%s\nError: Failed to save user to Redis.%s\n", RED_COLOR, RESET_COLOR);
        if (reply)
            freeReplyObject(reply);
        return;
    }

    printf("%s\nUser '%s' registered successfully.%s\n", GREEN_COLOR, username, RESET_COLOR);

    freeReplyObject(reply);
    memset(password, 0, sizeof(password));
    memset(hashed_password, 0, sizeof(hashed_password));
    memset(salt, 0, sizeof(salt));

    return;
}

// function for user login
void login_user(redisContext *c, char *user)
{
    char username[USERNAME_LENGTH] = {0};
    char password[PWD_LENGTH] = {0};
    unsigned char stored_salt[SALT_LENGTH];
    unsigned char stored_hashed_password[HASHED_PWD_LENGTH];
    unsigned char stored_combined[HASHED_PWD_LENGTH + SALT_LENGTH];
    unsigned char hashed_password[HASHED_PWD_LENGTH];

    // username validation
    printf("Enter username: ");
    if (!input_validation(username, "Username", sizeof(username)) || !is_valid_username(username))
    {
        return;
    }

    // username check in db
    redisReply *reply = redisCommand(c, "EXISTS user:%s", username);
    if (!reply)
    {
        printf("Error: Redis command failed.\n");
        return;
    }
    if (reply->integer == 0)
    {
        printf("%s\nUser '%s' does not exist.%s\n", RED_COLOR, username, RESET_COLOR);
        freeReplyObject(reply);
        return;
    }
    freeReplyObject(reply);

    // password validation
    printf("Enter password: ");
    if (!input_validation(password, "Password", sizeof(password)))
    {
        return;
    }

    // retrieve stored hash and salt from db
    reply = redisCommand(c, "HGET user:%s password", username);
    if (!reply || reply->type != REDIS_REPLY_STRING)
    {
        printf("%s\nError: Failed to retrieve user data from Redis.%s\n", RED_COLOR, RESET_COLOR);
        if (reply)
            freeReplyObject(reply);
        return;
    }

    // base64 decode the stored combined (password + salt)
    int decoded_length = EVP_DecodeBlock(stored_combined, (unsigned char *)reply->str, strlen(reply->str));
    if (decoded_length == -1)
    {
        printf("%s\nError: Base64 decoding failed.%s\n", RED_COLOR, RESET_COLOR);
        freeReplyObject(reply);
        return;
    }
    freeReplyObject(reply);

    // split the decoded combined into the stored password and salt
    memcpy(stored_hashed_password, stored_combined, sizeof(stored_hashed_password));
    memcpy(stored_salt, stored_combined + sizeof(stored_hashed_password), sizeof(stored_salt));

    // hash the input password with the stored salt
    if (argon2id_hash_raw(2, 1 << 16, 1, password, strlen(password), stored_salt, sizeof(stored_salt), hashed_password, sizeof(hashed_password)) != ARGON2_OK)
    {
        printf("%s\nError: Failed to hash the password.%s\n", RED_COLOR, RESET_COLOR);
        return;
    }

    // compare the hashed passwords
    if (memcmp(hashed_password, stored_hashed_password, sizeof(stored_hashed_password)) == 0)
    {
        printf("%s\nYou have successfully logged in as '%s'.%s\n", GREEN_COLOR, username, RESET_COLOR);
        strncpy(user, username, USERNAME_LENGTH - 1);
        user[USERNAME_LENGTH - 1] = '\0';
    }
    else
    {
        printf("%s\nIncorrect password.%s\n", RED_COLOR, RESET_COLOR);
    }

    memset(password, 0, sizeof(password));
    memset(hashed_password, 0, sizeof(hashed_password));
    memset(stored_hashed_password, 0, sizeof(stored_hashed_password));
    memset(stored_salt, 0, sizeof(stored_salt));

    return;
}

// open calendar for logged-in user
void open_calendar(redisContext *c, const char *username, const char *privilege_level)
{
    if (!is_valid_username(username))
    {
        press_enter_to_continue();
        return;
    }

    pid_t pid = fork();

    if (pid == 0)
    {
        execl("./calendar", "./calendar", username, privilege_level, (char *)NULL);
        printf("%sFailed to start calendar.\n%s", RED_COLOR, RESET_COLOR);
        exit(1);
    }
    else if (pid > 0)
    {
        int status;
        waitpid(pid, &status, 0);
    }
    else
    {
        printf("%sFork failed.%s\n", RED_COLOR, RESET_COLOR);
        press_enter_to_continue();
    }
}

// structure to store user data and created_at timestamp
typedef struct
{
    char *username;
    unsigned int created_at;
} User;

// comparison function for sorting users by created_at
int compare_users(const void *a, const void *b)
{
    // compare the created_at timestamps
    return ((User *)a)->created_at - ((User *)b)->created_at;
}

void display_registered_user(redisContext *c, char *logged_in_user)
{
    // retrieve all user keys from the Redis database
    redisReply *keys_reply = redisCommand(c, "KEYS user:*");

    if (keys_reply == NULL || keys_reply->type != REDIS_REPLY_ARRAY)
    {
        printf("%sError: Unable to retrieve user data.%s\n", RED_COLOR, RESET_COLOR);
        freeReplyObject(keys_reply);
        return;
    }

    // if no user keys are found
    if (keys_reply->elements == 0)
    {
        printf("No registered users found.\n");
        freeReplyObject(keys_reply);
        return;
    }

    // prepare memory to store user data
    User *users = malloc(keys_reply->elements * sizeof(User));
    size_t user_count = 0;

    // iterate through all the user keys
    for (size_t i = 0; i < keys_reply->elements; i++)
    {
        char *user_key = keys_reply->element[i]->str;

        // extract the username from the user key
        char *username_str = strrchr(user_key, ':');
        if (!username_str || strlen(username_str) < 2)
            continue;
        char *username = username_str + 1;

        // retrieve the creation timestamp for the user
        redisReply *user_reply = redisCommand(c, "HGET %s created_at", user_key);

        // check if user data was retrieved successfully
        if (user_reply == NULL || user_reply->type != REDIS_REPLY_STRING)
        {
            printf("%sError: Unable to retrieve data for user %s.%s\n", RED_COLOR, user_key, RESET_COLOR);
            freeReplyObject(user_reply);
            continue;
        }

        // convert Unix timestamp to an unsigned int
        unsigned int created_at = atoi(user_reply->str);

        // store the user data
        users[user_count].username = strdup(username);
        users[user_count].created_at = created_at;
        user_count++;

        freeReplyObject(user_reply);
    }

    // sort users by the created_at timestamp
    qsort(users, user_count, sizeof(User), compare_users);

    // print the table header
    printf("%s%s  %s(Sorted by Registration Timestamp)\n", BOLD, "Registered Users", RESET_COLOR);
    printf("%s----------------------------------------------------------------%s\n\n", BLUE_COLOR, RESET_COLOR);

    // display sorted user data
    for (size_t i = 0; i < user_count; i++)
    {
        // convert the timestamp to a human-readable format
        time_t raw_time = (time_t)users[i].created_at;
        struct tm *time_info = localtime(&raw_time);

        char formatted_time[20];
        strftime(formatted_time, sizeof(formatted_time), "%Y-%m-%d %H:%M", time_info);

        // display username and creation time
        printf("%s%s%s  (%s)\n\n", YELLOW_COLOR, users[i].username, RESET_COLOR, formatted_time);

        free(users[i].username); // free memory for username
    }

    // free memory for the users array
    free(users);
    freeReplyObject(keys_reply);

    printf("%s----------------------------------------------------------------%s\n\n", BLUE_COLOR, RESET_COLOR);
    printf("Select a user to view their public events (visible to everyone).\n\n%sYour choice: %s", BOLD, RESET_COLOR);

    char username[USERNAME_LENGTH] = {0};
    if (!input_validation(username, "Username", sizeof(username)) || !is_valid_username(username))
    {
        press_enter_to_continue();
        return;
    }

    // username check in db
    redisReply *reply = redisCommand(c, "EXISTS user:%s", username);
    if (!reply)
    {
        printf("Error: Redis command failed.\n");
        return;
    }
    if (reply->integer == 0)
    {
        printf("%s\nUser '%s' does not exist.%s\n", RED_COLOR, username, RESET_COLOR);
        press_enter_to_continue();
        freeReplyObject(reply);
        return;
    }
    freeReplyObject(reply);

    if (strcmp(username, logged_in_user) == 0)
    {
        open_calendar(c, username, "1");
    }
    else
    {
        open_calendar(c, username, "0");
    }
}

void show_menu(const char *logged_in_user)
{
    printf("=====================================\n");
    printf("%s        %sCommand Line Calendar        \n%s", BOLD, GREEN_COLOR, RESET_COLOR);
    printf("=====================================\n");
    printf("\n");

    if (logged_in_user != NULL && logged_in_user[0] != '\0')
    {
        printf("%sLogged in as: %s%s%s\n", BOLD, YELLOW_COLOR, logged_in_user, RESET_COLOR);
    }
    else
    {
        printf("%sYou are not logged in.\n%s", BOLD, RESET_COLOR);
    }

    printf("\n");
    printf("%s1.%s Register\n", RED_COLOR, RESET_COLOR);
    printf("%s2.%s Login\n", RED_COLOR, RESET_COLOR);
    printf("%s3.%s View and Edit Your Calendar\n", RED_COLOR, RESET_COLOR);
    printf("%s4.%s Browse Users and Their Calendars\n", RED_COLOR, RESET_COLOR);

    printf("\n%s5.%s Logout\n", RED_COLOR, RESET_COLOR);
    printf("%s6.%s Exit\n", RED_COLOR, RESET_COLOR);
    printf("\n");
    printf("=====================================\n");
    printf("Enter your choice: ");
}

// MAIN ------------
int main()
{
    clear();
    redisContext *c = connect_redis();
    char user[USERNAME_LENGTH] = "";

    char choice;

    do
    {
        show_menu(user);

        choice = getchar();
        empty_input_buffer();

        switch (choice)
        {
        case '1':
            clear();
            register_user(c);
            press_enter_to_continue();
            break;

        case '2':
            clear();
            login_user(c, user);
            press_enter_to_continue();
            break;

        case '3':
            clear();
            open_calendar(c, user, "1");
            break;

        case '4':
            clear();
            display_registered_user(c, user);
            break;

        case '5':
            clear();
            memset(user, 0, USERNAME_LENGTH);
            break;

        default:;
        }
        clear();
    } while (choice != '6');

    redisFree(c);
    return 0;
}

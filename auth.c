// Compile the program: gcc -o auth auth.c -lhiredis -largon2 -lssl -lcrypto

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

#define RESET_COLOR "\033[0m"
#define ORANGE_COLOR "\033[38;5;214m"
#define BOLD "\033[1m"
#define RED_COLOR "\033[31m"
#define GREEN_COLOR "\033[32m"
#define GRAY_COLOR "\033[90m"
#define YELLOW_COLOR "\033[0;33m"

#define HASHED_PWD_LENGTH 32
#define PWD_LENGTH 48
#define USERNAME_LENGTH 32
#define SALT_LENGTH 16

// connect with redis
redisContext *connect_redis()
{
    redisContext *c = redisConnect("127.0.0.1", 6379);
    if (c == NULL || c->err)
    {
        printf("%sError connecting to Redis: %s\n%s", RED_COLOR, c->errstr, RESET_COLOR);
        redisFree(c);
        exit(1);
    }
    return c;
}

void empty_input_buffer()
{
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF)
        ;
}

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

void login_user(redisContext *c, char *user)
{
    char username[USERNAME_LENGTH] = {0};
    char password[PWD_LENGTH] = {0};
    unsigned char stored_salt[SALT_LENGTH];
    unsigned char stored_hashed_password[HASHED_PWD_LENGTH];
    unsigned char stored_combined[HASHED_PWD_LENGTH + SALT_LENGTH];
    char base64_combined[EVP_ENCODE_LENGTH(sizeof(stored_combined))];
    unsigned char hashed_password[HASHED_PWD_LENGTH];

    // username validation
    printf("Enter username: ");
    if (!input_validation(username, "Username", sizeof(username)))
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

void press_enter_to_continue()
{
    printf("\nPress Enter to continue...");
    empty_input_buffer();
}

// show public accounts
void open_calendar(redisContext *c, const char *username)
{
    if (!is_valid_username(username))
    {
        press_enter_to_continue();
        return;
    }

    pid_t pid = fork();

    if (pid == 0)
    {
        execl("./calendar", "./calendar", username, (char *)NULL);
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

void show_menu(const char *logged_in_user)
{
    printf("=====================================\n");
    printf("%s        Command Line Calendar        \n%s", GREEN_COLOR, RESET_COLOR);
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
    printf("%s3.%s Open Calendar\n", RED_COLOR, RESET_COLOR);

    printf("\n%s4.%s Logout\n", RED_COLOR, RESET_COLOR);
    printf("%s5.%s Exit\n", RED_COLOR, RESET_COLOR);
    printf("\n");
    printf("=====================================\n");
    printf("\nEnter your choice: ");
}

void clear()
{
    system("clear");
}

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
            open_calendar(c, user);
            break;

        case '4':
            clear();
            memset(user, 0, USERNAME_LENGTH);
            break;

        default:;
        }
        clear();
    } while (choice != '5');

    redisFree(c);
    return 0;
}

// Compile the program: gcc -o auth auth.c -lhiredis -largon2 -lssl -lcrypto


#include <hiredis/hiredis.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argon2.h>
#include <openssl/rand.h>
#include <openssl/evp.h>

#define RESET_COLOR   "\033[0m"
#define ORANGE_COLOR  "\033[38;5;214m"
#define BOLD          "\033[1m"
#define RED_COLOR     "\033[31m"
#define GREEN_COLOR   "\033[32m"
#define GRAY_COLOR    "\033[90m"

// connect with redis
redisContext *connect_redis() {
    redisContext *c = redisConnect("127.0.0.1", 6379);
    if (c == NULL || c->err) {
        printf("%sError connecting to Redis: %s\n%s", RED_COLOR, c->errstr, RESET_COLOR);
        exit(1);
    }
    return c;
}

void empty_input_buffer() {
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF)
        ;
}

int input_validation(char* input, const char* str, int length) {
    if (fgets(input, length, stdin) == NULL) {
        printf("%s\nFailed to read input.%s\n", RED_COLOR, RESET_COLOR);
        return 0;
    }

    if (strchr(input, '\n') == NULL) {
        empty_input_buffer();
        printf("%s\n%s too long.%s\n", RED_COLOR, str, RESET_COLOR);
        return 0;
    }

    input[strcspn(input, "\n")] = '\0';

    if (strlen(input) == 0) {
        printf("%s\n%s cannot be empty.%s\n", RED_COLOR, str, RESET_COLOR);
        return 0;
    }

    return 1;
}

// register new user
void register_user(redisContext *c) {
    char username[32] = {0};
    char password[48] = {0};
    unsigned char salt[16];
    unsigned char hashed_password[32];
    unsigned char combined[sizeof(hashed_password) + sizeof(salt)];
    char base64_combined[EVP_ENCODE_LENGTH(sizeof(combined))];

    // username validation
    printf("Enter username: ");
    if (!input_validation(username, "Username", sizeof(username))) {
        return;
    }

    // username check in db
    redisReply *reply = redisCommand(c, "HEXISTS users %s", username);
    if (!reply || reply->integer == 1) {
        printf("%s\nUser '%s' already exists.%s\n", ORANGE_COLOR, username, RESET_COLOR);
        if (reply) freeReplyObject(reply);
        return;
    }
    freeReplyObject(reply);

    // password validation
    printf("Enter password: ");
    if (!input_validation(password, "Password", sizeof(password))) {
        return;
    }

    // generate salt
    if (!RAND_bytes(salt, sizeof(salt))) {
        printf("%s\nError: Failed to generate random salt.%s\n", RED_COLOR, RESET_COLOR);
        return;
    }

    // hash password
    if (argon2id_hash_raw(2, 1 << 16, 1, password, strlen(password), salt, sizeof(salt), hashed_password, sizeof(hashed_password)) != ARGON2_OK) {
        printf("%s\nError: Failed to hash the password.%s\n", RED_COLOR, RESET_COLOR);
        return;
    }

    memcpy(combined, hashed_password, sizeof(hashed_password));
    memcpy(combined + sizeof(hashed_password), salt, sizeof(salt));

    // base64 encoding
    if (EVP_EncodeBlock((unsigned char *)base64_combined, combined, sizeof(combined)) == -1) {
        printf("%s\nError: Base64 encoding failed.%s\n", RED_COLOR, RESET_COLOR);
        return;
    }

    // store in db
    reply = redisCommand(c, "HSET users %s %s", username, base64_combined);
    if (!reply || reply->type != REDIS_REPLY_INTEGER) {
        printf("%s\nError: Failed to save user to Redis.%s\n", RED_COLOR, RESET_COLOR);
        if (reply) freeReplyObject(reply);
        return;
    }

    printf("%s\nUser '%s' registered successfully.%s\n", GREEN_COLOR, username, RESET_COLOR);

    freeReplyObject(reply);
    memset(password, 0, sizeof(password));
    memset(hashed_password, 0, sizeof(hashed_password));
    memset(salt, 0, sizeof(salt));

    return;
}

void login_user(redisContext *c) {
    char username[32] = {0};
    char password[48] = {0};
    unsigned char stored_salt[16];
    unsigned char stored_hashed_password[32];
    unsigned char stored_combined[sizeof(stored_hashed_password) + sizeof(stored_salt)];
    char base64_combined[EVP_ENCODE_LENGTH(sizeof(stored_combined))];
    unsigned char hashed_password[32];

    // username validation
    printf("Enter username: ");
    if (!input_validation(username, "Username", sizeof(username))) {
        return;
    }

    // username check in db
    redisReply *reply = redisCommand(c, "HEXISTS users %s", username);
    if (!reply) {
        printf("Error: Redis command failed.\n");
        return;
    }
    if (reply->integer == 0) {
        printf("%s\nUser '%s' does not exist.%s\n", RED_COLOR, username, RESET_COLOR);
        freeReplyObject(reply);
        return;
    }
    freeReplyObject(reply);

    // password validation
    printf("Enter password: ");
    if (!input_validation(password, "Password", sizeof(password))) {
        return;
    }

    // retrieve stored hash and salt from db
    reply = redisCommand(c, "HGET users %s", username);
    if (!reply || reply->type != REDIS_REPLY_STRING) {
        printf("%s\nError: Failed to retrieve user data from Redis.%s\n", RED_COLOR, RESET_COLOR);
        if (reply) freeReplyObject(reply);
        return;
    }

    // base64 decode the stored combined (password + salt)
    int decoded_length = EVP_DecodeBlock(stored_combined, (unsigned char*)reply->str, strlen(reply->str));
    if (decoded_length == -1) {
        printf("%s\nError: Base64 decoding failed.%s\n", RED_COLOR, RESET_COLOR);
        freeReplyObject(reply);
        return;
    }
    freeReplyObject(reply);

    // split the decoded combined into the stored password and salt
    memcpy(stored_hashed_password, stored_combined, sizeof(stored_hashed_password));
    memcpy(stored_salt, stored_combined + sizeof(stored_hashed_password), sizeof(stored_salt));

    // hash the input password with the stored salt
    if (argon2id_hash_raw(2, 1 << 16, 1, password, strlen(password), stored_salt, sizeof(stored_salt), hashed_password, sizeof(hashed_password)) != ARGON2_OK) {
        printf("%s\nError: Failed to hash the password.%s\n", RED_COLOR, RESET_COLOR);
        return;
    }

    // compare the hashed passwords
    if (memcmp(hashed_password, stored_hashed_password, sizeof(stored_hashed_password)) == 0) {
        printf("%s\nYou have successfully logged in as %s.%s\n", GREEN_COLOR, username, RESET_COLOR);
    } else {
        printf("%s\nIncorrect password.%s\n", RED_COLOR, RESET_COLOR);
    }
    
    memset(password, 0, sizeof(password));
    memset(hashed_password, 0, sizeof(hashed_password));
    memset(stored_hashed_password, 0, sizeof(stored_hashed_password));
    memset(stored_salt, 0, sizeof(stored_salt));

    return;
}

// show public accounts
void view_public_events(redisContext *c, const char *username) {
    redisReply *reply = redisCommand(c, "LRANGE calendar:%s 0 -1", username);
    if (reply->type == REDIS_REPLY_ARRAY) {
        printf("Public events for user '%s':\n", username);
        for (size_t i = 0; i < reply->elements; i++) {
            char *event = strdup(reply->element[i]->str);
            char *token = strtok(event, ":");
            char *event_name = token;
            token = strtok(NULL, ":");
            int is_private = atoi(token);
            if (!is_private) {
                printf("- %s\n", event_name);
            }
            free(event);
        }
    } else {
        printf("%sNo events found for user '%s'.\n%s", RED_COLOR, username, RESET_COLOR);
    }
    freeReplyObject(reply);
}

void show_menu() {
    printf("=====================================\n");
    printf("%s        Command Line Calendar        \n%s", GREEN_COLOR, RESET_COLOR);
    printf("=====================================\n");
    printf("\n");
    printf(" 1. Register\n");
    printf(" 2. Login\n");
    printf(" 3. View Public Events\n");
    printf(" 4. Exit\n");
    printf("\n");
    printf("=====================================\n");
    printf("\nEnter your choice: ");
}


void clear() {
    system("clear");
}

void press_enter_to_continue() {
    printf("\nPress Enter to continue...");
    empty_input_buffer();
}

int main() {
    clear();
    redisContext *c = connect_redis();

    char choice;

    do {
        show_menu();

        choice = getchar();
        empty_input_buffer();

        printf("You entered: %c\n", choice);

        switch (choice) {
            case '1':
                clear();
                register_user(c);
                press_enter_to_continue();
                break;

            case '2':
                clear();
                login_user(c);
                press_enter_to_continue();
                break;

            case '3':
                clear();
                //view_public_events(c, username);
                break;

            default:
                ;
        }
        clear();
    } while (choice != '4');

    redisFree(c);
    return 0;
}

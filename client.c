#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#define PORT 2908
#define RESPONSE_TIME 10 

int main(int argc, char const *argv[]) {
    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    printf("Connected to the server. Please enter your username: ");
    char username[300];
    fgets(username, 300, stdin);
    size_t username_len = strlen(username);
    if (username_len > 0 && username[username_len - 1] == '\n') {
        username[username_len - 1] = '\0';
    }
    send(sock, username, strlen(username), 0);

    printf("Waiting for the quiz to start...\n");

    fd_set read_fds;
    struct timeval timeout;
    int question_received = 0;

    while (1) {
    FD_ZERO(&read_fds);
    FD_SET(sock, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    timeout.tv_sec = RESPONSE_TIME;
    timeout.tv_usec = 0;
    //wait for socket or input
    int activity = select(sock + 1, &read_fds, NULL, NULL, &timeout);

    if ((activity < 0) && (errno != EINTR)) {
        printf("Select error.\n");
        break;
    }

    if (activity == 0 && question_received) {
        //no activity within time 
        printf("\nTime's up! Moving to next question.\n");
        const char *timeoutMsg = "timeout";
        send(sock, timeoutMsg, strlen(timeoutMsg), 0);
        question_received = 0; //reset next question
        continue; //next iteration 
    }

    if (FD_ISSET(sock, &read_fds)) {
        valread = read(sock, buffer, 1024);
        if (valread <= 0) {
            printf("Server disconnected or error occurred.\n");
            break;
        }
        buffer[valread] = '\0';
        printf("%s", buffer);
        //check high score
        if (strstr(buffer, "Highest score is")) {
            break; //exit the loop and close the socket highest score mesg
        }

        if (strstr(buffer, "finished with score:")) {
            question_received = 0; //Mend of quiz
            continue; //continue to listen for highest score message
        }

        question_received = 1; //question received
    } else if (FD_ISSET(STDIN_FILENO, &read_fds) && question_received) {
        //check activity
        char input[256] = {0};
        if (fgets(input, sizeof(input), stdin) != NULL) {
            size_t len = strlen(input);
    if (len > 0 && input[len - 1] == '\n') {
    input[len - 1] = '\0'; 
    }
    send(sock, input, strlen(input), 0);
    question_received = 0; //Reset for the next question
    }
    } else if (!question_received) {
    //time s up and no question was received
    printf("\nTime's up! Moving to next question.\n");
    const char *timeoutMsg = "timeout";
    send(sock, timeoutMsg, strlen(timeoutMsg), 0);
    }
    }

    close(sock); 
    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <sqlite3.h>
#include <asm-generic/socket.h>
#include <time.h>
#include <errno.h>
#include <sys/select.h>

#define PORT 2908
#define MAX_CLIENTS 100
#define MAX_QUESTIONS 100
#define MESSAGE_BUFFER_SIZE 2048
#define SCORE_MESSAGE_SIZE 512


typedef struct {
    int socket;
    pthread_t thread;
    int ready;
    int idThread;
    char username[300];
    int score;
    int finished;
} Client;

Client clients[MAX_CLIENTS];
int clientCount = 0;
sem_t clientSem; 
int startFlag = 0;

typedef struct {
    char question[256];
    char answerA[256];
    char answerB[256];
    char answerC[256];
    char correctAnswer[256];
} Question;

Question questions[MAX_QUESTIONS];
int num_questions = 0;

int highestScore;              
char highestScorer[300] = "";      //username highest scorer
pthread_mutex_t scoreMutex;        //mutex for safe thread for score
int finishedClients;
pthread_cond_t all_done_cond = PTHREAD_COND_INITIALIZER;

void *clientHandler(void *arg);
void loadQuestionsFromDB();
int checkAnswer(const char *answer, int questionIndex);

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    loadQuestionsFromDB();
    sem_init(&clientSem, 0, 1); //sem init
    pthread_mutex_init(&scoreMutex, NULL);//mutex init
    //socket setup
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    //set socket options for reuse
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    //bind the socket
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    //listen for connections
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", PORT);

    //registration period and select
    time_t startTime, currentTime;
    time(&startTime);
    int registrationClosed = 0;
    fd_set readfds;
    struct timeval tv;
    int max_sd, activity;

 while (1) {
    time(&currentTime);

    //check registration period
    if (difftime(currentTime, startTime) >= 10 && !registrationClosed) {
        printf("Registration period over. No more clients accepted. Game starting.\n");
        registrationClosed = 1;
        startFlag = 1;

        //notify that the game is starting
        for (int i = 0; i < clientCount; i++) {
            const char *startMsg = "Game started";
            send(clients[i].socket, startMsg, strlen(startMsg), 0);
        }
    }

    if (registrationClosed) {
        //accept new connections and then disconnect them
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket >= 0) {
            const char *lateMsg = "Game already started\n";
            send(new_socket, lateMsg, strlen(lateMsg), 0);
            close(new_socket);
            printf("Rejected connection after game start. Socket closed.\n");
        }
    } else {
        //accept valid
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        //Set up timeout for select
        tv.tv_sec = 0;
        tv.tv_usec = 100000; //10 sec

        //wait for activity on the server socket
        activity = select(max_sd + 1, &readfds, NULL, NULL, &tv);

        if ((activity < 0) && (errno != EINTR)) {
            printf("Select error\n");
        }

        //check if something happened on the server socket, if true it is a connection
        if (FD_ISSET(server_fd, &readfds)) {
            new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
            if (new_socket < 0) {
                perror("Accept failed");
                continue;
            }

            //add new client
            sem_wait(&clientSem); //lock sem
            clients[clientCount].socket = new_socket;
            clients[clientCount].idThread = new_socket;
            clients[clientCount].ready = 0;
            if (pthread_create(&clients[clientCount].thread, NULL, clientHandler, &clients[clientCount]) != 0) {
                perror("Failed to create thread");
            }
            clientCount++;
            sem_post(&clientSem); //release sem
        }
    }

}   //cleanup
    sem_destroy(&clientSem);
    pthread_mutex_destroy(&scoreMutex);
    close(server_fd);
    return 0;
}
void loadQuestionsFromDB() {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    if (sqlite3_open("questions.db", &db) != SQLITE_OK) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return;
    }

    const char *sql = "SELECT Question, AnswerA, AnswerB, AnswerC, CorrectAnswer FROM Questions;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        strncpy(questions[num_questions].question, (const char*)sqlite3_column_text(stmt, 0), 255);
        strncpy(questions[num_questions].answerA, (const char*)sqlite3_column_text(stmt, 1), 255);
        strncpy(questions[num_questions].answerB, (const char*)sqlite3_column_text(stmt, 2), 255);
        strncpy(questions[num_questions].answerC, (const char*)sqlite3_column_text(stmt, 3), 255);
        strncpy(questions[num_questions].correctAnswer, (const char*)sqlite3_column_text(stmt, 4), 255);
        num_questions++;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    }

int checkAnswer(const char *answer, int questionIndex) {
    if (questionIndex < 0 || questionIndex >= num_questions) {
    return 0;
    }
    return strcmp(questions[questionIndex].correctAnswer, answer) == 0;
}

void *clientHandler(void *arg) {
    Client *client = (Client *)arg;
    char buffer[1024] = {0};
    int score = 0;

    //receive username from client
    int bytes_read = recv(client->socket, client->username, sizeof(client->username) - 1, 0);
    if (bytes_read <= 0) {
        printf("Error receiving username or client disconnected.\n");
        close(client->socket);
        return NULL;
    }
    client->username[bytes_read] = '\0'; //null
    printf("Username received: %s\n", client->username);

    //Wait for quiz start
    while (!startFlag) {
        sleep(0.1);
    }

    //send questions to the client and receive answers
    for (int i = 0; i < num_questions; i++) {
        char message[1024];
        snprintf(message, sizeof(message), "%s\n%s\n%s\n%s\n", 
                 questions[i].question, questions[i].answerA, questions[i].answerB, questions[i].answerC);
        send(client->socket, message, strlen(message), 0);

        //receive response
        bytes_read = recv(client->socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read <= 0) {
            score = -200;
            client->finished = 1;
            printf("Client %s disconnected during the quiz.\n", client->username);
        break;
        }
            buffer[bytes_read] = '\0'; //null

    if (checkAnswer(buffer, i)) {
        score++; //increment score
    }
}
     //update score 
    if (client->finished) {
        score = -200;
    }

    //after all questions are sent, send the client's score
    char scoreMessage[256];
    snprintf(scoreMessage, sizeof(scoreMessage), "You finished with score: %d\n", score);
    send(client->socket, scoreMessage, strlen(scoreMessage), 0);

    //update the highest score
    pthread_mutex_lock(&scoreMutex);
    if (score > highestScore) {
        highestScore = score;
        strncpy(highestScorer, client->username, sizeof(highestScorer) - 1);
    }
    finishedClients++; //increment
    int allFinished = (finishedClients == clientCount);

    if (allFinished) {
        //send the highest score to clients
        char highestScoreMessage[256];
        snprintf(highestScoreMessage, sizeof(highestScoreMessage), 
                "Highest score is %d by %s\n", highestScore, highestScorer);
        
        for (int i = 0; i < clientCount; i++) {
            send(clients[i].socket, highestScoreMessage, strlen(highestScoreMessage), 0);
        }
    }
    else {
        //Wait for clients
    while (finishedClients < clientCount) {
        pthread_cond_wait(&all_done_cond, &scoreMutex);
    }
    
    //recheck if the current client has the highest score 
    if (score > highestScore) {
        highestScore = score;
        strncpy(highestScorer, client->username, sizeof(highestScorer) - 1);
    }
    }
    pthread_mutex_unlock(&scoreMutex);
    printf("scor maxim:%d", highestScore);
    //unlock, send the highest score to client
    char highestScoreMessage[256];
    snprintf(highestScoreMessage, sizeof(highestScoreMessage), 
            "Highest score is %d by %s\n", highestScore, highestScorer);
    send(client->socket, highestScoreMessage, strlen(highestScoreMessage), 0);

    close(client->socket); //Close the client socket
    return NULL;
}
#include <stdio.h>
#include <sqlite3.h>
#include <string.h>
#include <stdlib.h>

//1- s-a executat comanda sql, 0 -fail
int executeSQL(sqlite3* db, const char* sql_stmt) {
    char* errMsg;
    int rc = sqlite3_exec(db, sql_stmt, 0, 0, &errMsg);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
        return 0;
    }
    
    return 1;
}

int main() {
    sqlite3 *db;
    int rc;

    //deschiderea conexiunii
    rc = sqlite3_open("questions.db", &db);

    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return 1;
    } else {
        fprintf(stdout, "Database opened successfully\n");
    }
    //stergem tabelul 
    const char *delete_data_stmt = "DELETE FROM Questions;";
     if (!executeSQL(db, delete_data_stmt)) {
        sqlite3_close(db);
        return 1;
    }

    //crearea tabelului
    const char *create_table_stmt = "CREATE TABLE IF NOT EXISTS Questions ("
                                    "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
                                    "Question TEXT,"
                                    "AnswerA TEXT,"
                                    "AnswerB TEXT,"
                                    "AnswerC TEXT,"
                                    "CorrectAnswer TEXT"
                                    ");";

    if (!executeSQL(db, create_table_stmt)) {
        sqlite3_close(db);
        return 1;
    }

    //intrebarile si raspunsurile
    const char *questions[] = {
        "Care este cel mai mare continent din lume după suprafață?",
        "a) Africa",
        "b) Asia",
        "c) Europa",
        "b",
        "Care este capitala Braziliei?",
        "a) Rio de Janeiro",
        "b) Brasilia",
        "c) Sao Paulo",
        "b",
        "Care este cel mai mic stat din lume după suprafață?",
        "a) Monaco",
        "b) Vatican",
        "c) San Marino",
        "b",
        "Care este cel mai adânc lac din lume?",
        "a) Lacul Baikal",
        "b) Lacul Victoria",
        "c) Lacul Tanganyika",
        "a",
        "Care este cel mai înalt munte din lume?",
        "a) Everest",
        "b) K2",
        "c) Kilimanjaro",
        "a",
        "Care este cel mai lung râu din lume?",
        "a) Nil",
        "b) Amazon",
        "c) Yangtze",
        "a",
        "Care este cel mai adânc ocean din lume?",
        "a) Oceanul Arctic",
        "b) Oceanul Indian",
        "c) Oceanul Pacific",
        "c",
        "Care este capitala Japoniei?",
        "a) Osaka",
        "b) Tokyo",
        "c) Kyoto",
        "b",
        "Care este cel mai înalt vârf montan din lume?",
        "a) Mont Blanc",
        "b) K2",
        "c) Mount Everest",
        "c"
    };

    //inseram in database
    //iteram prin fiecare element pana la ultimul
    for (int i = 0; i < sizeof(questions) / sizeof(questions[0]); i += 5) {
        //check daca e corecta structura
        if (i + 4 >= sizeof(questions) / sizeof(questions[0])) {
            fprintf(stderr, "Invalid question format at index %d\n", i);
            sqlite3_close(db);
            return 1;
        }

        char sql_stmt[512];
        const char* question = questions[i];
        const char* answerA = questions[i + 1];
        const char* answerB = questions[i + 2];
        const char* answerC = questions[i + 3];
        const char* correctAnswer = questions[i + 4]; 
/*
        printf("Question: %s\n", question);
        printf("A: %s\n", answerA);
        printf("B: %s\n", answerB);
        printf("C: %s\n", answerC);
        printf("Correct Answer: %s\n", correctAnswer);
*/
        //adaug in database
        sprintf(sql_stmt, "INSERT INTO Questions (Question, AnswerA, AnswerB, AnswerC, CorrectAnswer) VALUES ('%s', '%s', '%s', '%s', '%s');",
                question, answerA, answerB, answerC, correctAnswer);
        //verificare fail comanda
        if (!executeSQL(db, sql_stmt)) {
            sqlite3_close(db);
            return 1;
        }
    }

    sqlite3_close(db);
    return 0;
}

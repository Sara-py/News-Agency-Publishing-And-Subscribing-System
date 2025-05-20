#ifndef PROGRAM_H
#define PROGRAM_H

#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <semaphore.h>

#define MAX_NEWS 20
#define MAX_LINE 256
#define NEWS_FILE "news_database.txt"
#define CATEGORY_FILE "categories.txt"
#define NUM_READERS 5
#define DEMO_FILE "demo_news.txt"
#define NUM_DEMO_READERS 5
#define NUM_DEMO_WRITERS 2
#define WARN_THRESHOLD 18
#define NUM_CATEGORIES 6

extern const char* news_categories[];

typedef struct {
    int id;
    char category[20];
    char title[MAX_LINE];
    char content[MAX_LINE];
    time_t timestamp;
} News;

typedef struct {
    News news[MAX_NEWS];
    int num_news;
    int start;
    int end;
    pthread_mutex_t lock;
    pthread_mutex_t rw_lock;
    pthread_mutex_t reader_lock;
    pthread_mutex_t writer_lock;
    sem_t free_slots;
    sem_t used_slots;
    int num_readers;
    int is_writing;
    FILE* file;
    FILE* cat_file;
    char file_path[256];
} NewsDB;

typedef struct {
    NewsDB* news_db;
    int thread_id;
} ReaderArgs;

typedef struct {
    NewsDB* news_db;
    int thread_id;
    int is_writer;
} DemoArgs;

void init_news_db(NewsDB* news_db);
void close_news_db(NewsDB* news_db);
void add_news(NewsDB* news_db, const char* category, const char* title, const char* content, int writer_id);
void edit_news(NewsDB* news_db, int news_id);
void show_news_by_category(NewsDB* news_db, const char* category);
void show_all_news(NewsDB* news_db);
void save_news_to_file(NewsDB* news_db, News* news_item);
void load_news_from_file(NewsDB* news_db);
void* news_agency_thread(void* arg);
void* reader_thread(void* arg);
void reload_news_db(NewsDB* news_db);
void run_demo(NewsDB* news_db);
void* subscriber_thread(void* arg);
void* demo_writer_thread(void* arg);
void* demo_reader_thread(void* arg);
void remove_oldest_news(NewsDB* news_db);

#endif

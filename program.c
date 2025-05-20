#include "program.h"
#include <sys/time.h>

// Global flag to signal demo completion
volatile int demo_complete = 0;

// Predefined news categories
const char *news_categories[] = {
    "BREAKING",
    "POLITICS",
    "SPORTS",
    "TECHNOLOGY",
    "WEATHER",
    "ENTERTAINMENT"
};

// Initialize the news database with mutexes, semaphores, and file handles
void init_news_db(NewsDB *news_db){
    news_db->num_news = 0;
    news_db->start=0;
    news_db->end = 0;
    news_db->num_readers= 0;
    news_db->is_writing =0;

    // Initialize synchronization primitives
    pthread_mutex_init(&news_db->lock, NULL);
    pthread_mutex_init(&news_db->rw_lock, NULL);
    pthread_mutex_init(&news_db->reader_lock, NULL);
    pthread_mutex_init(&news_db->writer_lock, NULL);
    sem_init(&news_db->free_slots, 0, MAX_NEWS);
    sem_init(&news_db->used_slots, 0, 0);

    strcpy(news_db->file_path, NEWS_FILE);

    news_db->file = fopen(NEWS_FILE, "a+");
    if(!news_db->file){
        perror("Error opening news file");
        exit(1);
    }

    // Categories in alag file
    news_db->cat_file= fopen(CATEGORY_FILE, "w");
    if(!news_db->cat_file) {
        perror("Error opening categories file");
        exit(1);
    }
    for(int i=0; i < NUM_CATEGORIES; i++)
        fprintf(news_db->cat_file, "%s\n", news_categories[i]);
    fclose(news_db->cat_file);

    load_news_from_file(news_db);
}

// Clean up the news database, destroying mutexes and closing files
void close_news_db(NewsDB *news_db){
    pthread_mutex_destroy(&news_db->lock);
    pthread_mutex_destroy(&news_db->rw_lock);
    pthread_mutex_destroy(&news_db->reader_lock);
    pthread_mutex_destroy(&news_db->writer_lock);
    sem_destroy(&news_db->free_slots);
    sem_destroy(&news_db->used_slots);
    fclose(news_db->file);
}

// Add a new news item to the circular buffer and file
void add_news(NewsDB *news_db, const char *category, const char *title, const char *content, int writer_id){
    printf("\n[WRITER %d]'s trying to write...\n", writer_id);

    // Ensure only one writer at a time
    pthread_mutex_lock(&news_db->writer_lock);

    // Check if buffer is nearing warning- warnign = 18
    pthread_mutex_lock(&news_db->lock);
    if(news_db->num_news>= WARN_THRESHOLD){
        pthread_mutex_unlock(&news_db->lock);
        remove_oldest_news(news_db);
    }else{
        pthread_mutex_unlock(&news_db->lock);
    }

    // Wait for an available slot in the buffer
    sem_wait(&news_db->free_slots);

    pthread_mutex_lock(&news_db->rw_lock);
    printf("[WRITER %d] Got exclusive access\n", writer_id);
    news_db->is_writing= 1;

    News news_item;
    static int next_id = 1;
    news_item.id= __sync_fetch_and_add(&next_id, 1);

    strncpy(news_item.category, category, 19);
    news_item.category[19] = '\0';
    strncpy(news_item.title, title, MAX_LINE-1);
    news_item.title[MAX_LINE - 1]= '\0';
    strncpy(news_item.content, content, MAX_LINE - 1);
    news_item.content[MAX_LINE-1] ='\0';

    struct timeval tv;
    gettimeofday(&tv, NULL);
    news_item.timestamp = tv.tv_sec;

    char time_str[32];
    struct tm *tm_info = localtime(&news_item.timestamp);
    strftime(time_str, sizeof(time_str), "%a %b %d %H:%M:%S %Y", tm_info);
    snprintf(time_str + strlen(time_str), sizeof(time_str) - strlen(time_str), ".%06ld", tv.tv_usec);
    printf("Timestamp: %s\n", time_str);

    // Add to circular buffer
    pthread_mutex_lock(&news_db->lock);
    news_db->news[news_db->end] = news_item;
    news_db->end = (news_db->end + 1) % MAX_NEWS; // Circular buffer wrap-around
    if(news_db->num_news < MAX_NEWS)
        news_db->num_news ++;

    save_news_to_file(news_db, &news_item);

    printf("\n[WRITER %d] News added!\n", writer_id);
    printf("ID: %d\n", news_item.id);
    printf("Category: %s\n", news_item.category);
    printf("Title: %s\n", news_item.title);
    printf("Timestamp: %s\n", time_str);

    // Release Mutex-lock
    pthread_mutex_unlock(&news_db->lock);
    pthread_mutex_unlock(&news_db->rw_lock);
    printf("[WRITER %d] Released access\n", writer_id);
    news_db->is_writing =0;
    pthread_mutex_unlock(&news_db->writer_lock);

    sem_post(&news_db->used_slots);
}

// Remove the oldest news item from the buffer and update the file
void remove_oldest_news(NewsDB *news_db) {
    pthread_mutex_lock(&news_db->lock);

    if(news_db->num_news > 0){
        // Identify oldest news item
        int index = news_db->start;
        printf("\n[SYSTEM] Removing oldest news to make space:\n");
        printf("ID: %d\n", news_db->news[index].id);
        printf("Category: %s\n", news_db->news[index].category);
        printf("Title: %s\n", news_db->news[index].title);

        // Update circular buffer
        news_db->start= (news_db->start + 1) % MAX_NEWS; // Move start pointer
        news_db->num_news--;
        sem_post(&news_db->free_slots);

        // Update news file to reflect sabse agay - curent buffer
        FILE *temp = fopen("temp.txt", "w");
        if(!temp) {
            perror("Error creating temp file");
            pthread_mutex_unlock(&news_db->lock);
            return;
        }

        for(int i = 0; i<news_db->num_news; i++){
            int current_index = (news_db->start + i) % MAX_NEWS;
            char time_str[32];
            struct tm *tm_info= localtime(&news_db->news[current_index].timestamp);
            strftime(time_str, sizeof(time_str), "%a %b %d %H:%M:%S %Y", tm_info);

            fprintf(temp, "%d|%s|%s|%s|%s\n",
                    news_db->news[current_index].id,
                    news_db->news[current_index].category,
                    news_db->news[current_index].title,
                    news_db->news[current_index].content,
                    time_str);
        }

        fclose(temp);
        fclose(news_db->file);

        remove(NEWS_FILE);
        rename("temp.txt", NEWS_FILE);

        news_db->file = fopen(NEWS_FILE, "a+");
        if(!news_db->file){
            perror("Error reopening news file");
            pthread_mutex_unlock(&news_db->lock);
            exit(1);
        }

        printf("[SYSTEM] News removed and file updated. Current buffer size: %d/%d\n", news_db->num_news, MAX_NEWS);
    }

    pthread_mutex_unlock(&news_db->lock);
}

// Edit an existing news item by ID
void edit_news(NewsDB *news_db, int news_id){
    printf("\n[WRITER] Editing news...\n");

    pthread_mutex_lock(&news_db->writer_lock);
    pthread_mutex_lock(&news_db->rw_lock);
    printf("[WRITER] Got exclusive access\n");
    news_db->is_writing = 1;

    int is_found= 0, index = -1;
    pthread_mutex_lock(&news_db->lock);
    for(int i=0; i < news_db->num_news; i++){
        if(news_db->news[i].id == news_id){
            is_found = 1;
            index= i;
            break;
        }
    }

    if(!is_found){
        printf("[WRITER] News not found\n");
        pthread_mutex_unlock(&news_db->lock);
        pthread_mutex_unlock(&news_db->rw_lock);
        news_db->is_writing= 0;
        pthread_mutex_unlock(&news_db->writer_lock);
        return;
    }

    // GEO NEWS - BREAKING NEWS!! --- just a display
    printf("\nCurrent news:\n");
    printf("ID: %d\n", news_db->news[index].id);
    printf("Category: %s\n", news_db->news[index].category);
    printf("Title: %s\n", news_db->news[index].title);
    printf("Content: %s\n", news_db->news[index].content);
    char new_title[MAX_LINE];

    char new_content[MAX_LINE];
    printf("\nNew title (Enter to keep): ");
    fgets(new_title, MAX_LINE, stdin);
    new_title[strcspn(new_title, "\n")] = 0;

    printf("New content (Enter to keep): ");
    fgets(new_content, MAX_LINE, stdin);
    new_content[strcspn(new_content, "\n")]= 0;

    printf("\nCategories:\n");
    for(int i = 0; i< NUM_CATEGORIES; i++)
        printf("%d. %s\n", i + 1, news_categories[i]);
    printf("New category (1-%d, 0 to keep): ", NUM_CATEGORIES);
    int category;
    scanf("%d", &category);
    getchar();

    if(strlen(new_title) > 0)
        strncpy(news_db->news[index].title, new_title, MAX_LINE - 1);
    if(strlen(new_content)>0)
        strncpy(news_db->news[index].content, new_content, MAX_LINE-1);
    if(category > 0 && category<= NUM_CATEGORIES)
        strncpy(news_db->news[index].category, news_categories[category - 1], 19);

    FILE *temp= fopen("temp.txt", "w");
    if(!temp){
        perror("Error creating temp file");
        pthread_mutex_unlock(&news_db->lock);
        pthread_mutex_unlock(&news_db->rw_lock);
        news_db->is_writing = 0;
        pthread_mutex_unlock(&news_db->writer_lock);
        return;
    }

    for(int i=0; i < news_db->num_news; i++) {
        char time_str[32];
        struct tm *tm_info = localtime(&news_db->news[i].timestamp);
        strftime(time_str, sizeof(time_str), "%a %b %d %H:%M:%S %Y", tm_info);

        fprintf(temp, "%d|%s|%s|%s|%s\n",
                news_db->news[i].id,
                news_db->news[i].category,
                news_db->news[i].title,
                news_db->news[i].content,
                time_str);
    }

    fclose(temp);
    fclose(news_db->file);

    remove(NEWS_FILE);
    rename("temp.txt", NEWS_FILE);

    news_db->file= fopen(NEWS_FILE, "a+");
    if(!news_db->file) {
        perror("Error reopening news file");
        pthread_mutex_unlock(&news_db->lock);
        pthread_mutex_unlock(&news_db->rw_lock);
        news_db->is_writing=0;
        pthread_mutex_unlock(&news_db->writer_lock);
        return;
    }

    printf("\n[WRITER] News updated!\n");

    pthread_mutex_unlock(&news_db->lock);
    pthread_mutex_unlock(&news_db->rw_lock);
    printf("[WRITER] Released access\n");
    news_db->is_writing = 0;
    pthread_mutex_unlock(&news_db->writer_lock);
}

// Display news items for a specific category
void show_news_by_category(NewsDB *news_db, const char *category){
    printf("\n[READER] Reading news by category...\n");

    sem_wait(&news_db->used_slots);

    // Bohot log parg sakte with reader-writer lock
    pthread_mutex_lock(&news_db->reader_lock);
    news_db->num_readers++;
    if(news_db->num_readers == 1)
        pthread_mutex_lock(&news_db->rw_lock);
    pthread_mutex_unlock(&news_db->reader_lock);

    pthread_mutex_lock(&news_db->lock);

    int is_found = 0;
    for(int i=0; i< news_db->num_news; i++){
        int index = (news_db->start + i) % MAX_NEWS;
        if(strcmp(news_db->news[index].category, category) == 0){
            is_found= 1;
            char time_str[32];
            struct tm *tm_info = localtime(&news_db->news[index].timestamp);
            strftime(time_str, sizeof(time_str), "%a %b %d %H:%M:%S %Y", tm_info);

            printf("\nID: %d\n", news_db->news[index].id);
            printf("Time: %s\n", time_str);
            printf("Title: %s\n", news_db->news[index].title);
            printf("Content: %s\n", news_db->news[index].content);
            printf("-------------------\n");
        }
    }
    if(!is_found)
        printf("No news found in this category\n");

    pthread_mutex_unlock(&news_db->lock);

    pthread_mutex_lock(&news_db->reader_lock);
    news_db->num_readers --;
    if(news_db->num_readers== 0)
        pthread_mutex_unlock(&news_db->rw_lock);
    pthread_mutex_unlock(&news_db->reader_lock);

    sem_post(&news_db->used_slots);
}

// Display all news items in the buffer
void show_all_news(NewsDB *news_db) {
    pthread_mutex_lock(&news_db->reader_lock);
    news_db->num_readers++;
    if(news_db->num_readers==1)
        pthread_mutex_lock(&news_db->rw_lock);
    pthread_mutex_unlock(&news_db->reader_lock);

    printf("\n=== All News ===\n");

    pthread_mutex_lock(&news_db->lock);
    for(int i = 0; i<news_db->num_news; i++){
        int index= (news_db->start + i) % MAX_NEWS;
        char time_str[32];
        struct tm *tm_info= localtime(&news_db->news[index].timestamp);
        strftime(time_str, sizeof(time_str), "%a %b %d %H:%M:%S %Y", tm_info);

        printf("\nID: %d\n", news_db->news[index].id);
        printf("Category: %s\n", news_db->news[index].category);
        printf("Time: %s\n", time_str);
        printf("Title: %s\n", news_db->news[index].title);
        printf("Content: %s\n", news_db->news[index].content);
        printf("-------------------\n");
    }
    if(news_db->num_news == 0)
        printf("No news available\n");
    pthread_mutex_unlock(&news_db->lock);

    pthread_mutex_lock(&news_db->reader_lock);
    news_db->num_readers--;
    if(news_db->num_readers == 0)
        pthread_mutex_unlock(&news_db->rw_lock);
    pthread_mutex_unlock(&news_db->reader_lock);
}

// Save a single news item to the file
void save_news_to_file(NewsDB *news_db, News *news_item){
    char time_str[32];
    struct tm *tm_info = localtime(&news_item->timestamp);
    strftime(time_str, sizeof(time_str), "%a %b %d %H:%M:%S %Y", tm_info);

    fprintf(news_db->file, "%d|%s|%s|%s|%s\n",
            news_item->id,
            news_item->category,
            news_item->title,
            news_item->content,
            time_str);
    fflush(news_db->file);
}

// Load news items from the file into the buffer
void load_news_from_file(NewsDB *news_db){
    rewind(news_db->file);
    char line[1024];

    while(fgets(line, sizeof(line), news_db->file)){
        News news_item;
        char time_str[32];

        sscanf(line, "%d|%[^|]|%[^|]|%[^|]|%[^\n]",
               &news_item.id,
               news_item.category,
               news_item.title,
               news_item.content,
               time_str);

        struct tm tm = {0};
        char *result= strptime(time_str, "%a %b %d %H:%M:%S %Y", &tm);
        if(!result){
            printf("Error parsing time: %s\n", time_str);
            continue;
        }
        news_item.timestamp= mktime(&tm);

        news_db->news[news_db->num_news++] = news_item;
    }
}

// News agency thread for manual news addition/editing
void *news_agency_thread(void *arg){
    NewsDB *news_db = (NewsDB *)arg;
    int choice;

    while(1){
        printf("\n=== News Agency Menu ===\n");
        printf("1. Add news\n");
        printf("2. Edit news\n");
        printf("3. Back\n");
        printf("Choice: ");

        scanf("%d", &choice);
        getchar();

        switch(choice){
        case 1:
            printf("\nCategories:\n");
            for(int i=0; i < NUM_CATEGORIES; i++)
                printf("%d. %s\n", i+1, news_categories[i]);

            int category;
            printf("Category (1-%d): ", NUM_CATEGORIES);
            scanf("%d", &category);
            getchar();

            if(category<1 || category > NUM_CATEGORIES){
                printf("Invalid category\n");
                continue;
            }

            char title[MAX_LINE];
            printf("Title: ");
            fgets(title, MAX_LINE, stdin);
            title[strcspn(title, "\n")] = 0;

            char content[MAX_LINE];
            printf("Content: ");
            fgets(content, MAX_LINE, stdin);
            content[strcspn(content, "\n")]=0;

            add_news(news_db, news_categories[category-1], title, content, 0);
            break;

        case 2:
            printf("\nNews ID to edit: ");
            int news_id;
            scanf("%d", &news_id);
            getchar();
            edit_news(news_db, news_id);
            break;

        case 3:
            return NULL;

        default:
            printf("Invalid choice\n");
        }
    }
}

// Reader thread for viewing news
void *reader_thread(void *arg){
    ReaderArgs *args = (ReaderArgs *)arg;
    NewsDB *news_db= args->news_db;
    int thread_id = args->thread_id;

    printf("\n[READER %d] Starting\n", thread_id);

    while(1){
        printf("\n[READER %d] Menu:\n", thread_id);
        printf("1. View by category\n");
        printf("2. View all\n");
        printf("3. Refresh\n");
        printf("4. Exit\n");
        printf("Choice: ");

        int choice;
        scanf("%d", &choice);
        getchar();

        if(choice==4)
            break;

        switch(choice){
        case 1:
            printf("\n[READER %d] Categories:\n", thread_id);
            for(int i=0; i<NUM_CATEGORIES; i++)
                printf("%d. %s\n", i + 1, news_categories[i]);

            int category;
            printf("Category (1-%d): ", NUM_CATEGORIES);
            scanf("%d", &category);
            getchar();

            if(category < 1 || category>NUM_CATEGORIES){
                printf("[READER %d] Invalid category\n", thread_id);
                continue;
            }

            show_news_by_category(news_db, news_categories[category-1]);
            break;

        case 2:
            show_all_news(news_db);
            break;

        case 3:
            reload_news_db(news_db);
            break;

        default:
            printf("[READER %d] Invalid choice\n", thread_id);
        }
    }

    printf("[READER %d] Exiting\n", thread_id);
    return NULL;
}

// Reload news from file, ensuring no writer is active
void reload_news_db(NewsDB *news_db){
    printf("\n[READER] Refreshing...\n");

    pthread_mutex_lock(&news_db->writer_lock);
    if(news_db->is_writing){
        printf("[READER] Cannot refresh - Writer active\n");
        pthread_mutex_unlock(&news_db->writer_lock);
        return;
    }
    pthread_mutex_unlock(&news_db->writer_lock);

    pthread_mutex_lock(&news_db->lock);
    printf("[READER] Refreshing database...\n");

    if(news_db->file)
        fclose(news_db->file);

    news_db->file = fopen(news_db->file_path, "r");
    if(!news_db->file){
        perror("Error reopening news file");
        pthread_mutex_unlock(&news_db->lock);
        return;
    }

    news_db->num_news = 0;
    load_news_from_file(news_db);

    fclose(news_db->file);
    news_db->file= fopen(news_db->file_path, "a+");

    pthread_mutex_unlock(&news_db->lock);
    printf("[READER] Refresh complete!\n");
}

// Run a demo with multiple readers and writers
void run_demo(NewsDB *news_db){
    printf("\n=== Starting Enhanced Demo ===\n");
    printf("Creating %d readers and %d writers...\n", NUM_DEMO_READERS, NUM_DEMO_WRITERS);

    // Create demo file with sample news
    FILE *demo_file = fopen(DEMO_FILE, "w");
    if(!demo_file){
        printf("Error creating demo file\n");
        return;
    }

    const char *demo_news[] = {
        "BREAKING: Major Earthquake Strikes Tokyo",
        "BREAKING: Global Markets Nosedive Amid Recession Fears",
        "BREAKING: Cyberattack Hits Major International Bank",
        "POLITICS: U.S. Congress Passes Historic Climate Bill",
        "POLITICS: Coalition Government Formed in Pakistan",
        "POLITICS: UK Prime Minister Survives No-Confidence Vote",
        "SPORTS: Pakistan Stuns India in Last-Ball Thriller",
        "SPORTS: Serena Williams Retires After Iconic Career",
        "SPORTS: Olympics 2028 Adds Esports to the Lineup",
        "TECHNOLOGY: Apple Reveals First Foldable iPhone Prototype",
        "TECHNOLOGY: OpenAI Releases GPT-5 With Multimodal Capabilities",
        "TECHNOLOGY: Google Introduces Quantum AI Chip",
        "WEATHER: South Asia Faces Intense Heatwave",
        "WEATHER: Hurricane Alicia Nears U.S. Coastline",
        "WEATHER: Dubai Hit by Record Rainfall and Flash Floods",
        "ENTERTAINMENT: Taylor Swift Breaks Spotify Streaming Records",
        "ENTERTAINMENT: Deepika Padukone to Direct First Feature Film",
        "ENTERTAINMENT: House of the Dragon Renewed for Season 2",
        "ENTERTAINMENT: Cannes Film Festival Opens with Global Spotlight",
        "ENTERTAINMENT: Marvel Confirms Avengers: Secret Wars for 2027",
        "BREAKING: Major Power Outage Hits New York City",
        "POLITICS: UN Votes on Global Digital Privacy Agreement"
    };

    const char *demo_content[] = {
        "A 7.3 magnitude quake disrupts transport and power across the city.",
        "Dow, FTSE, and Nikkei all fall sharply as investors react to new economic data.",
        "Hackers breach security, compromising millions of accounts worldwide.",
        "The legislation commits $500 billion to clean energy and emission cuts.",
        "Opposition parties join hands to replace the incumbent administration.",
        "Despite mounting criticism, the PM secures enough support to remain in office.",
        "A six off the final delivery seals a dramatic T20 win at the Asia Cup.",
        "The 23-time Grand Slam champion bids farewell after final match in New York.",
        "Competitive video gaming officially joins the next Summer Games.",
        "The sleek device folds seamlessly and is expected to launch by 2026.",
        "The model can now process video, audio, and text in real-time conversations.",
        "The chip claims to outperform classical supercomputers in speed and efficiency.",
        "Extreme temperatures above 45°C cause health warnings across the region.",
        "The Category 4 storm is projected to hit Florida by early morning.",
        "Unseasonal weather causes travel delays and infrastructure damage.",
        "Her latest album crosses 1 billion plays in just 7 days.",
        "The actress steps into filmmaking with a psychological thriller project.",
        "HBO confirms the return after massive global success of Season 1.",
        "Stars from around the world arrive for the 78th edition of the prestigious event.",
        "Fans celebrate as Marvel reveals its Phase 6 lineup at Comic-Con.",
        "A grid failure leaves over 2 million residents without electricity during peak hours.",
        "Nations agree on a new framework to protect user data across borders."
    };

    for(int i=0; i < 22; i++)
        fprintf(demo_file, "%s|%s\n", demo_news[i], demo_content[i]);
    fclose(demo_file);

    printf("\n=== Demo Phase 1: Initial Writing ===\n");
    printf("Writers will add news until buffer is full (20 items)\n");
    printf("Readers will read concurrently\n");
    printf("System will show warning at 18 items\n");
    printf("When buffer is full, oldest news will be removed automatically\n\n");

    // Create reader and writer threads
    pthread_t r_threads[NUM_DEMO_READERS];
    pthread_t w_threads[NUM_DEMO_WRITERS];
    DemoArgs r_args[NUM_DEMO_READERS];
    DemoArgs w_args[NUM_DEMO_WRITERS];

    time_t start_time = time(NULL);
    const int TIMEOUT_SECONDS=30;

    for(int i=0; i<NUM_DEMO_READERS; i++){
        r_args[i].news_db = news_db;
        r_args[i].thread_id= i + 1;
        r_args[i].is_writer = 0;
        pthread_create(&r_threads[i], NULL, demo_reader_thread, &r_args[i]);
    }

    for(int i=0; i < NUM_DEMO_WRITERS; i++){
        w_args[i].news_db= news_db;
        w_args[i].thread_id = i+1;
        w_args[i].is_writer= 1;
        pthread_create(&w_threads[i], NULL, demo_writer_thread, &w_args[i]);
    }

    for(int i=0; i<NUM_DEMO_READERS; i++){
        while(time(NULL) - start_time < TIMEOUT_SECONDS){
            if(demo_complete)
                break;
            sleep(1);
        }
        pthread_cancel(r_threads[i]);
        pthread_join(r_threads[i], NULL);
    }

    for(int i=0; i<NUM_DEMO_WRITERS; i++){
        while(time(NULL)-start_time<TIMEOUT_SECONDS){
            if(demo_complete)
                break;
            sleep(1);
        }
        pthread_cancel(w_threads[i]);
        pthread_join(w_threads[i], NULL);
    }

    demo_complete = 0;
    // demo file dlete
    remove(DEMO_FILE);
    printf("\n=== Demonstration Complete ===\n");
}

// Subscriber thread for viewing and managing news
void *subscriber_thread(void *arg){
    NewsDB *news_db = (NewsDB *)arg;
    int choice;

    while(1){
        printf("\n=== Subscriber Menu ===\n");
        printf("1. View by category\n");
        printf("2. Show all (with refresh)\n");
        printf("3. Remove oldest news to free space for publisher\n");
        printf("4. Back\n");
        printf("Choice: ");

        scanf("%d", &choice);
        getchar();

        switch(choice){
        case 1:
            printf("\nCategories:\n");
            for(int i=0; i<NUM_CATEGORIES; i++)
                printf("%d. %s\n", i + 1, news_categories[i]);
            printf("Category (1-%d): ", NUM_CATEGORIES);
            int category;
            scanf("%d", &category);
            getchar();
            if(category >= 1 && category <= NUM_CATEGORIES)
                show_news_by_category(news_db, news_categories[category - 1]);
            else
                printf("Invalid category\n");
            break;

        case 2:
            reload_news_db(news_db);
            show_all_news(news_db);
            break;

        case 3:
            pthread_mutex_lock(&news_db->lock);
            if(news_db->num_news == MAX_NEWS){
                int index = news_db->start;
                char time_str[32];
                struct tm *tm_info = localtime(&news_db->news[index].timestamp);
                strftime(time_str, sizeof(time_str), "%a %b %d %H:%M:%S %Y", tm_info);
                printf("\n[SUBSCRIBER] Buffer full! Removing oldest news:\n");
                printf("ID: %d\nCategory: %s\nTime: %s\nTitle: %s\nContent: %s\n",
                       news_db->news[index].id,
                       news_db->news[index].category,
                       time_str,
                       news_db->news[index].title,
                       news_db->news[index].content);

                news_db->start = (news_db->start + 1) % MAX_NEWS;
                news_db->num_news--;
                sem_post(&news_db->free_slots);
                printf("[SUBSCRIBER] Oldest news removed. Publisher can now add news.\n");
            }else{
                printf("[SUBSCRIBER] Buffer is not full. No need to remove news.\n");
            }
            pthread_mutex_unlock(&news_db->lock);
            break;

        case 4:
            return NULL;

        default:
            printf("Invalid choice\n");
        }
    }
}

// Demo writer thread for adding news from demo file
void *demo_writer_thread(void *arg){
    DemoArgs *args = (DemoArgs *)arg;
    NewsDB *news_db= args->news_db;
    int thread_id = args->thread_id;
    int loops= 0;
    const int max_loops = 11;

    FILE *demo_file= fopen(DEMO_FILE, "r");
    if(!demo_file){
        printf("[Writer %d] Error: Cannot open demo file '%s'\n", thread_id, DEMO_FILE);
        return NULL;
    }

    char line[MAX_LINE *2];
    int start_line = (thread_id - 1)*max_loops;
    for(int i=0; i<start_line; i++){
        if(!fgets(line, sizeof(line), demo_file)){
            rewind(demo_file);
            break;
        }
    }

    while(loops< max_loops && !demo_complete){
        if(!fgets(line, sizeof(line), demo_file)){
            printf("[Writer %d] Reached end of demo file, restarting\n", thread_id);
            rewind(demo_file);
            continue;
        }
        char *title = strtok(line, "|");
        char *content= strtok(NULL, "\n");
        if(!title){
            printf("[Writer %d] Warning: Malformed line in demo file, skipping\n", thread_id);
            continue;
        }
        if(!content){
            printf("[Writer %d] Warning: No content found, using default\n", thread_id);
            content = "Demo content";
        }

        char category[20];
        strncpy(category, title, 19);
        category[19] = '\0';
        char *colon = strchr(category, ':');
        if(colon){
            *colon= '\0';
            int valid_category= 0;
            for(int i=0; i < NUM_CATEGORIES; i++){
                if(strcmp(category, news_categories[i]) == 0){
                    valid_category = 1;
                    break;
                }
            }
            if(!valid_category){
                printf("[Writer %d] Warning: Invalid category '%s', using default\n", thread_id, category);
                strncpy(category, news_categories[loops % NUM_CATEGORIES], 19);
            }
            title= title + (colon - category) + 2;
            while(*title == ' ') title++;
        }else{
            printf("[Writer %d] Warning: No category in title, using default\n", thread_id);
            strncpy(category, news_categories[loops % NUM_CATEGORIES], 19);
        }
        sem_wait(&news_db->free_slots);
        add_news(news_db, category, title, content, thread_id);
        loops++;

        if(loops>= max_loops){
            demo_complete= 1;
        }

        sleep(2); //writing delay - to make it real  ... DRAMAAA
    }

    fclose(demo_file);
    printf("[Writer %d] Completed %d news items\n", thread_id, loops);
    return NULL;
}

// Demo reader thread for reading news
void *demo_reader_thread(void *arg){
    DemoArgs *args= (DemoArgs *)arg;
    NewsDB *news_db = args->news_db;
    int thread_id= args->thread_id;
    int loops = 0;

    while(loops < 10 && !demo_complete){
        sem_wait(&news_db->used_slots);

        pthread_mutex_lock(&news_db->reader_lock);
        news_db->num_readers++;
        if(news_db->num_readers == 1)
            pthread_mutex_lock(&news_db->rw_lock);
        pthread_mutex_unlock(&news_db->reader_lock);

        pthread_mutex_lock(&news_db->lock);

        if(news_db->num_news > 0){
            int index = (news_db->start + (loops % news_db->num_news)) % MAX_NEWS;

            char time_str[32];
            struct tm *tm_info= localtime(&news_db->news[index].timestamp);
            strftime(time_str, sizeof(time_str), "%a %b %d %H:%M:%S %Y", tm_info);

            printf("\n=== Reader %d Reading ===\n", thread_id);
            printf("ID: %d\n", news_db->news[index].id);
            printf("Category: %s\n", news_db->news[index].category);
            printf("Title: %s\n", news_db->news[index].title);
            printf("Content: %s\n", time_str);
            printf("Time: %s\n", time_str);
            printf("Total items in buffer: %d\n", news_db->num_news);
            printf("-------------------\n");
        }else{
            printf("\n=== Reader %d Reading ===\n", thread_id);
            printf("Buffer empty\n");
        }

        pthread_mutex_unlock(&news_db->lock);

        pthread_mutex_lock(&news_db->reader_lock);
        news_db->num_readers--;
        if(news_db->num_readers == 0)
            pthread_mutex_unlock(&news_db->rw_lock);
        pthread_mutex_unlock(&news_db->reader_lock);

        sem_post(&news_db->free_slots);

        loops++;
        sleep(1);
    }
    return NULL;
}

#include "program.h"

int main() {
    NewsDB db;
    init_news_db(&db);

    int choice;
    while (1) {
        printf("\n=== Main Menu ===\n");
        printf("1. Run demo\n");
        printf("2. News agency\n");
        printf("3. Subscriber\n");
        printf("4. Exit\n");
        printf("Choice: ");
        scanf("%d", &choice);
        getchar();

        switch (choice) {
        case 1:
            run_demo(&db);
            break;
        case 2:
            pthread_t agency_tid;
            pthread_create(&agency_tid, NULL, news_agency_thread, &db);
            pthread_join(agency_tid, NULL);
            break;
        case 3:
            pthread_t subscriber_tid;
            pthread_create(&subscriber_tid, NULL, subscriber_thread, &db);
            pthread_join(subscriber_tid, NULL);
            break;
        case 4:
            close_news_db(&db);
            return 0;
        default:
            printf("Invalid choice\n");
        }
    }
}

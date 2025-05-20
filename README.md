News Agency Publishing and Subscribing System

This is the News Agency Publishing and Subscribing System, where stories break faster than a printing press on overdrive! Haha ... just kidding
This C program is your ticket to a multi-threaded newsroom, letting you publish, edit, and read news with the precision of a seasoned editor. With a circular buffer and file persistence, it handles concurrent readers and writers smoother than a front-page scoop.

What's the Headline?

This project is a thread-safe news management system that stores stories in a circular buffer, backed by a file to keep things persistent. Multiple threads—writers dropping the latest headlines and readers gobbling them up—work in harmony, thanks to mutexes and semaphores. Want to add a breaking story, edit a typo, or browse the sports section? We've got you covered. Plus, a demo mode lets you see the newsroom buzz with multiple threads, like reporters chasing deadlines!

Why You'll Love It

- Thread-Safe Shenanigans: Writers publish while readers browse, all without stepping on each other's toes—synchronized like a well-timed news ticker.
- Circular Buffer Brilliance: Holds up to 20 stories, with older ones gracefully bowing out when the buffer's nearly full (18 stories trigger a heads-up).
- Persistent Pages: News lives in news_database.txt, and categories are listed in categories.txt, so nothing gets lost in the shuffle.
- Interactive Interfaces: Menus for publishers and subscribers make adding, editing, or reading news as easy as flipping through a paper.
- Demo Drama: Watch multiple readers and writers duke it out with sample stories, testing the system's ability to keep up with the news cycle.

Get the Press Rolling:

1. What You Need
- A C compiler like gcc with POSIX thread support (because we’re threading the needle here!).
- A Unix-like environment (Linux, macOS, or WSL on Windows).
- A knack for news and a bit of threading know-how.

2.Setup Steps:
Grab the repo and step into the newsroom:

git clone <repository-url>
cd news-agency-system

Build the project faster than a breaking news alert:

make

Launch the program and start making headlines:

./newsProgram

3. How to Use It
Fire up the program and pick your role from the main menu:
- Run Demo: See the system in action with multiple readers and writers, like a newsroom in full swing.
- News Agency: Publish or edit stories like a pro editor chasing the next big scoop.
- Subscriber: Browse news by category, view all stories, or clear space for new headlines.
- Exit: Shut down the presses and clean up.

The demo mode spins up a demo_news.txt file with juicy sample stories and runs for 30 seconds or until the news cycle wraps up. Your stories are saved in news_database.txt, with categories in categories.txt.

What's in the Newsstand

program.h: The blueprint with structs, constants, and function declarations.
program.c: The heart of the system, handling news management, threading, and demo logic.
main.c: The front door, with the main menu and thread orchestration.
makefile: Builds the project and sweeps away old files like yesterday’s news.

Hot Off the Press

- Choose from six categories: BREAKING, POLITICS, SPORTS, TECHNOLOGY, WEATHER, and ENTERTAINMENT.
- The buffer holds 20 stories, automatically tossing the oldest when it’s time to make room.
- File operations are locked tighter than a newsroom safe, ensuring no data gets scrambled.
- Hit a snag? Check the console for error messages—they’re clearer than a morning edition.

Want to Write the Next Big Story?

Got a headline-worthy idea to improve the system? Fork the repo, tweak the code, and send a pull request. Keep your commit messages sharp and snappy, like a well-crafted lede.

License

This project is under the MIT License. Use it, tweak it, share it—just don’t turn it into tomorrow’s fish-and-chip wrapper!

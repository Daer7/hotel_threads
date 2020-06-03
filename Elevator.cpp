#include <iostream>
#include <vector>
#include <list>
#include <thread>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <experimental/random>
#include <atomic>
#include <ncurses.h>
#include <condition_variable>
#include <memory>

struct Elevator
{
    int current_floor = -1;
    std::vector<int> guests_inside;
    std::mutex mx;
    std::condition_variable cv_out;
    std::condition_variable cv_in;

    WINDOW *elevator_window;
    WINDOW *progress_window;

    void draw_elevator()
    {
        elevator_window = newwin(18, 39, 0, 121);
        progress_window = newwin(3, 39, 18, 121);
        std::lock_guard<std::mutex> writing_lock(mx_writing);
        wattron(this->elevator_window, COLOR_PAIR(ELEVATOR_C));
        box(this->elevator_window, 0, 0);
        wattroff(this->elevator_window, COLOR_PAIR(ELEVATOR_C));
        wattron(this->progress_window, COLOR_PAIR(ELEVATOR_C));
        box(this->progress_window, 0, 0);
        wattroff(this->progress_window, COLOR_PAIR(ELEVATOR_C));
        mvwprintw(this->elevator_window, 2, 16, "ELEVATOR");
        mvwprintw(this->elevator_window, 6, 13, "CURRENT FLOOR");
        //mvwprintw(this->elevator_window, 7, 20, "%1d", this->current_floor);
        mvwprintw(this->elevator_window, 9, 13, "GUESTS INSIDE");
        mvwprintw(this->elevator_window, 12, 17, "STATUS");
        wrefresh(this->elevator_window);
        wrefresh(this->progress_window);
    }

    void go_to_floor(int floor)
    {
        {
            std::lock_guard<std::mutex> writing_lock(mx_writing);
            mvwprintw(this->elevator_window, 7, 20, "M");
            mvwprintw(this->elevator_window, 13, 5, "    ON ITS WAY TO FLOOR %1d     ", floor);
            wrefresh(this->elevator_window);
        }
        fill_progress_bar(this->progress_window, COLOR_PAIR(ELEVATOR_C), 39, std::experimental::randint(1500, 2500));

        {
            std::unique_lock<std::mutex> lock_out(this->mx);
            this->current_floor = floor;
            this->cv_out.notify_all();
        }

        {
            std::lock_guard<std::mutex> writing_lock(mx_writing);
            mvwprintw(this->elevator_window, 7, 20, "%1d", this->current_floor);
            mvwprintw(this->elevator_window, 13, 5, "GUESTS GETTING OFF ON FLOOR %1d", this->current_floor);
            wrefresh(this->elevator_window);
        }
        fill_progress_bar(this->progress_window, COLOR_PAIR(ELEVATOR_C), 39, std::experimental::randint(500, 1000));

        {
            std::unique_lock<std::mutex> lock_in(this->mx);
            this->cv_in.notify_all();
        }

        {
            std::lock_guard<std::mutex> writing_lock(mx_writing);
            mvwprintw(this->elevator_window, 13, 5, "GUESTS GETTING IN ON FLOOR %1d ", this->current_floor);
            wrefresh(this->elevator_window);
        }
        fill_progress_bar(this->progress_window, COLOR_PAIR(ELEVATOR_C), 39, std::experimental::randint(500, 1000));

        std::unique_lock<std::mutex> writing_lock(mx_writing, std::defer_lock);
        std::unique_lock<std::mutex> guests_lock(this->mx, std::defer_lock);
        std::lock(writing_lock, guests_lock);
        mvwprintw(this->elevator_window, 10, 8, "                              ");
        wattron(this->elevator_window, COLOR_PAIR(GUEST_C));
        for (int i = 0; i < this->guests_inside.size(); i++)
        {

            mvwprintw(this->elevator_window, 10, 8 + 3 * i, "%2d ", this->guests_inside[i]);
        }
        wattroff(this->elevator_window, COLOR_PAIR(GUEST_C));
        wrefresh(this->elevator_window);

        this->current_floor = -1; //floor equal to -1 while elevator is moving (changed when mutex is owned)
    }

    void go()
    {
        while (!cancellation_token)
        {
            go_to_floor(0);
            go_to_floor(1);
            go_to_floor(2);
            go_to_floor(1);
        }
    }
};
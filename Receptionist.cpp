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

struct Receptionist
{
    Receptionist(std::vector<Room> &rooms) : rooms(rooms)
    {
        for (Room &room : rooms)
        {
            ready_rooms.push_back(room.id);
        }
    }
    std::vector<Room> &rooms;

    std::vector<int> rooms_to_be_cleaned; // guests push back, cleaners remove
    std::vector<int> ready_rooms;         // cleaners push back, receptioist removes

    std::mutex mx;
    std::condition_variable cv_pick_a_room; // receptionist waits, cleanes notify
    std::condition_variable cv_assign_room; // guests wait, receptionist notifies
    std::condition_variable cv_clean_room;  // guests notify, cleaners wait

    std::atomic<bool> found_a_ready_room{false};
    int number_to_check_in = 0; // receptionist writes, guests read

    int y_size = 5, x_size = 40, y_corner, x_corner;
    WINDOW *receptionist_window;
    WINDOW *progress_window;

    void draw_receptionist_desk()
    {
        this->receptionist_window = newwin(3, 40, 21, 100);
        this->progress_window = newwin(3, 20, 21, 140);
        std::lock_guard<std::mutex> writing_lock(mx_writing);
        wattron(this->receptionist_window, COLOR_PAIR(RECEP_C));
        box(this->receptionist_window, 0, 0);
        wattroff(this->receptionist_window, COLOR_PAIR(RECEP_C));
        wattron(this->progress_window, COLOR_PAIR(RECEP_C));
        box(this->progress_window, 0, 0);
        wattroff(this->progress_window, COLOR_PAIR(RECEP_C));
        mvwprintw(this->receptionist_window, 1, 1, "RECEPTIONIST LOOKING FOR A ROOM  ");
        wrefresh(this->receptionist_window);
        wrefresh(this->progress_window);
    }

    void check_free_rooms()
    {
        fill_progress_bar(this->progress_window, COLOR_PAIR(RECEP_C), 20, std::experimental::randint(500, 1000));
        {
            std::unique_lock<std::mutex> lock(this->mx);
            while (this->ready_rooms.size() == 0)
            {
                this->cv_pick_a_room.wait(lock);
                if (cancellation_token)
                {
                    return;
                }
            }

            int idx = std::experimental::randint(0, (int)ready_rooms.size() - 1);
            this->number_to_check_in = this->ready_rooms[idx]; //draw a ready room
            this->found_a_ready_room = true;
            this->ready_rooms.erase(this->ready_rooms.begin() + idx);
            this->cv_assign_room.notify_one(); //notify a guest that an empty room has been found
        }

        {
            std::lock_guard<std::mutex> writing_lock(mx_writing);
            mvwprintw(this->receptionist_window, 1, 14, "FOUND A FREE ROOM %d", this->number_to_check_in);
            wrefresh(this->receptionist_window);
        }

        fill_progress_bar(this->progress_window, COLOR_PAIR(RECEP_C), 20, std::experimental::randint(500, 1000));

        {
            std::lock_guard<std::mutex> writing_lock(mx_writing);
            mvwprintw(this->receptionist_window, 1, 14, "LOOKING FOR A ROOM  ");
            wrefresh(this->receptionist_window);
        }
    }

    void accommodate_guests()
    {
        while (!cancellation_token)
        {
            check_free_rooms();
        }
    }
};
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
    Receptionist(std::vector<Room> &rooms) : rooms(rooms) {}
    std::vector<Room> &rooms;

    std::mutex mx;
    std::condition_variable cv_look_for_a_room, cv_assign_room;
    std::atomic<bool> found_a_ready_room{false};

    int number_to_check_in = 0;

    int y_size = 5, x_size = 40, y_corner, x_corner;
    WINDOW *receptionist_window;
    WINDOW *progress_window;

    void draw_receptionist_desk()
    {
        receptionist_window = newwin(3, 40, 21, 100);
        progress_window = newwin(3, 20, 21, 140);
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
            std::unique_lock<std::mutex> lock_receptionist(mx);

            do
            {
                this->number_to_check_in = std::experimental::randint(0, (int)rooms.size() - 1); //try to find an empty room
            } while (!rooms[this->number_to_check_in].is_ready_for_guest);

            found_a_ready_room = true;
            cv_assign_room.notify_one(); //notify a guest that an empty room has been found
        }

        {
            std::lock_guard<std::mutex> writing_lock(mx_writing);
            mvwprintw(this->receptionist_window, 1, 14, "FOUND A FREE ROOM %d", this->number_to_check_in);
            wrefresh(this->receptionist_window);
        }

        fill_progress_bar(this->progress_window, COLOR_PAIR(RECEP_C), 20, std::experimental::randint(1000, 1400));

        {
            std::lock_guard<std::mutex> writing_lock(mx_writing);
            mvwprintw(this->receptionist_window, 1, 14, "LOOKING FOR A ROOM  ");
            wrefresh(this->receptionist_window);
        }
    }

    void accommodate_guests()
    {
        while (true)
        {
            check_free_rooms();
        }
    }
};
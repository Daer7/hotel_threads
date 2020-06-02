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

#include "screen_utils.cpp"

struct Room
{
    Room() {}
    int id;
    int guest_id = -1;
    int floor;

    int y_size = 7, x_size = 7, y_corner, x_corner;
    WINDOW *room_window;

    void draw_room()
    {
        room_window = newwin(y_size, x_size, y_corner, x_corner);
        std::lock_guard<std::mutex> writing_lock(mx_writing);
        box(room_window, 0, 0);
        mvwprintw(room_window, 1, 2, "%2d", this->id);
        wrefresh(room_window);
    }
    void guest_arrives(int guest_id)
    {
        this->guest_id = guest_id;
    }
    void display_guest_arrival()
    {
        std::lock_guard<std::mutex> writing_lock(mx_writing);
        wattron(room_window, COLOR_PAIR(GUEST_C));
        mvwprintw(room_window, 2, 2, "%2d", this->guest_id);
        wattroff(room_window, COLOR_PAIR(GUEST_C));
        wrefresh(room_window);
    }
    void guest_leaves(int guest_id)
    {
        this->guest_id = -1;
        std::lock_guard<std::mutex> writing_lock(mx_writing);
        mvwprintw(room_window, 2, 2, "  ");
        wrefresh(room_window);
    }

    void room_being_cleaned(int cleaner_id)
    {
        std::lock_guard<std::mutex> writing_lock(mx_writing);
        wattron(this->room_window, COLOR_PAIR(CLEANER_C));
        mvwprintw(this->room_window, 3, 2, "C%1d", cleaner_id);
        wattroff(this->room_window, COLOR_PAIR(CLEANER_C));
        wrefresh(this->room_window);
    }

    void room_clean()
    {
        std::lock_guard<std::mutex> writing_lock(mx_writing);
        mvwprintw(this->room_window, 3, 2, "  ");
        wrefresh(this->room_window);
    }
};
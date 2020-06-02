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

struct Jacuzzi
{
    Jacuzzi() : capacity(2), floor(0) {}
    const int capacity;
    const int floor;
    int guests_inside = 0;

    std::mutex place_mxs[2];

    WINDOW *jacuzzi_window;

    void draw_jacuzzi()
    {
        jacuzzi_window = newwin(24, 40, 0, 160);

        std::lock_guard<std::mutex> writing_lock(mx_writing);
        wattron(this->jacuzzi_window, COLOR_PAIR(JACUZZI_C));
        box(this->jacuzzi_window, 0, 0);
        wattroff(this->jacuzzi_window, COLOR_PAIR(JACUZZI_C));
        mvwprintw(this->jacuzzi_window, 5, 17, "JACUZZI");
        mvwprintw(this->jacuzzi_window, 6, 17, "FLOOR %d", this->floor);
        mvwprintw(this->jacuzzi_window, 9, 13, "AVAILABLE PLACES");
        mvwprintw(this->jacuzzi_window, 10, 20, "%d", this->capacity - this->guests_inside);
        wrefresh(this->jacuzzi_window);
    }

    void update_guests_inside(int guest_id, char s)
    {
        std::lock_guard<std::mutex> writing_lock(mx_writing);

        mvwprintw(this->jacuzzi_window, 10, 20, "%d", this->capacity - this->guests_inside);
        wattron(this->jacuzzi_window, COLOR_PAIR(GUEST_C));
        mvwprintw(this->jacuzzi_window, 13, 18, "%2d", guest_id);
        wattroff(this->jacuzzi_window, COLOR_PAIR(GUEST_C));
        if (s == 'I')
        {
            mvwprintw(this->jacuzzi_window, 13, 21, "IN ");
        }
        else
        {
            mvwprintw(this->jacuzzi_window, 13, 21, "OUT");
        }

        wrefresh(this->jacuzzi_window);
    }
};
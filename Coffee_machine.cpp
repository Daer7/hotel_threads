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

struct Coffee_machine
{
    Coffee_machine() {}
    int millilitres = 500;
    int floor = 2;

    std::list<std::pair<int, int>> guests_amounts_list;

    std::mutex mx;
    std::condition_variable cv_drink;

    WINDOW *coffee_window;

    void draw_coffee_machine()
    {
        coffee_window = newwin(12, 20, 0, 80);

        std::lock_guard<std::mutex> writing_lock(mx_writing);
        wattron(this->coffee_window, COLOR_PAIR(COFFEE_C));
        box(this->coffee_window, 0, 0);
        wattroff(this->coffee_window, COLOR_PAIR(COFFEE_C));
        mvwprintw(this->coffee_window, 1, 7, "COFFEE");
        mvwprintw(this->coffee_window, 2, 7, "FLOOR 2");
        mvwprintw(this->coffee_window, 8, 2, "AMOUNT OF COFFEE");
        mvwprintw(this->coffee_window, 9, 7, "%4d ML", this->millilitres);
        wrefresh(this->coffee_window);
    }

    void guest_drinks(int guest_id, int amount)
    {
        std::lock_guard<std::mutex> writing_lock(mx_writing);
        mvwprintw(this->coffee_window, 9, 7, "%4d", this->millilitres);
        wattron(this->coffee_window, COLOR_PAIR(GUEST_C));
        mvwprintw(this->coffee_window, 5, 3, "GUEST %2d POURED", guest_id);
        mvwprintw(this->coffee_window, 6, 7, "-%4d ML", amount);
        wattroff(this->coffee_window, COLOR_PAIR(GUEST_C));
        wrefresh(this->coffee_window);
    }

    void waiter_refills(int amount)
    {
        std::lock_guard<std::mutex> writing_lock(mx_writing);
        mvwprintw(this->coffee_window, 9, 7, "%4d", this->millilitres);
        wattron(this->coffee_window, COLOR_PAIR(WAITER_C));
        mvwprintw(this->coffee_window, 5, 3, "WAITER REFILLED");
        mvwprintw(this->coffee_window, 6, 7, "+%4d ML", amount);
        wattroff(this->coffee_window, COLOR_PAIR(WAITER_C));
        wrefresh(this->coffee_window);
    }
};
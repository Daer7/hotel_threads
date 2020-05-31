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

struct Waiter
{
    Waiter(Coffee_machine &coffee_machine) : coffee_machine(coffee_machine) {}
    Coffee_machine &coffee_machine;

    WINDOW *waiter_window;
    WINDOW *progress_window;

    void draw_waiter()
    {
        waiter_window = newwin(3, 60, 12, 0);
        progress_window = newwin(3, 40, 12, 60);

        std::lock_guard<std::mutex> writing_lock(mx_writing);
        wattron(this->waiter_window, COLOR_PAIR(WAITER_C));
        box(this->waiter_window, 0, 0);
        wattroff(this->waiter_window, COLOR_PAIR(WAITER_C));
        wattron(this->progress_window, COLOR_PAIR(WAITER_C));
        box(this->progress_window, 0, 0);
        wattroff(this->progress_window, COLOR_PAIR(WAITER_C));
        mvwprintw(this->waiter_window, 1, 1, "WAITER READY TO SERVE            ");
        wrefresh(this->waiter_window);
        wrefresh(this->progress_window);
    }

    void refill_coffee()
    {
        {
            std::lock_guard<std::mutex> writing_lock(mx_writing);
            mvwprintw(this->waiter_window, 1, 8, "READY TO SERVE             ");
            wrefresh(this->waiter_window);
        }
        fill_progress_bar(this->progress_window, COLOR_PAIR(WAITER_C), 40, std::experimental::randint(3500, 4500));

        {
            std::unique_lock<std::mutex> lock_coffee(coffee_machine.mx);

            int refill_amount = 0;
            for (std::pair<int, int> &guest_amount : coffee_machine.guests_amounts_list) //calculate sufficient refill vallue
            {
                refill_amount += guest_amount.second; //sum the amount all waiting guests want to drink
            }

            int final_refill_amount = refill_amount + std::experimental::randint(100, 300); //refill a little more than guests need
            coffee_machine.millilitres += final_refill_amount;

            {
                std::lock_guard<std::mutex> writing_lock(mx_writing);
                mvwprintw(this->waiter_window, 1, 8, "REFILLING %4d ML OF COFFEE", final_refill_amount);
                wrefresh(this->waiter_window);
            }
            fill_progress_bar(this->progress_window, COLOR_PAIR(WAITER_C), 40, std::experimental::randint(2500, 3500));
            coffee_machine.waiter_refills(final_refill_amount);

            coffee_machine.cv_drink.notify_all(); //let all waiting guests know that coffee has been refilled
        }
    }

    void serve_guests()
    {
        while (true)
        {
            refill_coffee();
        }
    }
};
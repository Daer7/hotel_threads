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

#include "Room.cpp"
#include "Coffee_machine.cpp"
#include "Jacuzzi.cpp"
#include "Elevator.cpp"
#include "Receptionist.cpp"
#include "Guest.cpp"
#include "Waiter.cpp"
#include "Cleaner.cpp"

int main()
{
    initscr();
    echo();
    cbreak();

    start_color();
    init_pair(RECEP_C, COLOR_YELLOW, COLOR_BLACK);
    init_pair(GUEST_C, COLOR_BLUE, COLOR_BLACK);
    init_pair(CLEANER_C, COLOR_RED, COLOR_BLACK);
    init_pair(ELEVATOR_C, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(WAITER_C, COLOR_CYAN, COLOR_BLACK);
    init_pair(COFFEE_C, COLOR_GREEN, COLOR_BLACK);
    init_pair(JACUZZI_C, COLOR_YELLOW, COLOR_BLACK);
    init_pair(EXIT_C, COLOR_BLACK, COLOR_BLUE);
    init_pair(EXIT_C2, COLOR_YELLOW, COLOR_BLACK);

    WINDOW *exitwin = newwin(4, 54, 4, 6);
    wbkgd(exitwin, COLOR_PAIR(EXIT_C));
    wattron(exitwin, COLOR_PAIR(EXIT_C2));
    mvwprintw(exitwin, 1, 1, "PRESS Q TO EXIT (IT TAKES A REALLY LONG TIME):  ");
    wrefresh(exitwin);

    std::vector<Room> rooms(9);
    for (int i = 8; i >= 0; i--)
    {
        rooms[i].id = i;
        rooms[i].floor = rooms[i].id / 3;
        rooms[i].y_corner = (8 - i) / 3 * 7;
        rooms[i].x_corner = (8 - i) % 3 * 7 + 100;
        rooms[i].draw_room();
    }

    Receptionist receptionist(rooms);
    receptionist.draw_receptionist_desk();

    std::vector<Cleaner> cleaners;
    for (int i = 0; i < 3; i++)
    {
        cleaners.emplace_back(i, receptionist);
        cleaners[i].yc_corner = 15 + i * 3;
        cleaners[i].yp_corner = 15 + i * 3;
        cleaners[i].draw_cleaner();
    }

    Coffee_machine coffee_machine;
    coffee_machine.draw_coffee_machine();

    Jacuzzi jacuzzi;
    jacuzzi.draw_jacuzzi();

    Elevator elevator;
    elevator.draw_elevator();

    Waiter waiter(coffee_machine);
    waiter.draw_waiter();

    std::vector<Guest> guests;
    for (int i = 0; i < 15; i++)
    {
        guests.emplace_back(i, receptionist, coffee_machine, jacuzzi, elevator);
        guests[i].yg_corner = 24 + i % 10 * 3;
        guests[i].xg_corner = (i / 10) * 100;
        guests[i].yp_corner = 24 + i % 10 * 3;
        guests[i].xp_corner = 60 + (i / 10) * 100;
        guests[i].draw_guest();
    }

    std::vector<std::thread> threadList;

    threadList.emplace_back(&Receptionist::accommodate_guests, std::ref(receptionist));
    for (Guest &guest : guests)
    {
        threadList.emplace_back(&Guest::have_holiday, std::ref(guest));
    }
    for (Cleaner &cleaner : cleaners)
    {
        threadList.emplace_back(&Cleaner::clean_rooms, std::ref(cleaner));
    }
    threadList.emplace_back(&Waiter::serve_guests, std::ref(waiter));
    threadList.emplace_back(&Elevator::go, std::ref(elevator));

    while (true)
    {
        char c = wgetch(exitwin);
        if (c == 'q')
        {
            cancellation_token_guests = true;
            std::unique_lock<std::mutex> lock(mx_guests_exit);
            while (num_of_finished_guest_threads < 15)
            {
                cv_guests_exit.wait(lock);
            }
            cancellation_token = true;
            //notify other threads if waiting in vain
            receptionist.cv_pick_a_room.notify_all();
            receptionist.cv_clean_room.notify_all();
            break;
        }
    }

    for (std::thread &t : threadList)
    {
        t.join();
    }

    endwin();
    return 0;
}
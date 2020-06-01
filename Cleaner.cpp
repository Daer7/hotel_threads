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

struct Cleaner
{
    Cleaner(int id, Receptionist &receptionist, std::vector<Room> &rooms) : id(id), receptionist(receptionist), rooms(rooms) {}
    int id;

    Receptionist &receptionist;
    std::vector<Room> &rooms;

    WINDOW *cleaner_window;
    WINDOW *progress_window;

    int yc_size = 3, xc_size = 60, yc_corner, xc_corner = 0, yp_size = 3, xp_size = 40, yp_corner, xp_corner = 60;

    void draw_cleaner()
    {
        cleaner_window = newwin(yc_size, xc_size, yc_corner, xc_corner);
        progress_window = newwin(yp_size, xp_size, yp_corner, xp_corner);
        std::lock_guard<std::mutex> writing_lock(mx_writing);
        wattron(this->cleaner_window, COLOR_PAIR(CLEANER_C));
        box(this->cleaner_window, 0, 0);
        wattroff(this->cleaner_window, COLOR_PAIR(CLEANER_C));
        wattron(this->progress_window, COLOR_PAIR(CLEANER_C));
        box(this->progress_window, 0, 0);
        wattroff(this->progress_window, COLOR_PAIR(CLEANER_C));
        mvwprintw(cleaner_window, 1, 1, "CLEANER %1d WAITING FOR A ROOM", this->id);
        wrefresh(cleaner_window);
        wrefresh(progress_window);
    }

    void find_room_to_clean()
    {
        for (Room &room : rooms)
        {
            if (!room.mx.try_lock()) //try to lock, in case of failure, continue loop
            {
                continue;
            }
            else //see if room needs to be cleaned
            {
                std::unique_lock<std::mutex> lock_room(room.mx, std::adopt_lock);
                if (room.guest_id == -1 && !room.is_ready_for_guest)
                {
                    room.room_being_cleaned(this->id);
                    {
                        std::lock_guard<std::mutex> writing_lock(mx_writing);
                        mvwprintw(this->cleaner_window, 1, 11, "CLEANING ROOM %2d  ", room.id);
                        wrefresh(this->cleaner_window);
                    }
                    fill_progress_bar(this->progress_window, COLOR_PAIR(CLEANER_C), 40, std::experimental::randint(2500, 3500));

                    room.is_ready_for_guest = true;
                    room.room_clean();
                    {
                        std::lock_guard<std::mutex> writing_lock(mx_writing);
                        mvwprintw(this->cleaner_window, 1, 11, "WAITING FOR A ROOM");
                        wrefresh(this->cleaner_window);
                    }
                    fill_progress_bar(this->progress_window, COLOR_PAIR(CLEANER_C), 40, std::experimental::randint(1500, 2500));
                }
            }
        }
    }

    void clean_rooms()
    {
        while (true)
        {
            find_room_to_clean();
        }
    }
};
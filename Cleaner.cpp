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
    Cleaner(int id, Receptionist &receptionist) : id(id), receptionist(receptionist) {}
    int id;

    Receptionist &receptionist;

    WINDOW *cleaner_window;
    WINDOW *progress_window;

    int yc_size = 3, xc_size = 60, yc_corner, xc_corner = 0, yp_size = 3, xp_size = 40, yp_corner, xp_corner = 60;

    void draw_cleaner()
    {
        this->cleaner_window = newwin(yc_size, xc_size, yc_corner, xc_corner);
        this->progress_window = newwin(yp_size, xp_size, yp_corner, xp_corner);
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
        int room_to_clean_id;
        {
            std::unique_lock<std::mutex> lock(receptionist.mx);
            while (receptionist.rooms_to_be_cleaned.size() == 0)
            {
                receptionist.cv_clean_room.wait(lock); //wait for notification from a guest
                if (cancellation_token)
                {
                    return;
                }
            }

            // draw a room to clean, remember it and erase from rooms_to_be_cleaned
            int idx = std::experimental::randint(0, (int)receptionist.rooms_to_be_cleaned.size() - 1);
            room_to_clean_id = receptionist.rooms_to_be_cleaned[idx];
            receptionist.rooms_to_be_cleaned.erase(receptionist.rooms_to_be_cleaned.begin() + idx);
        }

        receptionist.rooms[room_to_clean_id].room_being_cleaned(this->id);
        {
            std::lock_guard<std::mutex> writing_lock(mx_writing);
            mvwprintw(this->cleaner_window, 1, 11, "CLEANING ROOM %2d  ", room_to_clean_id);
            wrefresh(this->cleaner_window);
        }
        fill_progress_bar(this->progress_window, COLOR_PAIR(CLEANER_C), 40, std::experimental::randint(2500, 3500));
        receptionist.rooms[room_to_clean_id].room_clean();

        {
            std::unique_lock<std::mutex> lock(receptionist.mx);
            receptionist.ready_rooms.push_back(room_to_clean_id);
            receptionist.cv_pick_a_room.notify_one(); //let receptionist know
        }

        {
            std::lock_guard<std::mutex> writing_lock(mx_writing);
            mvwprintw(this->cleaner_window, 1, 11, "WAITING FOR A ROOM");
            wrefresh(this->cleaner_window);
        }
        fill_progress_bar(this->progress_window, COLOR_PAIR(CLEANER_C), 40, std::experimental::randint(1000, 1500));
    }

    void clean_rooms()
    {
        while (!cancellation_token)
        {
            find_room_to_clean();
        }
    }
};
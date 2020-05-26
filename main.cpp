#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <experimental/random>
#include <atomic>
#include <ncurses.h>
#include <condition_variable>
#include <memory>

#define ROOM_C 1
#define RECEP_C 2
#define GUEST_C 3
#define CLEANER_C 4

std::mutex mx_writing;
std::atomic<bool> cancellation_token(false);

void fill_progress_bar(WINDOW *progress_window, int COL, int width, int sleep_time)
{
    werase(progress_window);
    wattron(progress_window, COL);
    box(progress_window, 0, 0);
    wattroff(progress_window, COL);
    int single_sleep = sleep_time / (width - 2);
    for (int i = 1; i < width - 1; i++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(single_sleep));
        {
            std::lock_guard<std::mutex> writing_lock(mx_writing);
            mvwprintw(progress_window, 1, i, "$");
            wrefresh(progress_window);
        }
    }
}

struct Guest;

struct Room
{
    Room() {}
    int id;
    int guest_id = -1;
    int floor;
    std::atomic<bool> is_ready_for_guest{true};
    std::mutex mx;

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
        this->is_ready_for_guest = false;
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
};

struct Coffee_machine
{
    Coffee_machine() {}
    std::mutex mx;
    int millilitres = 3000;
    int floor = 1;
};

struct Swimming_pool
{
    Swimming_pool() {}
    std::mutex mx;
    int capacity = 5;
    int floor = 2;
};

struct Elevator
{
    std::vector<int> guests_inside;
    std::mutex mxs[3];
    std::condition_variable cvs[3];
};

struct Receptionist
{
    Receptionist(std::vector<Room> &rooms) : rooms(rooms) {}
    std::vector<Room> &rooms;

    std::mutex mx;
    std::condition_variable cv;
    std::atomic<bool> is_a_room_ready{false};
    int number_to_check_in = 0;

    int y_size = 5, x_size = 40, y_corner, x_corner;
    WINDOW *receptionist_window;
    WINDOW *progress_window;

    void draw_receptionist_desk()
    {
        receptionist_window = newwin(3, 40, 21, 0);
        progress_window = newwin(3, 20, 21, 40);
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
        fill_progress_bar(this->progress_window, COLOR_PAIR(RECEP_C), 20, std::experimental::randint(1400, 1800));
        std::unique_lock<std::mutex> lock_receptionist(mx);
        do
        {
            this->number_to_check_in = std::experimental::randint(0, (int)rooms.size() - 1); //find an empty room
        } while (!rooms[this->number_to_check_in].is_ready_for_guest);
        is_a_room_ready = true;
        cv.notify_one();

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

struct Guest
{
    Guest(int id, Receptionist &receptionist, Coffee_machine &coffee_machine,
          Swimming_pool &swimming_pool, Elevator &elevator) : id(id), receptionist(receptionist),
                                                              coffee_machine(coffee_machine), swimming_pool(swimming_pool), elevator(elevator) {}
    int id;
    int room_id = -1;

    int current_floor;

    Receptionist &receptionist;
    Coffee_machine &coffee_machine;
    Swimming_pool &swimming_pool;
    Elevator &elevator;

    WINDOW *guest_window;
    WINDOW *progress_window;

    int yg_size = 3, xg_size = 60, yg_corner, xg_corner, yp_size = 3, xp_size = 40, yp_corner, xp_corner;

    void draw_guest()
    {
        guest_window = newwin(yg_size, xg_size, yg_corner, xg_corner);
        progress_window = newwin(yp_size, xp_size, yp_corner, xp_corner);

        std::lock_guard<std::mutex> writing_lock(mx_writing);
        wattron(this->guest_window, COLOR_PAIR(GUEST_C));
        box(this->guest_window, 0, 0);
        wattroff(this->guest_window, COLOR_PAIR(GUEST_C));
        wattron(this->progress_window, COLOR_PAIR(GUEST_C));
        box(this->progress_window, 0, 0);
        wattroff(this->progress_window, COLOR_PAIR(GUEST_C));
        mvwprintw(this->guest_window, 1, 1, "GUEST %2d ON FLOOR:   ", this->id);
        wrefresh(this->guest_window);
        wrefresh(this->progress_window);
    }

    void check_in()
    {
        std::unique_lock<std::mutex> lock_receptionist(receptionist.mx);
        while (!receptionist.is_a_room_ready)
        {
            receptionist.cv.wait(lock_receptionist);
        }
        receptionist.is_a_room_ready = false;
        this->room_id = receptionist.number_to_check_in;               //assign room to guest
        this->current_floor = receptionist.rooms[this->room_id].floor; //assign floor where guest currently is
        receptionist.rooms[this->room_id].guest_arrives(this->id);     //assign guest to room && room becomes occupied
        {
            std::lock_guard<std::mutex> writing_lock(mx_writing);
            mvwprintw(this->guest_window, 1, 19, "%1d CHECKED IN TO ROOM %1d", this->current_floor, this->room_id);
            wrefresh(this->guest_window);
        }
        fill_progress_bar(this->progress_window, COLOR_PAIR(GUEST_C), 40, std::experimental::randint(2500, 3500));
    }

    void drink_coffee()
    {
        std::unique_lock<std::mutex> lock_coffee(coffee_machine.mx);
        int amount_to_drink = std::experimental::randint(400, 600);
        if (coffee_machine.millilitres - amount_to_drink >= 0)
        {
            coffee_machine.millilitres -= amount_to_drink;
        }
    }

    void go_swimming()
    {
        {
            std::unique_lock<std::mutex> lock_coffee(swimming_pool.mx);
            if (swimming_pool.capacity > 0)
            {
                swimming_pool.capacity -= 1;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(std::experimental::randint(500, 700)));
        {
            std::unique_lock<std::mutex> lock_coffee(swimming_pool.mx);
            swimming_pool.capacity += 1;
        }
    }

    void use_elevator(int destination_floor)
    {
        std::unique_lock<std::mutex> lock_elevator(this->elevator.mxs[this->current_floor]);
        while (this->elevator.guests_inside.size() > 3)
        {
            this->elevator.cvs[this->current_floor].wait(lock_elevator);
        }
    }

    void leave_hotel()
    {
        receptionist.rooms[this->room_id].guest_leaves(this->id);
        {
            std::lock_guard<std::mutex> writing_lock(mx_writing);
            mvwprintw(this->guest_window, 1, 19, "XX LEFT THE HOTEL");
            wrefresh(this->guest_window);
        }
        fill_progress_bar(this->progress_window, COLOR_PAIR(GUEST_C), 40, std::experimental::randint(2500, 3500));
    }

    void have_holiday()
    {
        while (true)
        {
            check_in();
            leave_hotel();
        }
    }
};

struct Waiter
{
    Waiter(Coffee_machine &coffee_machine) : coffee_machine(coffee_machine) {}
    Coffee_machine &coffee_machine;

    void refill_coffee()
    {
        std::unique_lock<std::mutex> lock_coffee(coffee_machine.mx);
        if (coffee_machine.millilitres < 450)
        {
            coffee_machine.millilitres += std::experimental::randint(2000, 3000);
        }
    }
};

struct Cleaner
{
    Cleaner(int id, std::vector<Room> &rooms) : id(id), rooms(rooms) {}
    int id;
    std::vector<Room> &rooms;

    WINDOW *cleaner_window;
    WINDOW *progress_window;

    int yc_size = 3, xc_size = 60, yc_corner, xc_corner = 100, yp_size = 3, xp_size = 40, yp_corner, xp_corner = 160;

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
            std::unique_lock<std::mutex> lock_room(room.mx);
            if (room.guest_id == -1 && !room.is_ready_for_guest)
            {
                {
                    std::lock_guard<std::mutex> writing_lock(mx_writing);
                    mvwprintw(this->cleaner_window, 1, 11, "CLEANING ROOM %2d  ", room.id);
                    wrefresh(this->cleaner_window);
                }
                fill_progress_bar(this->progress_window, COLOR_PAIR(CLEANER_C), 40, std::experimental::randint(2500, 3500));
                room.is_ready_for_guest = true;
                {
                    std::lock_guard<std::mutex> writing_lock(mx_writing);
                    mvwprintw(this->cleaner_window, 1, 11, "WAITING FOR A ROOM");
                    wrefresh(this->cleaner_window);
                }
                fill_progress_bar(this->progress_window, COLOR_PAIR(CLEANER_C), 40, std::experimental::randint(2500, 3500));
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

int main()
{
    initscr();
    echo();
    cbreak();

    start_color();
    init_pair(ROOM_C, COLOR_GREEN, COLOR_BLACK);
    init_pair(RECEP_C, COLOR_YELLOW, COLOR_BLACK);
    init_pair(GUEST_C, COLOR_BLUE, COLOR_BLACK);
    init_pair(CLEANER_C, COLOR_RED, COLOR_BLACK);

    std::vector<Room> rooms(9);
    for (int i = 8; i >= 0; i--)
    {
        rooms[i].id = i;
        rooms[i].floor = rooms[i].id / 3;
        rooms[i].y_corner = (8 - i) / 3 * 7;
        rooms[i].x_corner = (8 - i) % 3 * 7;
        rooms[i].draw_room();
    }

    Receptionist receptionist(rooms);
    receptionist.draw_receptionist_desk();

    std::vector<Cleaner> cleaners;
    for (int i = 0; i < 3; i++)
    {
        cleaners.emplace_back(i, rooms);
        cleaners[i].yc_corner = 39 + i * 3;
        cleaners[i].yp_corner = 39 + i * 3;
        cleaners[i].draw_cleaner();
    }

    Coffee_machine coffee_machine;
    Swimming_pool swimming_pool;
    Elevator elevator;

    std::vector<Guest> guests;
    for (int i = 0; i < 15; i++)
    {
        guests.emplace_back(i, receptionist, coffee_machine, swimming_pool, elevator);
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

    for (std::thread &t : threadList)
    {
        t.join();
    }

    return 0;
}
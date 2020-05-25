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
#define PHIL_C 3
#define FORK_C 4

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
    std::atomic<bool> is_ready_for_guest{true};
    std::mutex mx;

    int y_size = 5, x_size = 5, y_corner, x_corner;
    WINDOW *room_window;

    void draw_room()
    {
        room_window = newwin(y_size, x_size, y_corner, x_corner);
        std::lock_guard<std::mutex> writing_lock(mx_writing);
        box(room_window, 0, 0);
        mvwprintw(room_window, 1, 1, "%2d", this->id);
        wrefresh(room_window);
    }
    void guest_arrives(int guest_id)
    {
        this->guest_id = guest_id;
        this->is_ready_for_guest = false;
        std::lock_guard<std::mutex> writing_lock(mx_writing);
        wattron(room_window, COLOR_PAIR(ROOM_C));
        mvwprintw(room_window, 2, 1, "%2d", this->guest_id);
        wattroff(room_window, COLOR_PAIR(ROOM_C));
        wrefresh(room_window);
    }
    void guest_leaves(int guest_id)
    {
        this->guest_id = -1;
        std::lock_guard<std::mutex> writing_lock(mx_writing);
        mvwprintw(room_window, 2, 1, "  ");
        wrefresh(room_window);
    }
};

struct Coffee_machine
{
    Coffee_machine() {}
    std::mutex mx;
    int millilitres = 3000;
};

struct Swimming_pool
{
    Swimming_pool() {}
    std::mutex mx;
    int capacity = 5;
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
        receptionist_window = newwin(3, 40, 15, 0);
        progress_window = newwin(3, 20, 15, 40);
        std::lock_guard<std::mutex> writing_lock(mx_writing);
        wattron(this->receptionist_window, COLOR_PAIR(RECEP_C));
        box(this->receptionist_window, 0, 0);
        wattroff(this->receptionist_window, COLOR_PAIR(RECEP_C));
        wattron(this->progress_window, COLOR_PAIR(RECEP_C));
        box(this->progress_window, 0, 0);
        wattroff(this->progress_window, COLOR_PAIR(RECEP_C));
        mvwprintw(this->receptionist_window, 1, 1, "RECEPTIONIST FOUND EMPTY ROOM: ");
        wrefresh(this->receptionist_window);
        wrefresh(this->progress_window);
    }
    void check_free_rooms()
    {
        std::unique_lock<std::mutex> lock_receptionist(mx);
        do
        {
            this->number_to_check_in = std::experimental::randint(0, (int)rooms.size() - 1); //find an empty room
        } while (!rooms[this->number_to_check_in].is_ready_for_guest);
        is_a_room_ready = true;
        cv.notify_one();
        std::lock_guard<std::mutex> writing_lock(mx_writing);
        mvwprintw(this->receptionist_window, 1, 32, "  ");
        mvwprintw(this->receptionist_window, 1, 32, "%2d", this->number_to_check_in);
        wrefresh(this->receptionist_window);
    }

    void accommodate_guests()
    {
        while (true)
        {
            fill_progress_bar(this->progress_window, COLOR_PAIR(RECEP_C), 20, std::experimental::randint(1000, 1400));
            check_free_rooms();
        }
    }
};

struct Guest
{
    Guest(int id, Receptionist &receptionist, Coffee_machine &coffee_machine,
          Swimming_pool &swimming_pool) : id(id), receptionist(receptionist),
                                          coffee_machine(coffee_machine), swimming_pool(swimming_pool) {}
    int id;
    int room_id = -1;

    Receptionist &receptionist;
    Coffee_machine &coffee_machine;
    Swimming_pool &swimming_pool;

    void check_in()
    {
        std::unique_lock<std::mutex> lock_receptionist(receptionist.mx);
        while (!receptionist.is_a_room_ready)
        {
            receptionist.cv.wait(lock_receptionist);
        }
        receptionist.is_a_room_ready = false;
        this->room_id = receptionist.number_to_check_in;           //assign room to guest
        receptionist.rooms[this->room_id].guest_arrives(this->id); //assign guest to room && room becomes occupied
        {
            std::lock_guard<std::mutex> writing_lock(mx_writing);
            //std::cout << "Guest " << this->id << " accomodated in room " << this->room_id << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
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

    void leave_hotel()
    {
        receptionist.rooms[this->room_id].guest_leaves(this->id);
    }

    void have_holiday()
    {
        while (true)
        {
            check_in();
            std::this_thread::sleep_for(std::chrono::milliseconds(std::experimental::randint(1000, 1500)));
            leave_hotel();
            std::this_thread::sleep_for(std::chrono::milliseconds(std::experimental::randint(500, 1000)));
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
    void find_room_to_clean()
    {
        for (Room &room : rooms)
        {
            std::unique_lock<std::mutex> lock_room(room.mx);
            if (room.guest_id == -1 && !room.is_ready_for_guest)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(std::experimental::randint(1000, 2000)));
                room.is_ready_for_guest = true;
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

    std::vector<Room> rooms(9);
    for (int i = 0; i < 9; i++)
    {
        rooms[i].id = i;
        rooms[i].y_corner = i / 3 * 5;
        rooms[i].x_corner = i % 3 * 5;
        rooms[i].draw_room();
    }

    Receptionist receptionist(rooms);
    receptionist.draw_receptionist_desk();

    std::vector<Cleaner> cleaners;
    for (int i = 0; i < 3; i++)
    {
        cleaners.emplace_back(i, rooms);
    }

    Coffee_machine coffee_machine;
    Swimming_pool swimming_pool;

    std::vector<Guest> guests;
    for (int i = 0; i < 15; i++)
    {
        guests.emplace_back(i, receptionist, coffee_machine, swimming_pool);
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
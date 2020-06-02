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

struct Guest
{
    Guest(int id, Receptionist &receptionist, Coffee_machine &coffee_machine,
          Jacuzzi &jacuzzi, Elevator &elevator) : id(id), receptionist(receptionist),
                                                  coffee_machine(coffee_machine), jacuzzi(jacuzzi), elevator(elevator) {}
    int id;
    int room_id = -1;

    int current_floor = 0;

    Receptionist &receptionist;
    Coffee_machine &coffee_machine;
    Jacuzzi &jacuzzi;
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
        mvwprintw(this->guest_window, 1, 1, "GUEST %2d ROOM: X | FLOOR:", this->id);
        wrefresh(this->guest_window);
        wrefresh(this->progress_window);
    }

    void check_in()
    {
        {
            std::lock_guard<std::mutex> writing_lock(mx_writing);
            mvwprintw(this->guest_window, 1, 27, "0 WAITING FOR CHECK-IN          ");
            wrefresh(this->guest_window);
        }
        fill_progress_bar(this->progress_window, COLOR_PAIR(GUEST_C), 40, std::experimental::randint(2500, 3500));

        {
            std::unique_lock<std::mutex> lock_receptionist(receptionist.mx);
            while (!receptionist.found_a_ready_room)
            {
                receptionist.cv_assign_room.wait(lock_receptionist);
            }

            receptionist.found_a_ready_room = false;
            this->room_id = receptionist.number_to_check_in; //assign room to guest

            std::unique_lock<std::mutex> lock_room(receptionist.rooms[this->room_id].mx); //owns room mutex
            receptionist.rooms[this->room_id].guest_arrives(this->id);                    //assign guest to room && room becomes occupied
        }

        use_elevator(receptionist.rooms[this->room_id].floor); //go to your room by elevator

        receptionist.rooms[this->room_id].display_guest_arrival(); //show that guest is already in their room

        {
            std::lock_guard<std::mutex> writing_lock(mx_writing);
            mvwprintw(this->guest_window, 1, 16, "%1d", this->room_id);
            mvwprintw(this->guest_window, 1, 27, "%1d CHECKED IN TO ROOM %1d          ", this->current_floor, this->room_id);
            wrefresh(this->guest_window);
        }
        fill_progress_bar(this->progress_window, COLOR_PAIR(GUEST_C), 40, std::experimental::randint(2500, 3500));
    }

    void drink_coffee()
    {
        use_elevator(this->coffee_machine.floor);

        int amount_to_drink = std::experimental::randint(100, 300);
        {
            std::unique_lock<std::mutex> lock_coffee(coffee_machine.mx);
            {
                std::lock_guard<std::mutex> writing_lock(mx_writing);
                mvwprintw(this->guest_window, 1, 29, "WAITING TO DRINK COFFEE       ");
                wrefresh(this->guest_window);
            }

            coffee_machine.guests_amounts_list.emplace_back(this->id, amount_to_drink); //queue up for coffee

            while (coffee_machine.millilitres - amount_to_drink < 0)
            {
                coffee_machine.cv_drink.wait(lock_coffee);
            }

            coffee_machine.guests_amounts_list.remove(std::pair<int, int>(this->id, amount_to_drink)); //leave the queue

            coffee_machine.millilitres -= amount_to_drink;

            {
                std::lock_guard<std::mutex> writing_lock(mx_writing);
                mvwprintw(this->guest_window, 1, 29, "POURING %3d ML OF COFFEE      ", amount_to_drink);
                wrefresh(this->guest_window);
            }
            fill_progress_bar(this->progress_window, COLOR_PAIR(GUEST_C), 40, std::experimental::randint(1500, 2500));
            coffee_machine.guest_drinks(this->id, amount_to_drink);
        }
        {
            std::lock_guard<std::mutex> writing_lock(mx_writing);
            mvwprintw(this->guest_window, 1, 29, "DRINKING %3d ML OF COFFEE    ", amount_to_drink);
            wrefresh(this->guest_window);
        }
        //sleep while drinking coffee
        fill_progress_bar(this->progress_window, COLOR_PAIR(GUEST_C), 40, std::experimental::randint(2500, 3500));
    }

    void relax_in_jacuzzi()
    {
        use_elevator(this->jacuzzi.floor);

        std::atomic<bool> had_bath{false};
        for (int i = 0; i < 2; i++)
        {
            if (had_bath) //if guest has already had a bath in for loop
            {
                continue;
            }
            else if (!this->jacuzzi.place_mxs[i].try_lock()) //if place mutex owned by another thread
            {
                continue;
            }
            else
            {
                {
                    std::unique_lock<std::mutex> lock_jacuzzi(this->jacuzzi.place_mxs[i], std::adopt_lock);
                    this->jacuzzi.guests_inside++;
                    this->jacuzzi.update_guests_inside(this->id, 'I');
                    {
                        std::lock_guard<std::mutex> writing_lock(mx_writing);
                        mvwprintw(this->guest_window, 1, 29, "RELAXING IN JACUZZI           ");
                        wrefresh(this->guest_window);
                    }

                    fill_progress_bar(this->progress_window, COLOR_PAIR(GUEST_C), 40, std::experimental::randint(3500, 4500));

                    this->jacuzzi.guests_inside--;
                    this->jacuzzi.update_guests_inside(this->id, 'O');

                    had_bath = true;
                }

                {
                    std::lock_guard<std::mutex> writing_lock(mx_writing);
                    mvwprintw(this->guest_window, 1, 29, "GETTING DRESSED AFTER JACUZZI ");
                    wrefresh(this->guest_window);
                }

                fill_progress_bar(this->progress_window, COLOR_PAIR(GUEST_C), 40, std::experimental::randint(1500, 2500));
            }
        }

        if (!had_bath)
        {
            {
                std::lock_guard<std::mutex> writing_lock(mx_writing);
                mvwprintw(this->guest_window, 1, 29, "NO RELAX, JACUZZI WAS FULL    ");
                wrefresh(this->guest_window);
            }
            fill_progress_bar(this->progress_window, COLOR_PAIR(GUEST_C), 40, std::experimental::randint(2500, 3500));
        }
    }

    void use_elevator(int destination_floor)
    {
        if (this->current_floor != destination_floor) //use elevator only if you're not on the destination floor atm
        {
            {
                std::lock_guard<std::mutex> writing_lock(mx_writing);
                mvwprintw(this->guest_window, 1, 29, "WAITING TO GET TO FLOOR %d     ", destination_floor);
                wrefresh(this->guest_window);
            }
            {
                std::unique_lock<std::mutex> lock_elevator(this->elevator.mx);
                while (this->current_floor != elevator.current_floor)
                {
                    this->elevator.cv_in.wait(lock_elevator);
                }
                elevator.guests_inside.push_back(this->id);
            }

            {
                std::lock_guard<std::mutex> writing_lock(mx_writing);
                mvwprintw(this->guest_window, 1, 29, "IN THE ELEVATOR ---> FLOOR %d  ", destination_floor);
                wrefresh(this->guest_window);
            }
            {
                std::unique_lock<std::mutex> lock_elevator(this->elevator.mx);
                while (destination_floor != elevator.current_floor)
                {
                    this->elevator.cv_out.wait(lock_elevator);
                }
                //delete the id of the guest who is getting off
                elevator.guests_inside.erase(std::remove(elevator.guests_inside.begin(), elevator.guests_inside.end(), this->id), elevator.guests_inside.end());
                this->current_floor = destination_floor;
            }

            {
                std::lock_guard<std::mutex> writing_lock(mx_writing);
                mvwprintw(this->guest_window, 1, 27, "%d", destination_floor);
                mvwprintw(this->guest_window, 1, 29, "GOT OFF THE ELEVATOR, FLOOR %d ", destination_floor);
                wrefresh(this->guest_window);
            }
            fill_progress_bar(this->progress_window, COLOR_PAIR(GUEST_C), 40, std::experimental::randint(1000, 1500));
        }
    }

    void leave_hotel()
    {
        use_elevator(this->receptionist.rooms[this->room_id].floor); //go pick up your stuff

        {
            std::lock_guard<std::mutex> writing_lock(mx_writing);
            mvwprintw(this->guest_window, 1, 29, "PACKING STUFF BEFORE LEAVING  ");
            wrefresh(this->guest_window);
        }
        fill_progress_bar(this->progress_window, COLOR_PAIR(GUEST_C), 40, std::experimental::randint(1500, 2500));

        {
            std::unique_lock<std::mutex> lock_room(receptionist.rooms[this->room_id].mx); //acquire mutex before guest_id is changed
            receptionist.rooms[this->room_id].guest_leaves(this->id);
        }
        use_elevator(0); //go to floor 0 before leaving

        {
            std::lock_guard<std::mutex> writing_lock(mx_writing);
            mvwprintw(this->guest_window, 1, 16, "X");
            mvwprintw(this->guest_window, 1, 27, "X LEFT THE HOTEL                ");
            wrefresh(this->guest_window);
        }
        fill_progress_bar(this->progress_window, COLOR_PAIR(GUEST_C), 40, std::experimental::randint(2500, 3500));
    }

    void have_holiday()
    {
        while (true)
        {
            check_in();
            drink_coffee();
            relax_in_jacuzzi();
            leave_hotel();
        }
    }
};
#include <mutex>
#include <atomic>
#include <ncurses.h>
#include <thread>

const int RECEP_C = 1;
const int GUEST_C = 2;
const int CLEANER_C = 3;
const int ELEVATOR_C = 4;
const int WAITER_C = 5;
const int COFFEE_C = 6;
const int JACUZZI_C = 7;

std::mutex mx_writing;
std::atomic<bool> cancellation_token(false);

void fill_progress_bar(WINDOW *progress_window, int COL, int width, int sleep_time)
{
    {
        std::lock_guard<std::mutex> writing_lock(mx_writing);
        werase(progress_window);
        wattron(progress_window, COL);
        box(progress_window, 0, 0);
        wattroff(progress_window, COL);
    }
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
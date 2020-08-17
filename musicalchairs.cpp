/*
 * Program: Musical chairs game with n players and m intervals.
 * Author:  SANDEEP KUMAR, VIJAY REDDY
 * Roll# :  CS18BTECH11041,CS18BTECH11017
 */

#include <stdlib.h> /* for exit, atoi */
#include <iostream> /* for fprintf */
#include <errno.h>  /* for error code eg. E2BIG */
#include <getopt.h> /* for getopt */
#include <assert.h> /* for assert */
#include <chrono>   /* for timers */
#include <thread>
#include <mutex>
#include <sstream>
#include <condition_variable>
using namespace std;

int all_spawn; //Set to 1 once all player threads have been spawned
int n_searching; // n_searching is the number of players still searching for a seat in a particular lap
int winner; //Contains player id of winner
int laps_left; //Contains number of remaining laps. Always one less than players remaining.
int *ready; //Ready array is used to make sure players don't occupy multiple seats in the same lap
int *seat, *sleepers; //Seat array is used to see which seats are empty. sleepers array stores how much each player has to sleep 
mutex mtx, mtx1, mtx2, mtx3,mtx_sleep; //mtx is the main lock. mtx1, mtx2, mtx3 are mutexes for the condition varaibles to use.
std::condition_variable cv1, cv2, cv3; 

void usage(int argc, char *argv[]);
unsigned long long musical_chairs(int nplayers);

int main(int argc, char *argv[])
{
    int c;
    int nplayers = 0;

    /* Loop through each option (and its's arguments) and populate variables */
    while (1)
    {
        int this_option_optind = optind ? optind : 1;
        int option_index = 0;
        static struct option long_options[] = {
            {"help", no_argument, 0, 'h'},
            {"nplayers", required_argument, 0, '1'},
            {0, 0, 0, 0}};

        c = getopt_long(argc, argv, "h1:", long_options, &option_index);
        if (c == -1)
            break;

        switch (c)
        {
        case 0:
            cerr << "option " << long_options[option_index].name;
            if (optarg)
                cerr << " with arg " << optarg << endl;
            break;

        case '1':
            nplayers = atoi(optarg);
            break;

        case 'h':
        case '?':
            usage(argc, argv);

        default:
            cerr << "?? getopt returned character code 0%o ??n" << c << endl;
            usage(argc, argv);
        }
    }

    if (optind != argc)
    {
        cerr << "Unexpected arguments.\n";
        usage(argc, argv);
    }

    if (nplayers <= 0)
    {
        cerr << "Invalid nplayers argument." << endl;
        return EXIT_FAILURE;
    }

    unsigned long long game_time;
    game_time = musical_chairs(nplayers);

    cout << "Time taken for the game: " << game_time << " us" << endl;

    exit(EXIT_SUCCESS);
}

/*
 * Show usage of the program
 */
void usage(int argc, char *argv[])
{
    cerr << "Usage:\n";
    cerr << argv[0] << "--nplayers <n>" << endl;
    exit(EXIT_FAILURE);
}

void umpire_main(int nplayers)
{
    /* Add your code here */
    /* read stdin only from umpire */
    string instr;
    mtx.lock(); //Acquires main mutex lock
    laps_left = nplayers - 1;
    n_searching = nplayers; 

    std::unique_lock<std::mutex> lk1(mtx1);
    while (!all_spawn) //Checks if all players have been spawned
    {
        cv1.wait(lk1);
    }

    if (laps_left == 0) // Base case when only one player plays
    {
        winner = 0;
    }

    while (laps_left > 0)
    {
        getline(cin, instr);

        if (instr.compare("lap_start") == 0)
        {
            cout << "======= lap# " << nplayers - laps_left << " =======\n";
            n_searching = laps_left + 1; //Remaining laps is always one less than number of remaining players
            
            mtx_sleep.lock(); //Acquires lock to reset sleepers array and fill in with new sleep values 
            std::unique_lock<std::mutex> lk3(mtx3); //Acquires lock to ensure concurrency for players trying to see if they are ready or not
            for (int i = 0; i < nplayers; i++)
            {
                ready[i] = 1;    //Gets all players ready
                sleepers[i] = 0; // By default, no player sleeps
            }
            lk3.unlock();
            cv3.notify_all(); //Notifies busy-waiting players

            for (int i = 0; i < laps_left; i++) //Makes everyone get up from seat
            {
                seat[i] = 0; // 0 means vacant seat
            }

            getline(cin, instr);
            stringstream X(instr);
            string S;
            getline(X, S, ' ');

            if (S.compare("player_sleep") == 0)
            {
                int p_index, sleep_time;
                do
                {
                    getline(X, S, ' ');
                    p_index = stoi(S);
                    getline(X, S, ' ');
                    sleep_time = stoi(S);
                    sleepers[p_index] = sleep_time;//Sets player sleep value

                    getline(cin, instr);
                    X.str(instr);
                    X.clear();
                    getline(X, S, ' ');
                } while (S.compare("player_sleep") == 0);
            }
            

            if (instr.compare("music_start") == 0)
            {
                mtx_sleep.unlock();//Players are now allowed to start sleeping
                getline(cin, instr);
                X.clear();
                X.str(instr);
                getline(X, S, ' ');
                if (S.compare("umpire_sleep") == 0)
                {
                    int sleep_time;
                    getline(X, S, ' ');
                    sleep_time = stoi(S);
                    this_thread::sleep_for(chrono::microseconds(sleep_time)); //Umpire sleeps
                    getline(cin, instr);
                }

                if (instr.compare("music_stop") == 0)
                {
                    //Release lock, allow players to look for seats
                    mtx.unlock();
                    getline(cin, instr);
                    if (instr.compare("lap_stop") == 0)
                    {                    
                        std::unique_lock<std::mutex> lk2(mtx2);
                        while (n_searching != 0) //Busy wait till all players are done searching
                            cv2.wait(lk2);
                        mtx.lock(); //Regain main mtx lock
                        cout << "**********************\n";
                        
                    }
                }
            }
        }

        laps_left--;
    }
    cout << "Winner is " << winner << "\n";
    mtx.unlock();
    return;
}

void player_main(int plid)
{

    if (laps_left == 0) //Base case when only one player
    {
        return;
    }

    while (1)
    {
        std::unique_lock<std::mutex> lk3(mtx3); //Busy wait till start of a lap. Makse sure one player can't choose more than one seat
        while (!ready[plid])
            cv3.wait(lk3);
        lk3.unlock();

        mtx_sleep.lock(); //Waits till sleep values have been set by umpire
        if (sleepers[plid] != 0)
        {
            int sleep_time = sleepers[plid]; 
            mtx_sleep.unlock();

            this_thread::sleep_for(chrono::microseconds(sleep_time)); //Player sleeps right after music starts 
        }
        else
            mtx_sleep.unlock();
        int seat_found = 0;

        mtx.lock();//Acquire main mtx lock once music has been stopped and start searching for empty seat
        for (int i = 0; i < laps_left; i++) //laps_left is equal to seats left
        {
            if (seat[i] == 0) //Empty seat
            {
                seat[i] = 1; //Mark as occupied
                seat_found = 1;
                break;
            }
        }
        if (!seat_found) //No empty seat found, i.e this player is knocked out
        {
            cout << plid << " could not get chair\n";
            ready[plid] = 0; //Done searching in this lap

            std::unique_lock<std::mutex> lk2(mtx2); //Acquires lock to ensure concurrency between eliminated player and umpire checking whether all players are done searching
            n_searching--;
            lk2.unlock();
            cv2.notify_one();
            mtx.unlock(); //Releases main mtx lock for umpire
            return;
        }
        else
        {
            if (laps_left == 1) //All other players have died
            {
                winner = plid; 
                ready[plid] = 0;
                n_searching--; 
                mtx.unlock(); // Releases main mtx lock for umpire to print winner
                return;
            }
            ready[plid] = 0;
            n_searching--;
            mtx.unlock(); //Releases lock for other players to search
        }
    }
}

unsigned long long musical_chairs(int nplayers)
{
    auto t1 = chrono::steady_clock::now();
    cout << "Musical Chairs: " << nplayers << " player game with " << nplayers - 1 << " laps.\n";

    // Spawn umpire thread.
    /* Add your code here */
    mtx.lock(); //initializes all global variables
    seat = new int[nplayers - 1];
    sleepers = new int[nplayers];
    ready = new int[nplayers];
    all_spawn = 0;
    winner = -1;
    laps_left = -1;
    mtx.unlock();

    thread t_umpire(umpire_main, nplayers); //Spawn umpire

    thread *t_player = new thread[nplayers];
    for (int i = 0; i < nplayers; i++)
    {
        t_player[i] = thread(player_main, i);//Spawn players
    }

    std::unique_lock<std::mutex> lk1(mtx1);
    all_spawn = 1;
    lk1.unlock(); //Notify umpire that all player threads have been spawned
    cv1.notify_one();

    for (int i = 0; i < nplayers; i++)
    {
        t_player[i].join(); //Join all players
    }
    t_umpire.join();

    auto t2 = chrono::steady_clock::now();

    auto d1 = chrono::duration_cast<chrono::microseconds>(t2 - t1);

    return d1.count();
}
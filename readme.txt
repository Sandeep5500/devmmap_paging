Files included are musicalchairs.cpp, README.txt and report.pdf.
You have tor run g++ musicalchairs.cpp -o obj -pthread -O2 to compile the program.
To run the program, execute ./obj --nplayers <no of players> < input.txt
where input.txt contains all the required input commands.
Sample Output:
INPUT:
lap_start
player_sleep 0 1000
player_sleep 1 2000
player_sleep 2 3000
player_sleep 3 4000
music_start
umpire_sleep 500
music_stop
lap_stop
lap_start
player_sleep 0 1000
player_sleep 1 2000
player_sleep 2 3000
music_start
umpire_sleep 500
music_stop
lap_stop
lap_start
player_sleep 0 1000
player_sleep 1 2000
music_start
umpire_sleep 500
music_stop
lap_stop
COMMAND: ​./obj --nplayers 4 < input.txt
OUTPUT:
Musical Chairs: 4 player game with 3 laps.
======= lap# 1 =======
3 could not get chair
**********************
======= lap# 2 =======
2 could not get chair
**********************
======= lap# 3 =======
1 could not get chair
**********************
Winner is 0
Time taken for the game: 10353 us

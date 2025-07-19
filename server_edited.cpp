// edited version of server.cpp
/* changes made:
    - modified to 5 rounds
    - fixed scoring tally
		- fixed winner declaration
		- added ready up function
		- added ending message
    - added final score summary on client end
*/

#include <iostream>
#include <vector>
#include <thread>
#include <map>
#include <algorithm>
#include <random>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <barrier>
#include <latch>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_PLAYERS 4

using namespace std;

const string RESET     = "\033[0m";
const string RED       = "\033[31m";
const string GREEN     = "\033[32m";
const string YELLOW    = "\033[33m";
const string BLUE      = "\033[34m";
const string MAGENTA   = "\033[35m";
const string CYAN      = "\033[36m";
const string BOLD      = "\033[1m";
const string UNDERLINE = "\033[4m";
const string GREY      = "\033[38;5;250m";
 
struct Player {
    int socket;
    string name;
    vector<int> cards;
    int score = 0;
};

vector<Player> players;
vector<bool> readyFlags(MAX_PLAYERS, false);
vector<int> deck;
mutex playerMutex;
mutex printMutex;
int roundNo = 1;
barrier gameBarrier(MAX_PLAYERS);
latch gameStart(MAX_PLAYERS);
latch gameLatch(MAX_PLAYERS);
latch gameEnd(1);

string cts(int val) {
    if (val <= 10) return to_string(val);
    if (val == 11) return "JACK";
    if (val == 12) return "QUEEN";
    if (val == 13) return "KING";
    if (val == 14) return "ACE";
    return "??";
}

string playerColor(int num){
    switch(num){
        case 0: return BLUE; 
        case 1: return MAGENTA; 
        case 2: return CYAN; 
        case 3: return YELLOW;
        default: return RESET; 
    }
}

void broadcast(const string& msg)
{
    lock_guard<mutex> lock(printMutex);
    for (auto& p : players) {
        send(p.socket, msg.c_str(), msg.size(), 0);
    }
}

barrier roundBarrier(MAX_PLAYERS, [](){  //called before each round
    string roundMsg;
    
    roundMsg = BOLD + "[ROUND " + to_string(roundNo) + " END!]\n" + RESET;
    cout<<roundMsg;
    broadcast(roundMsg);

    int maxCard = -1;
    vector<int> winners;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        int lastCard = players[i].cards.back();
        if (lastCard > maxCard) {
            maxCard = lastCard;
            winners = { i };
        } else if (lastCard == maxCard) {
            winners.push_back(i);
        }
    }

    string winMsg;
    if (winners.size() == 1) {
        players[winners[0]].score += 1;
        winMsg = GREEN + BOLD + players[winners[0]].name + " wins this round!\n" + RESET;
    } else {
        winMsg = GREEN + BOLD + "It's a tie between: ";
        for (int idx : winners) {
            winMsg += players[idx].name + " ";
        }
        winMsg += "\n" + RESET;
    }
    broadcast(winMsg);
    cout << winMsg;
    
    this_thread::sleep_for(chrono::seconds(1));
  

    cout<<GREY<<BOLD<<"Current players' cards: "<<RESET<<endl;

    for(int i = 0; i < MAX_PLAYERS; i++){
        string msg = playerColor(i) + players[i].name + "'s Cards: \n" + RESET;
        cout<<msg;
        send(players[i].socket, msg.c_str(), msg.size(), 0);
        string cards = "";
        for(auto& c : players[i].cards){
            cards += cts(c) + " ";
        }
        send(players[i].socket, cards.c_str(), cards.size(), 0);
        cout<<cards;
        cout<<"\n"<<RESET;
        this_thread::sleep_for(chrono::seconds(1));
    }
    cout<<"\n";

    roundNo++;
    
    
    if(roundNo <= 3){
        roundMsg = BOLD + "[ROUND " + to_string(roundNo) + " START!]\n" + RESET;
        cout<<roundMsg;
        broadcast(roundMsg);
    }
});

void generateCards() {
    for (int val = 2; val <= 14; ++val)
        for (int suit = 1; suit <= 4; ++suit)
            deck.push_back(val);

    shuffle(deck.begin(), deck.end(), mt19937{random_device{}()});
}

void handle_player(int socket, int id) {
    Player& p = players[id];
    p.socket = socket;
    int card;
    
    char buffer[BUFFER_SIZE]{};
    int bytes = recv(p.socket, buffer, BUFFER_SIZE - 1, 0);  // leave room for '\0'
    if (bytes <= 0) {
        close(p.socket);
        return;
    }
    buffer[bytes] = '\0';  // Null-terminate it
    p.name = buffer;
    
    string joinMsg = p.name + " has joined the game.\n";
    broadcast(joinMsg);
    cout << joinMsg;

    // for ready up
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes = recv(p.socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0) {
            close(p.socket);
            return;
        }
        buffer[bytes] = '\0';

        string input(buffer);
        if (input == "1") {
            {
                lock_guard<mutex> lock(playerMutex);
                readyFlags[id] = true;
            }
            string msg = GREEN + p.name + " is ready!\n" + RESET;
            broadcast(msg);
            cout << msg;
            break;
        } else {
            string msg = RED + "Please press 1 to ready up.\n" + RESET;
            send(p.socket, msg.c_str(), msg.size(), 0);
        }
    }
    
    gameStart.arrive_and_wait();
  
    while (roundNo <= 5) {
        {
            lock_guard<mutex> lock(playerMutex);
            
            string drawMsg;
            card = deck.back();
            deck.pop_back();
            p.cards.push_back(card);
            
            drawMsg = "[ROUND " + to_string(roundNo) + "] " + playerColor(id) + p.name  + " is drawing a card...\n" + RESET;
            broadcast(drawMsg);
            cout<<drawMsg;
            
            cout<<"[ROUND " + to_string(roundNo) + "] " + playerColor(id) + p.name + " drew " + cts(card) + RESET + "\n";
            drawMsg = "[ROUND " + to_string(roundNo) + "] " + playerColor(id) + "You drew " + cts(card) + RESET + "\n";
            send(p.socket, drawMsg.c_str(), drawMsg.size(), 0);
        }

        roundBarrier.arrive_and_wait();
    }
    gameLatch.arrive_and_wait(); //ADDING SCORES TIME
 
    gameEnd.wait();
}

void game_server() {
    vector<thread> playerThreads;
    
    cout << GREY << "Preparing game setup...\n" << RESET << endl;
    this_thread::sleep_for(chrono::milliseconds(300)); //simulate delay for card generation
    generateCards();

    //CONNECTING
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) { perror("socket"); return; }
    
    /* Allow fast restart (optional) */
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in server_addr{};
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(PORT);
    
    if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind"); return;
    }
    if (listen(server_socket, MAX_PLAYERS) < 0) {
        perror("listen"); return;
    }
    std::cout << "Server listening on port " << PORT << ", waiting for players...\n";

    int connected_players = 0;
    players.resize(MAX_PLAYERS);
    while (connected_players < MAX_PLAYERS) {
        int client_socket = accept(server_socket, nullptr, nullptr);
        if (client_socket < 0) {
            perror("accept");
            continue;
        }

        playerThreads.emplace_back(handle_player, client_socket, connected_players);
        ++connected_players;
    }

    // Wait until all players are ready
    while (true) {
        {
            lock_guard<mutex> lock(playerMutex);
            if (all_of(readyFlags.begin(), readyFlags.end(), [](bool b) { return b; })) {
                break;
            }
        }
        this_thread::sleep_for(chrono::milliseconds(100));
    }

    gameStart.wait();

    string roundMsg = BOLD + "[ROUND 1 START!]\n" + RESET;
    broadcast(roundMsg);
    cout << roundMsg;
    
    gameLatch.wait();	
    string scoreMsg = GREY + "\nAll rounds finished. Calculating winner...\n" + RESET;
    broadcast(scoreMsg);
    cout<<scoreMsg;
    
    

    // Determine winner
    int maxScore = 0;
    for (const auto& p : players) {
        cout << p.name << ": " << p.score << " pts\n";
        maxScore = max(maxScore, p.score);
    }

    // Gather winners
    vector<string> winners;
    for (const auto& p : players) {
        if (p.score == maxScore)
            winners.push_back(p.name);
    }

    // final score summary on client end
    string summary = "\n===== FINAL SCORES =====\n";
    for (const auto& p : players) {
        summary += p.name + ": " + to_string(p.score) + " pts\n";
    }
    broadcast(summary);

    // winner declaration
    string result;
    if (winners.size() == 1) {
        result = GREEN + BOLD + "And the winner is " + winners[0] + " with a score of " + to_string(maxScore) + " !\n";
    } else {
        result = YELLOW + BOLD + "It's a tie between ";
        for (size_t i = 0; i < winners.size(); ++i) {
            result += winners[i];
            if (i < winners.size() - 1) result += ", ";
        }
        result += " — each with a score of " + to_string(maxScore) + "!\n";
    }
    result += RESET;
    broadcast(result);
    cout << result;

    // game ending message
    string endMsg = "\nDisconnecting from the server in 10 seconds. Thank you for playing!";
    broadcast(endMsg);
    cout << endMsg << endl;
    this_thread::sleep_for(chrono::seconds(10));
        
    gameEnd.arrive_and_wait();
    
    for (auto& t : playerThreads) {
        if (t.joinable()) t.join();
    }

    for (auto& p : players) close(p.socket);
    close(server_socket);
     
}
int main() {
    players.clear();
    deck.clear();
    roundNo = 1;
    game_server();
    return 0;
}

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
#define MAX_PLAYERS 2

//COLORS
#define RESET       "\033[0m"
#define RED         "\033[31m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define BLUE        "\033[34m"
#define MAGENTA     "\033[35m"
#define CYAN        "\033[36m"
#define BOLD        "\033[1m"
#define UNDERLINE   "\033[4m"
#define GREY        "\033[38;5;250m"

using namespace std;

struct Player {
    int socket;
    string name;
    vector<int> cards;
};

vector<Player> players;
vector<int> deck;
mutex playerMutex;
mutex cardMutex;
int roundNo = 1;
latch gameStart(MAX_PLAYERS);


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
        case 1: return BLUE; 
        case 2: return MAGENTA; 
        case 3: return CYAN; 
        case 4: return YELLOW;
        default: return RESET; 
    }
}

barrier roundBarrier(MAX_PLAYERS, [](){//called before each round
    cout<<BOLD<<"[ROUND "<<roundNo<<" END!]\n"<<RESET<<endl;
    this_thread::sleep_for(chrono::seconds(2));


    cout<<GREY<<BOLD<<"Current players' cards: "<<RESET<<endl;

    for(int i = 0; i < MAX_PLAYERS; i++){
        cout<<playerColor(i+1)<<players[i].name<<"'s Cards: "<<endl;
        for(auto& c : players[i].cards){
            cout<<cts(c)<<" ";
        }
        cout<<"\n"<<RESET;
        this_thread::sleep_for(chrono::seconds(1));
    }
    cout<<"\n";

    roundNo++;

    if(roundNo <= 3){
        cout<<BOLD<<"[ROUND "<<roundNo<<" START!]"<<RESET<<endl;
    }else if(roundNo > 3){
        cout<<"REACHED MAX\n";
    }
});

void generateCards() {
    for (int val = 2; val <= 14; ++val)
        for (int suit = 1; suit <= 4; ++suit)
            deck.push_back(val);

    shuffle(deck.begin(), deck.end(), mt19937{random_device{}()});
}

void broadcast(const string& msg)
{
    cout<<"Thread "<<this_thread::get_id()<<" is in broadcast"<<endl;;
    lock_guard<mutex> lock(playerMutex);
    for (auto& p : players) {
        send(p.socket, msg.c_str(), msg.size(), 0);
    }
}

void handle_player(Player& p) {
    int card;
    
    char buffer[BUFFER_SIZE]{};
    
    /* Get client name */
    if (recv(p.socket, buffer, BUFFER_SIZE, 0) <= 0) {
        close(p.socket);
        return;
    }
    p.name = buffer;
    
    string joinMsg = p.name + " has joined the game.\n";
    broadcast(joinMsg);
    cout << joinMsg;
    
    cout << "Thread " << p.name << " reached gameStart latch.\n";
    gameStart.arrive_and_wait();
    cout << "Thread " << p.name << " passed gameStart latch.\n";
    
    if(p.name == players[0].name){
        cout << "Thread " << p.name << " reached rounds\n";
        string r1= "[ROUND 1 STARTING]\n";
        broadcast(r1);
        cout<<r1;
    }
  
    while (roundNo <= 3) {
        {
            cout << "Thread " << p.name << " reached while loop\n";
            lock_guard<mutex> lock(cardMutex);
            cout << "Thread " << p.name << " acquired lock\n";
            //memset(buffer, 0, BUFFER_SIZE);

            // Prompt player
            //string prompt = "[Round " + to_string(roundNo) + "] " + p.name + ", press any key to draw:\n";
            //send(p.socket, prompt.c_str(), prompt.size(), 0);

            // Wait for any input
            //if (recv(p.socket, buffer, BUFFER_SIZE, 0) <= 0) {
            //    close(p.socket);
            //    return;
            //}
            
            card = deck.back();
            deck.pop_back();
            p.cards.push_back(card);

            string drawMsg = "[Round " + to_string(roundNo) + "] " + p.name + " drew " + cts(card) + "\n" + RESET;
            broadcast(drawMsg);
            cout<<drawMsg<<endl;
            cout << "Thread " << p.name << " release lock\n";
        }

        roundBarrier.arrive_and_wait();
    }
    return;
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

    while (players.size() < MAX_PLAYERS) {
        cout<<"Creating player threads..."<<endl;;
        int client_socket = accept(server_socket, nullptr, nullptr);
        lock_guard<mutex> lock(playerMutex);
        players.push_back({client_socket});
        playerThreads.emplace_back(handle_player, players.size() - 1);
    }

    
    for (auto& t : playerThreads) {
        if (t.joinable()) t.join();
    }

    cout<<"\nAll rounds finished. Calculating winner...\n";
    broadcast("\nAll rounds finished. Calculating winner...\n");

    // Determine winner
    map<string, int> scores;
    for (auto& p : players) {
        int high = *max_element(p.cards.begin(), p.cards.end());
        scores[p.name] = high;
    }

    auto winner = max_element(scores.begin(), scores.end(),
        [](auto& a, auto& b) { return a.second < b.second; });

    string result = "Winner: " + winner->first + " with card " + cts(winner->second) + "\n";
    broadcast(result);
    cout << result;

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


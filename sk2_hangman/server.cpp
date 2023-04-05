#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <mutex>

const int max_clients = 5;
const int max_tries = 7;
const int port = 8080;
const int buffer_size = 1024;

std::vector<std::string> playerNicknames;
std::mutex nicknameMutex;

std::vector<std::string> words;

// Reading words from file
std::vector<std::string> readWordsFromFile(std::string filename)
{
    std::vector<std::string> words;
    std::ifstream file(filename);
    std::string line;
    while(getline(file, line))
    {
        words.push_back(line);
    }
    file.close();
    return words;
}

std::string chooseWord(std::vector<std::string> words)
{
    srand(time(NULL));
    return words[rand() % words.size()];
}

std::string hideWord(std::string word)
{
    std::string hiddenWord(word.length(), '_');
    return hiddenWord;
}

void handleClient(int client_socket)
{
    char buffer[buffer_size] = {0};

    //Asking the player to enter their nickname
    std::string nn = "Enter your nickname: ";
    int read_value = recv(client_socket, buffer, buffer_size, 0);
    std::string nickname(buffer, read_value);
    std::cout << "Connection accepted.\n";

    // Checking if the name is unique
    // using mutex to prevent from overreading data 
    // during different threads
    nicknameMutex.lock();
    while (find(playerNicknames.begin(), playerNicknames.end(), nickname) != playerNicknames.end())
    {
        std::cout << "Nickname " << nickname << "Already taken.\n";
        std::string nickname_taken = "Nickname already taken. Enter a different nickname: ";
        send(client_socket, nickname_taken.c_str(), nickname_taken.length(), 0);
        read_value = recv(client_socket, buffer, buffer_size, 0);
        nickname = std::string(buffer, read_value);
    }

    // Adding the player's nickname to the list of nicknames
    playerNicknames.push_back(nickname);
    nicknameMutex.unlock();

    // Choose a word for the game
    std::string word = chooseWord(words);
    std::string hiddenWord = hideWord(word);

    // Play the game
    int numTries = 0;
    std::string guessedLetters;

    while(numTries < max_tries && hiddenWord != word)
    {
        // Current status
        std::string message = "Current word: " + hiddenWord + "\nGuessed letters: " + guessedLetters;
        send(client_socket, message.c_str(), message.length(), 0);

        // Receive a letter from the player
        read_value = recv(client_socket, buffer, buffer_size, 0);
        char letter = buffer[0];

        // Check if the letter has already been guessed
        if(guessedLetters.find(letter) != std::string::npos)
        {
            std::string mess = "You already guessed that letter.\n";
            send(client_socket, mess.c_str(), mess.length(), 0);
        }
        else
        {
            // Add the letter to guessed
            guessedLetters += letter;

            // Check if the letter is in the word
            if(word.find(letter) != std::string::npos)
            {
                for(int i = 0; i < word.length(); i++)
                {
                    if (word[i] == letter)
                    {
                        hiddenWord[i] = letter;
                        if (hiddenWord == word)
                        {
                            break;
                        }
                    }
                }

            }
            else
            {
                // Increment tries
                numTries++;
            }
        }
    }
    // Sending the final sttatust of the game to the player
    if(numTries == max_tries)
    {
        std::string lost = "You lost. The word was: " + word + ".";
        send(client_socket, lost.c_str(), lost.length(), 0);
    }
    else
    {
        std::string win = "You won! The word was: " + word;
        send(client_socket, win.c_str(), win.length(), 0);
    }

    playerNicknames.erase(find(playerNicknames.begin(), playerNicknames.end(), nickname));

    
    close(client_socket);
}

int main()
{
    int server_fd, new_socket, read_value;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};

    // Read words from file
    words = readWordsFromFile("words.txt");

    // Create socket
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Attach socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind socket to the address and port
    if(bind(server_fd, (struct sockaddr *)&address, sizeof(address)))
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    // Start listening for incoming connections
    if(listen(server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    std::vector<int> sockets;

    while (true)
    {
        std::cout << "Waiting for connection...\n";

        // Accept new connection
        if((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0)
        {
            perror("accept failed");
            exit(EXIT_FAILURE);
        }

        sockets.push_back(new_socket);

        // create new thread to handle the client
        std::thread t(handleClient, new_socket);
        t.detach();
    }
    return 0;
}
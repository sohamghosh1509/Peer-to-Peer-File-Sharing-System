#include <iostream>
#include <stdlib.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <bits/stdc++.h>
#define CHUNK_SIZE 512*1024

using namespace std;

unordered_map<string, vector<string>> download_info;
unordered_map<string, string> file_path;

void send_file(int client_socket, string filepath)
{

    int fd_from = open(filepath.c_str(), O_RDONLY);
    int file_size = lseek(fd_from, 0, SEEK_END);
    int num = file_size/CHUNK_SIZE;
    if(file_size % CHUNK_SIZE != 0)
        num++;
    char buffer[CHUNK_SIZE];
    int chunk = 1;
    int rem_size = file_size;

    lseek(fd_from, 0, SEEK_SET);

    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        int r;
        if(rem_size < CHUNK_SIZE)
            r = read(fd_from, buffer, rem_size);
        else
            r = read(fd_from, buffer, CHUNK_SIZE);
        if (r <= 0)
        {
            cout << "Error in reading file" << endl;
            break;
        }
        chunk++;
        send(client_socket, buffer, r, 0);
        if(chunk == num)
            break;
    }
}

bool receive_file(string filename, string destpath, int client_socket)
{
    string f_path = destpath + "/" + filename;
    int fd_to = open(f_path.c_str(), O_WRONLY | O_CREAT, 0777);
    if (fd_to < 0)
    {
        cout << "Error in opening file" << endl;
        return false;
    }
    char buffer[CHUNK_SIZE];
    cout << "Downloading file..." << endl;
    int total_bytes = 0;

    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        int n = recv(client_socket, buffer, CHUNK_SIZE, 0);
        if (n < CHUNK_SIZE)
        {
            if (n <= 0)
                break;
            else
            {
                write(fd_to, buffer, n);
                total_bytes += n;
                break;
            }
        }
        total_bytes += n;

        int r = write(fd_to, buffer, n);
        if (r < 0)
        {
            cout << "Error in writing file" << endl;
            return false;
        }
    }
    cout << "Downloaded file successfully! Total bytes = " << total_bytes << "\n\n";
    return true;
}

string getFileName(string filepath)
{
    string filename;
    size_t found;
    found = filepath.find_last_of('/');
    if (found != string::npos)
        filename = filepath.substr(found + 1);
    else
        filename = filepath;
    return filename;
}

void handleConn(int clientSocket)
{
    string server_response, user_id = "";

    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    recv(clientSocket, buffer, sizeof(buffer), 0);

    send_file(clientSocket, file_path[buffer]);
    close(clientSocket);
}

void handlePeerConn(int listenPort)
{
    int clientServerSocket, newSocket;
    struct sockaddr_in clientServerAddr, clientAddr;
    socklen_t addr_size = sizeof(clientAddr);
    
    vector<thread> threadVec;

    clientServerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientServerSocket == -1)
    {
        cerr << "Error creating socket" << endl;
    }

    clientServerAddr.sin_family = AF_INET;
    clientServerAddr.sin_port = htons(listenPort);
    clientServerAddr.sin_addr.s_addr = INADDR_ANY;


    if (bind(clientServerSocket, (struct sockaddr *)&clientServerAddr, sizeof(clientServerAddr)) == -1)
    {
        cerr << "Error binding to address" << endl;
    }

    if (listen(clientServerSocket, 50) == 0)
    {
        while (1)
        {
            newSocket = accept(clientServerSocket, (struct sockaddr *)&clientServerAddr, &addr_size);
            if (newSocket == -1)
            {
                cerr << "Error accepting connection" << std::endl;
                continue;
            }

            threadVec.push_back(thread(handleConn, newSocket));
        }
    }
    for (int i = 0; i < threadVec.size(); i++)
    {
        if (threadVec[i].joinable())
            threadVec[i].join();
    }
}

int main(int argc, char *argv[])
{
    string IpPort = argv[1];
    int pos = IpPort.find(":");
    int clientListenPort = atoi(IpPort.substr(pos+1).data());
    
    thread t(handlePeerConn, clientListenPort);

    string tracker_info_path = argv[2];

    
    int clientSocket;
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1)
    {
        cerr << "Error creating socket" << endl;
        return 1;
    }

    struct sockaddr_in trackerAddress;
    trackerAddress.sin_family = AF_INET;
    trackerAddress.sin_port = htons(8989);
    trackerAddress.sin_addr.s_addr = inet_addr("127.0.0.1");

    int connectionStatus = connect(clientSocket, (struct sockaddr *)&trackerAddress, sizeof(trackerAddress));
    if (connectionStatus == -1)
    {
        cerr << "Error connecting to the tracker" << endl;
        close(clientSocket);
        return 1;
    }

    cout << "Connection established" << endl;

    while (1)
    {
        string request, response;
        char buffer[1024];
        cout << "Enter request:" << endl;
        getline(cin, request);

        if (request.substr(0, 11) == "upload_file")
        {
            string rest = request.substr(12);
            size_t pos = rest.find(" ");
            string filepath = rest.substr(0, pos);
            string filename = getFileName(filepath);
            file_path[filename] = filepath;
        }

        else if(request.substr(0,5) == "login")
        {
            request = request + " " + to_string(clientListenPort);
        }

        send(clientSocket, request.c_str(), request.size(), 0);
        memset(buffer, 0, sizeof(buffer));
        recv(clientSocket, buffer, sizeof(buffer), 0);
        response = buffer;

        if ((request.substr(0, 13) == "download_file") &&
            (response != "Group does not exist") &&
            (response != "User not part of the group") &&
            (response != "File not part of the group") &&
            (response != "Login first to download the file") &&
            (response != "Invalid request"))
        {
            if (response.empty())
                cout << "No clients available to share the file. \n";
            else
            {
                string rest = request.substr(14);
                vector<string> tokens;
                int n = rest.size();
                string str = "";
                for (int i = 0; i < n; i++)
                {
                    if (rest[i] == ' ')
                    {
                        tokens.push_back(str);
                        str = "";
                    }
                    else
                        str += rest[i];
                }
                tokens.push_back(str);
                string groupid = tokens[0];
                string filename = tokens[1];
                string destpath = tokens[2];
                string port = "";
                n = response.size();
                for (int i = 0; i < n; i++)
                {
                    int flag = 0;
                    if (response[i] == ' ')
                    {
                        int clientServerSocket;
                        clientServerSocket = socket(AF_INET, SOCK_STREAM, 0);

                        int port_no = atoi(port.data());
                        struct sockaddr_in serverAddress;
                        serverAddress.sin_family = AF_INET;
                        serverAddress.sin_port = htons(port_no);
                        serverAddress.sin_addr.s_addr = INADDR_ANY;
                        int connectedPeer = connect(clientServerSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress));

                        if (connectedPeer != -1)
                        {
                            send(clientServerSocket, filename.c_str(), filename.size(), 0);
                            if (receive_file(filename, destpath, clientSocket))
                            {
                                request = "downloaded " + filename + " " + groupid;
                                send(clientSocket, request.c_str(), request.size(), 0);
                                memset(buffer, 0, sizeof(buffer));
                                recv(clientSocket, buffer, sizeof(buffer), 0);
                                response = buffer;
                                flag = 1;
                            }
                        }
                        else
                            cout << "Error accepting connection! \n";
                        port = "";
                    }
                    else
                        port += response[i];
                    if (flag == 1)
                        break;
                }
            }
        }
        cout << "Response from tracker: " << response << endl;
        cout << "-------------\n";
    }

    close(clientSocket);

    return 0;
}

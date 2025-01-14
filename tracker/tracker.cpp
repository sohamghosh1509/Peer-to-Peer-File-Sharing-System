#include <iostream>
#include <string>
#include <thread>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <bits/stdc++.h>

using namespace std;

unordered_map<string, string> users;
unordered_map<string, int> users_loggedIn;
unordered_map<string, string> userPort;
unordered_map<string, vector<string>> groups;
unordered_map<string, unordered_set<string>> pending;
unordered_map<string, vector<string>> group_files;
unordered_map<string, unordered_map<string, vector<string>>> file_info;

std::mutex mutex;

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

int lengthRequest(string request)
{
    int n = request.length();
    int ans = 0;
    for(int i = 0;i<n;i++)
    {
        if(request[i] == ' ')
            ans++;
    }
    return ans;
}

void handleClient(int clientSocket)
{
    char buffer[1024];
    string user = "";
    int login = 0;
    string response = "";
    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead > 0)
        {
            string request(buffer);

            if (request.substr(0, 11) == "create_user")
            {
                // cout << "create_user is sent from client " << clientSocket << endl;
                string rest = request.substr(12);
                size_t pos = rest.find(" ");

                string userid = rest.substr(0, pos);
                string password = rest.substr(pos + 1);

                if (users.find(userid) == users.end())
                {
                    users[userid] = password;
                    users_loggedIn[userid] = 0;
                    response = "User created successfully\n";
                    send(clientSocket, response.c_str(), response.size(), 0);
                }
                else
                {
                    response = "User already created\n";
                    send(clientSocket, response.c_str(), response.size(), 0);
                }
            }
            else if (request.substr(0, 5) == "login")
            {
                // cout << "login is sent from client " << clientSocket << endl;
                string rest = request.substr(6);
                size_t pos = rest.find(" ");
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

                string userid = tokens[0];
                string password = tokens[1];
                string port = tokens[2];

                if (users_loggedIn[userid] == 1)
                {
                    response = "User already logged in\n";
                    send(clientSocket, response.c_str(), response.size(), 0);
                }

                if (users.find(userid) != users.end() && users[userid] == password)
                {
                    users_loggedIn[userid] = 1;
                    login = 1;
                    user = userid;
                    userPort[userid] = port;
                    response = "User logged in successfully\n";
                    send(clientSocket, response.c_str(), response.size(), 0);
                }
                else
                {
                    response = "User does not exist\n";
                    send(clientSocket, response.c_str(), response.size(), 0);
                }
            }
            else if (request == "logout")
            {
                // cout << "logout is sent from client " << clientSocket << endl;
                if(login == 0)
                {
                    response = "Login first to logout\n";
                    send(clientSocket, response.c_str(), response.size(), 0);
                    continue;
                }

                users_loggedIn[user] = 0;
                user = "";
                login = 0;

                response = "Logout successful\n";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
            else if (request.substr(0, 12) == "create_group")
            {
                // cout << "create_group is sent from client " << clientSocket << endl;

                if (login == 0)
                {
                    response = "Login first to create group\n";
                    send(clientSocket, response.c_str(), response.size(), 0);
                }
                else
                {
                    string groupid = request.substr(13);
                    if (groups.find(groupid) == groups.end())
                    {
                        groups[groupid].push_back(user);
                        response = "Group created successfully by owner " + user + "\n";
                        send(clientSocket, response.c_str(), response.size(), 0);
                    }
                    else
                    {
                        response = "Group already exists\n";
                        send(clientSocket, response.c_str(), response.size(), 0);
                    }
                }
            }
            else if (request.substr(0, 11) == "list_groups")
            {
                if (login == 0)
                {
                    response = "Login first to see list of groups\n";
                    send(clientSocket, response.c_str(), response.size(), 0);
                }
                
                string response = "List of groups:\n";
                for (auto group : groups)
                    response += group.first + "\n";

                send(clientSocket, response.c_str(), response.size(), 0);
            }
            else if (request.substr(0, 10) == "join_group")
            {
                // cout << "join_group is sent from client " << clientSocket << endl;
                if (login == 0)
                {
                    response = "Login first to join group\n";
                    send(clientSocket, response.c_str(), response.size(), 0);
                }
                else
                {
                    string groupid = request.substr(11);
                    if (groups.find(groupid) != groups.end())
                    {
                        vector<string> members = groups[groupid];
                        auto it = find(members.begin(), members.end(), user);
                        if (it == members.end())
                        {
                            pending[groupid].insert(user);
                            response = "Join group request made successfully\n";
                            send(clientSocket, response.c_str(), response.size(), 0);
                        }
                        else
                        {
                            response = "User is part of the group already\n";
                            send(clientSocket, response.c_str(), response.size(), 0);
                        }
                    }
                    else
                    {
                        response = "Group does not exist\n";
                        send(clientSocket, response.c_str(), response.size(), 0);
                    }
                }
            }
            else if (request.substr(0, 11) == "leave_group")
            {
                if (login == 0)
                {
                    response = "Login first to leave group\n";
                    send(clientSocket, response.c_str(), response.size(), 0);
                }
                else
                {
                    string groupid = request.substr(12);
                    if (groups.find(groupid) != groups.end())
                    {
                        vector<string> members = groups[groupid];
                        auto it = find(members.begin(), members.end(), user);
                        if (it != members.end())
                        {
                            groups[groupid].erase(it);
                            if (groups[groupid].empty())
                                groups.erase(groupid);

                            response = "Group left successfully by " + user + "\n";
                            send(clientSocket, response.c_str(), response.size(), 0);
                        }
                        else
                        {
                            response = "User not part of the group\n";
                            send(clientSocket, response.c_str(), response.size(), 0);
                        }
                    }
                    else
                    {
                        response = "Group does not exist";
                        send(clientSocket, response.c_str(), response.size(), 0);
                    }
                }
            }
            else if (request.substr(0, 13) == "list_requests")
            {
                string grpid = request.substr(14);

                if(login == 0)
                {
                    response = "Login first to see list of requests\n";
                    send(clientSocket, response.c_str(), response.size(), 0);
                    continue;
                }

                if (groups.find(grpid) != groups.end())
                {
                    if (!groups[grpid].empty() && groups[grpid][0] == user)
                    {
                        if (pending.find(grpid) != pending.end())
                        {
                            unordered_set<string> join_requests = pending[grpid];
                            response = "List of pending requests:\n";
                            for (string clients : join_requests)
                                response += clients + "\n";

                            send(clientSocket, response.c_str(), response.size(), 0);
                        }
                        else
                        {
                            response = "No pending requests\n";
                            send(clientSocket, response.c_str(), response.size(), 0);
                        }
                    }
                    else
                    {
                        response = "User not owner of group\n";
                        send(clientSocket, response.c_str(), response.size(), 0);
                    }
                }
                else
                {
                    response = "Group does not exist\n";
                    send(clientSocket, response.c_str(), response.size(), 0);
                }
            }
            else if (request.substr(0, 14) == "accept_request")
            {
                if(login == 0)
                {
                    response = "Login first to accept request\n";
                    send(clientSocket, response.c_str(), response.size(), 0);
                    continue;
                }
                
                string rest = request.substr(15);
                size_t pos = rest.find(" ");
                string groupid = rest.substr(0, pos);
                string userid = rest.substr(pos + 1);

                if (pending.find(groupid) != pending.end())
                {
                    if (!groups[groupid].empty() && groups[groupid][0] == user)
                    {
                        unordered_set<string> requests = pending[groupid];

                        if (requests.find(userid) != requests.end())
                        {
                            groups[groupid].push_back(userid);
                            pending[groupid].erase(userid);

                            if (pending[groupid].empty())
                                pending.erase(groupid);

                            response = "User has accepted request of " + userid + " to join group";
                            send(clientSocket, response.c_str(), response.size(), 0);
                        }
                        else
                        {
                            response = "No request has been made by " + userid;
                            send(clientSocket, response.c_str(), response.size(), 0);
                        }
                    }
                    else
                    {
                        response = "User not owner of group:\n";
                        send(clientSocket, response.c_str(), response.size(), 0);
                    }
                }
                else
                {
                    response = "Group does not exist in pending requests\n";
                    send(clientSocket, response.c_str(), response.size(), 0);
                }
            }
            else if (request.substr(0, 11) == "upload_file")
            {
                if(login == 0)
                {
                    response = "Login first to upload file\n";
                    send(clientSocket, response.c_str(), response.size(), 0);
                    continue;
                }

                string rest = request.substr(12);
                size_t pos = rest.find(" ");
                string filepath = rest.substr(0, pos);
                string groupid = rest.substr(pos + 1);

                if (groups.find(groupid) == groups.end())
                {
                    response = "Group does not exist\n";
                    send(clientSocket, response.c_str(), response.size(), 0);
                }
                else
                {
                    vector<string> members = groups[groupid];
                    auto it = find(members.begin(), members.end(), user);
                    if (it == members.end())
                    {
                        response = "User not part of the group\n";
                        send(clientSocket, response.c_str(), response.size(), 0);
                    }
                    else
                    {
                        string filename = getFileName(filepath);
                        group_files[groupid].push_back(filename);
                        file_info[groupid][filename].push_back(user);
                        response = "File uploaded successfully\n";
                        send(clientSocket, response.c_str(), response.size(), 0);
                    }
                }
            }
            else if (request.substr(0, 10) == "list_files")
            {
                if(login == 0)
                {
                    response = "Login first to see list of files\n";
                    send(clientSocket, response.c_str(), response.size(), 0);
                    continue;
                }
                
                string groupid = request.substr(11);
                if (groups.find(groupid) == groups.end())
                {
                    response = "Group does not exist\n";
                    send(clientSocket, response.c_str(), response.size(), 0);
                }
                else
                {
                    vector<string> files = group_files[groupid];
                    response = "List of files sharable:\n";
                    for (auto file : files)
                        response += file + "\n";
                    send(clientSocket, response.c_str(), response.size(), 0);
                }
            }
            else if (request.substr(0, 13) == "download_file")
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
                if (groups.find(groupid) == groups.end())
                {
                    response = "Group does not exist\n";
                    send(clientSocket, response.c_str(), response.size(), 0);
                }
                else
                {
                    vector<string> members = groups[groupid];
                    auto it = find(members.begin(), members.end(), user);
                    if (it == members.end())
                    {
                        response = "User not part of the group\n";
                        send(clientSocket, response.c_str(), response.size(), 0);
                    }
                    else
                    {
                        vector<string> files = group_files[groupid];
                        auto it = find(files.begin(), files.end(), filename);
                        if (it == files.end())
                        {
                            response = "File not part of the group\n";
                            send(clientSocket, response.c_str(), response.size(), 0);
                        }
                        else
                        {
                            response = "";
                            vector<string> users = file_info[groupid][filename];
                            for (string user : users)
                            {
                                if (users_loggedIn.find(user) != users_loggedIn.end())
                                    response += userPort[user] + " ";
                            }
                            send(clientSocket, response.c_str(), response.size(), 0);
                        }
                    }
                }
            }
            else if (request.substr(0, 10) == "downloaded")
            {
                string rest = request.substr(11);
                size_t pos = rest.find(" ");
                string filename = rest.substr(0, pos);
                string groupid = rest.substr(pos + 1);
                file_info[groupid][filename].push_back(user);
                response = "Downloaded file successfully";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
            else
            {
                response = "Invalid request\n";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }
    }
    close(clientSocket);
}

int main(int argc, char *argv[])
{
    int trackerSocket, newSocket;
    struct sockaddr_in trackerAddr, clientAddr;
    socklen_t addr_size = sizeof(clientAddr);

    vector<thread> threadVec;
    string tracker_info_path = argv[1]; 
    int trackerNum = atoi(argv[2]);

    trackerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (trackerSocket == -1)
    {
        cerr << "Error creating socket" << endl;
        return 1;
    }

    trackerAddr.sin_addr.s_addr = INADDR_ANY;
    trackerAddr.sin_family = AF_INET;
    trackerAddr.sin_port = htons(8989);

    if (bind(trackerSocket, (struct sockaddr *)&trackerAddr, sizeof(trackerAddr)) == -1)
    {
        cerr << "Error binding to address" << endl;
        return 1;
    }

    if (listen(trackerSocket, 50) == 0)
        cout << "Listening" << endl;
    else
    {
        cerr << "Error listening" << endl;
        return 1;
    }

    while (1)
    {
        newSocket = accept(trackerSocket, (struct sockaddr *)&clientAddr, &addr_size);
        if (newSocket == -1)
        {
            cerr << "Error accepting connection" << std::endl;
            continue;
        }

        threadVec.push_back(thread(handleClient, newSocket));
    }
    for (int i = 0; i < threadVec.size(); i++)
    {
        if (threadVec[i].joinable())
            threadVec[i].join();
    }

    close(trackerSocket);

    return 0;
}
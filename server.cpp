#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "chat.pb.h"
#include <thread>
#include <chrono>

struct ClientConnection
{
    int socket;
    std::string username;
    std::string ip;
    std::string status;
    std::chrono::time_point<std::chrono::system_clock> lastActionTime;
};

const std::chrono::seconds TIMEOUT_PERIOD(15);

std::vector<ClientConnection> connectedClients;

// checks for inactive clients every 5 seconds!
std::thread timeoutThread([]()
                          {
    while (true) {
        auto now = std::chrono::system_clock::now();
        for (auto& client : connectedClients) {
            if (client.status == "INACTIVO") {
                continue;
            }
            if (now - client.lastActionTime > TIMEOUT_PERIOD) {
                std::cout << "Cliente inactivo." << std::endl;
                client.status = "INACTIVO";
                
                // Send status change response to the specific client
                chat::ServerResponse response;
                response.set_option(3);
                response.set_code(200);
                chat::ChangeStatus *change = response.mutable_change();
                change->set_username(client.username);
                change->set_status("INACTIVO");
                std::string serializedResponse;
                response.SerializeToString(&serializedResponse);
                send(client.socket, serializedResponse.c_str(), serializedResponse.length(), 0);
                
            } 
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    } });

void markClientAsActive(int clientSocket)
{
    for (auto &client : connectedClients)
    {
        if (client.socket == clientSocket)
        {
            if (client.status != "OCUPADO")
            {
                std::cout << "Usuario actual: " << client.username << std::endl;
                std::cout << "Estado Previo: " << client.status << std::endl;
                client.status = "ACTIVO";
                client.lastActionTime = std::chrono::system_clock::now();
                std::cout << "Estado Marcado:  " << client.status << std::endl;
            }
            break;
        }
    }
}

void handleClientConnection(int clientSocket)
{
    connectedClients.push_back({clientSocket,
                                "",
                                "",
                                "INVITADO", // he hasn't registered yet
                                std::chrono::system_clock::now()});

    // last client added
    auto it = std::prev(connectedClients.end());

    while (true)
    {
        char buffer[1024];
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0)
        {
            // Client disconnected or error occurred
            break;
        }

        std::string receivedData(buffer, bytesRead);

        chat::ClientPetition petition;
        if (petition.ParseFromString(receivedData))
        {

            // markClientAsActive(clientSocket);
            client.lastActionTime = std::chrono::system_clock::now();

            switch (petition.option())
            {
            /*
                User registration
           */
            case 1:
            {

                chat::UserRegistration registration = petition.registration();
                std::cout << "Registro: " << registration.username() << ", IP: " << registration.ip() << std::endl;

                // we need to check if the user is already registered
                bool found = false;
                for (const auto &client : connectedClients)
                {
                    if (client.username == registration.username())
                    {
                        found = true;
                        break;
                    }
                }

                if (found)
                {
                    // Create a response message
                    chat::ServerResponse response;
                    response.set_option(1);
                    response.set_code(500);
                    response.set_servermessage("Ya existe un usuario con ese nombre");

                    std::string serializedResponse;
                    response.SerializeToString(&serializedResponse);

                    send(clientSocket, serializedResponse.c_str(), serializedResponse.length(), 0);
                    break;
                }

                for (auto &client : connectedClients)
                {
                    if (client.socket == clientSocket)
                    {
                        client.username = registration.username();
                        client.ip = registration.ip();
                        client.status = "ACTIVO";
                        break;
                    }
                }

                chat::ServerResponse response;
                response.set_option(1);
                response.set_code(200);
                response.set_servermessage("Usuario registrado correctamente.");

                std::string serializedResponse;
                response.SerializeToString(&serializedResponse);

                send(clientSocket, serializedResponse.c_str(), serializedResponse.length(), 0);

                break;
            }
            /*
                Connected users list
            */
            case 2:
            {

                chat::UserRequest userRequest = petition.users();

                chat::ServerResponse response;
                response.set_option(2);

                // Add the list of connected users to the response
                chat::ConnectedUsersResponse *connectedUsers = response.mutable_connectedusers();
                for (const auto &client : connectedClients)
                {
                    chat::UserInfo *userInfo = connectedUsers->add_connectedusers();
                    userInfo->set_username(client.username);
                    userInfo->set_ip(client.ip);
                    userInfo->set_status(client.status);
                }

                // Serialize the response message
                std::string serializedResponse;
                response.set_code(200);
                response.set_servermessage("Lista de usuarios conectados. ");
                response.SerializeToString(&serializedResponse);

                // Send the serialized response back to the client
                send(clientSocket, serializedResponse.c_str(), serializedResponse.length(), 0);

                break;
            }
            /*
                Change status
            */
            case 3:
            {
                chat::ChangeStatus changeStatus = petition.change();
                std::string username = changeStatus.username();
                std::string status = changeStatus.status();

                // Find the client that changed their status
                for (auto &client : connectedClients)
                {
                    if (client.username == username)
                    {
                        client.status = status;

                        // Send the status change response to the specific client
                        chat::ServerResponse response;
                        response.set_option(3);
                        response.set_code(200);
                        chat::ChangeStatus *change = response.mutable_change();
                        change->set_username(username);
                        change->set_status(status);
                        std::string serializedResponse;
                        response.SerializeToString(&serializedResponse);
                        send(client.socket, serializedResponse.c_str(), serializedResponse.length(), 0);

                        break;
                    }
                }

                break;
            }
            /*
                Private and group messages
            */
            case 4:
            {
                // Handle incoming message
                chat::MessageCommunication messageComm = petition.messagecommunication();
                std::string recipient = messageComm.recipient();
                std::string message = messageComm.message();
                std::string sender = messageComm.sender();

                if (recipient == "everyone")
                {
                    std::cout << "Mensaje grupal de: " << sender << std::endl;

                    // Broadcast message to all connected clients
                    for (const auto &client : connectedClients)
                    {
                        chat::ServerResponse response;
                        response.set_option(4);
                        response.set_code(200);
                        response.set_servermessage("Nuevo mensaje de grupo.");

                        chat::MessageCommunication *responseMessageComm = response.mutable_messagecommunication();
                        responseMessageComm->set_message(message);
                        responseMessageComm->set_recipient(recipient);
                        responseMessageComm->set_sender(sender);

                        std::string serializedResponse;
                        response.SerializeToString(&serializedResponse);

                        send(client.socket, serializedResponse.c_str(), serializedResponse.length(), 0);
                    }
                }
                else
                {
                    std::cout << "Mensaje de: " << sender << " para " << recipient << std::endl;

                    bool found = false;

                    // Send message to the specified recipient
                    for (const auto &client : connectedClients)
                    {
                        if (client.username == recipient)
                        {
                            std::cout << "Usuario encontrado." << std::endl;
                            found = true;
                            chat::ServerResponse response;
                            response.set_option(4);
                            response.set_code(200);
                            response.set_servermessage("Mensaje privado.");

                            chat::MessageCommunication *responseMessageComm = response.mutable_messagecommunication();
                            responseMessageComm->set_message(message);
                            responseMessageComm->set_recipient(recipient);
                            responseMessageComm->set_sender(sender);

                            std::string serializedResponse;
                            response.SerializeToString(&serializedResponse);

                            send(client.socket, serializedResponse.c_str(), serializedResponse.length(), 0);
                            break;
                        }
                    }

                    if (found)
                    {
                        for (const auto &client : connectedClients)
                        {
                            if (client.username == sender)
                            {
                                std::cout << "Enviando respuesta a: " << sender << std::endl;
                                chat::ServerResponse response;
                                response.set_option(4);
                                response.set_code(200);
                                response.set_servermessage("Mensaje enviado.");

                                std::string serializedResponse;
                                response.SerializeToString(&serializedResponse);

                                send(client.socket, serializedResponse.c_str(), serializedResponse.length(), 0);
                                break;
                            }
                        }
                    }

                    if (found == false)
                    {
                        for (const auto &client : connectedClients)
                        {
                            if (client.username == sender)
                            {
                                std::cout << "Enviando error a: " << sender << std::endl;
                                chat::ServerResponse response;
                                response.set_option(4);
                                response.set_code(500);
                                response.set_servermessage("Usuario no encontrado");

                                std::string serializedResponse;
                                response.SerializeToString(&serializedResponse);

                                send(client.socket, serializedResponse.c_str(), serializedResponse.length(), 0);
                                break;
                            }
                        }
                    }
                }
                break;
            }
            /*
                Specific User request
            */
            case 5:
            {

                // Solicitud de usuario en especifico
                chat::UserRequest userRequest = petition.users();

                // Create a response message
                chat::ServerResponse response;
                response.set_option(5);

                // Add the list of connected users to the response
                chat::ConnectedUsersResponse *connectedUsers = response.mutable_connectedusers();
                bool found = false;
                for (const auto &client : connectedClients)
                {
                    if (client.username == userRequest.user())
                    {
                        chat::UserInfo *userInfo = connectedUsers->add_connectedusers();
                        userInfo->set_username(client.username);
                        userInfo->set_ip(client.ip);
                        userInfo->set_status(client.status);
                        found = true;
                    }
                }

                if (!found)
                {
                    // Serialize the response message
                    std::string serializedResponse;
                    response.set_code(500);
                    response.set_servermessage("Usuario " + userRequest.user() + " no encontrado.");
                    response.SerializeToString(&serializedResponse);

                    // Send the serialized response back to the client
                    send(clientSocket, serializedResponse.c_str(), serializedResponse.length(), 0);
                }
                else
                {
                    // Serialize the response message
                    std::string serializedResponse;
                    response.set_code(200);
                    response.set_servermessage("Usuario " + userRequest.user() + " encontrado.");
                    response.SerializeToString(&serializedResponse);

                    // Send the serialized response back to the client
                    send(clientSocket, serializedResponse.c_str(), serializedResponse.length(), 0);
                }

                break;
            }
            default:
            {
                std::cout << "Opción desconocida" << std::endl;
                break;
            }
            }
        }
        else
        {
            std::cerr << "Error parseando mensaje" << std::endl;
        }
    }

    // Remove the client
    for (auto it = connectedClients.begin(); it != connectedClients.end(); ++it)
    {
        if (it->socket == clientSocket)
        {
            connectedClients.erase(it);
            break;
        }
    }

    close(clientSocket);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: <servidor> <puerto>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
    {
        std::cerr << "Error al crear el socket del servidor" << std::endl;
        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(port);

    int optval = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1)
    {
        std::cerr << "Error al establecer la opción SO_REUSEADDR" << std::endl;
        close(serverSocket);
        return 1;
    }

    if (bind(serverSocket, reinterpret_cast<sockaddr *>(&serverAddress), sizeof(serverAddress)) == -1)
    {
        std::cerr << "Error al enlazar el socket del servidor" << std::endl;
        close(serverSocket);
        return 1;
    }

    if (listen(serverSocket, 5) == -1)
    {
        std::cerr << "Error al escuchar en el socket del servidor" << std::endl;
        close(serverSocket);
        return 1;
    }

    std::cout << "Escuchando en el puerto: " << port << std::endl;

    while (true)
    {
        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);

        int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr *>(&clientAddress), &clientAddressLength);
        if (clientSocket == -1)
        {
            std::cerr << "Error aceptando cliente" << std::endl;
            close(serverSocket);
            return 1;
        }

        // 1 thread per client
        std::thread clientThread(handleClientConnection, clientSocket);
        clientThread.detach();
    }

    timeoutThread.detach();
    close(serverSocket);
    return 0;
}

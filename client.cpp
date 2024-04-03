#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "chat.pb.h"
#include <thread>
#include <sstream>
#include <atomic>

bool wait = false;                 // Indicates if the client is waiting for a response from the server.
std::string userStatus = "ACTIVO"; // The status of the user
std::string username = "";

std::atomic<bool> shouldExit(false);
/**
 * Connects to the server.
 *
 * @param serverIP The IP address of the server.
 * @param port The port number of the server.
 * @param username The username of the client.
 * @return The socket descriptor for the client connection, or -1 if an error occurs.
 */
int connectToServer(const std::string &serverIP, int port, const std::string &username)
{
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1)
    {
        std::cerr << "Error al crear el socket del cliente" << std::endl;
        return -1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    if (inet_pton(AF_INET, serverIP.c_str(), &(serverAddress.sin_addr)) <= 0)
    {
        std::cerr << "Dirección IP inválida" << std::endl;
        close(clientSocket);
        return -1;
    }

    if (connect(clientSocket, reinterpret_cast<sockaddr *>(&serverAddress), sizeof(serverAddress)) == -1)
    {
        std::cerr << "Error al conectar con el servidor" << std::endl;
        close(clientSocket);
        return -1;
    }

    return clientSocket;
}

/**
 * Registers a user with the server.
 *
 * @param clientSocket The socket descriptor for the client connection.
 * @param username The username of the user to be registered.
 * @return true if the registration is successful, false otherwise.
 */
bool registerUser(int clientSocket, const std::string &username)
{
    chat::ClientPetition registerPetition;
    registerPetition.set_option(1);
    chat::UserRegistration *registration = registerPetition.mutable_registration();
    registration->set_username(username);

    sockaddr_in clientAddress{};
    socklen_t clientAddressLength = sizeof(clientAddress);

    if (getpeername(clientSocket, reinterpret_cast<sockaddr *>(&clientAddress), &clientAddressLength) == -1)
    {
        std::cerr << "Error al obtener la información del cliente" << std::endl;
        return false;
    }

    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clientAddress.sin_addr), clientIP, INET_ADDRSTRLEN);
    registration->set_ip(clientIP);

    std::string serializedRegisterMessage;
    registerPetition.SerializeToString(&serializedRegisterMessage);

    ssize_t bytesSent = send(clientSocket, serializedRegisterMessage.c_str(), serializedRegisterMessage.length(), 0);
    if (bytesSent == -1)
    {
        std::cerr << "Error al enviar el mensaje de registro" << std::endl;
        return false;
    }

    // Receive the server's response
    char buffer[4096];
    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesRead == -1)
    {
        std::cerr << "Error al recibir la respuesta del servidor" << std::endl;
        return false;
    }

    // Parse the server's response
    chat::ServerResponse response;
    if (!response.ParseFromArray(buffer, bytesRead))
    {
        std::cerr << "Error al parsear la respuesta del servidor" << std::endl;
        return false;
    }

    // Check the response code
    if (response.has_code())
    {
        int responseCode = response.code();
        if (responseCode == 500)
        {
            std::cerr << "Error de registro: " << response.servermessage() << std::endl; // username already exists. Change username!
            return false;
        }
    }

    return true;
}

/**
 * Receives messages from the server.
 *
 * @param clientSocket The socket descriptor for the client connection.
 */
void receiveMessages(int clientSocket)
{
    while (!shouldExit)
    {
        char buffer[1024];
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0)
        {
            std::cerr << "Error recibiendo respuesta del servidor." << std::endl;
            break;
        }

        chat::ServerResponse response;
        if (response.ParseFromArray(buffer, bytesRead))
        {
            if (response.has_code())
            {
                int responseCode = response.code();
                if (responseCode == 200)
                {
                    std::cout << "OK (200) " << response.servermessage() << std::endl;

                    switch (response.option())
                    {
                    case 2:
                    {
                        // Connected users
                        if (response.has_connectedusers())
                        {
                            const chat::ConnectedUsersResponse &connectedUsersResponse = response.connectedusers();
                            for (int i = 0; i < connectedUsersResponse.connectedusers_size(); i++)
                            {
                                const chat::UserInfo &userInfo = connectedUsersResponse.connectedusers(i);
                                std::string username = userInfo.username();
                                std::string status = userInfo.status();
                                std::string ip = userInfo.ip();

                                std::cout << "Usuario: " << username << " - " << status << " - " << ip << std::endl;
                            }
                        }
                        else
                        {
                            std::cerr << "ConnectedUsersResponse no encontrado." << std::endl;
                        }
                        break;
                    }
                    case 3:
                    {
                        // Change status response
                        if (response.has_change())
                        {
                            const chat::ChangeStatus &statusChangeResponse = response.change();
                            std::string username = statusChangeResponse.username();
                            std::string status = statusChangeResponse.status();
                            userStatus = status;
                            std::cout << "Cambio de estado de " << username << " a " << status << std::endl;
                        }
                        else
                        {
                            std::cerr << "StatusChangeResponse no encontrado." << std::endl;
                        }
                        break;
                    }

                    case 5:
                    {
                        // User information
                        if (response.has_connectedusers())
                        {
                            const chat::ConnectedUsersResponse &connectedUsersResponse = response.connectedusers();
                            for (int i = 0; i < connectedUsersResponse.connectedusers_size(); i++)
                            {
                                const chat::UserInfo &userInfo = connectedUsersResponse.connectedusers(i);
                                std::string username = userInfo.username();
                                std::string status = userInfo.status();
                                std::string ip = userInfo.ip();

                                std::cout << "Usuario: " << username << " - " << status << " - " << ip << std::endl;
                            }
                        }
                        else
                        {
                            std::cerr << "ConnectedUsersResponse no encontrado." << std::endl;
                        }
                        break;
                    }
                    case 4:
                        // A message has been received
                        if (response.has_messagecommunication())
                        {
                            const chat::MessageCommunication &messageComm = response.messagecommunication();
                            std::string sender = messageComm.sender();
                            std::string recipient = messageComm.recipient();

                            if (recipient == "everyone")
                            {
                                std::cout << "<" << sender << ">: " << messageComm.message() << std::endl;
                            }
                            else
                            {
                                std::cout
                                    << "(Privado) "
                                    << "<" << sender << ">: " << messageComm.message() << std::endl;
                            }
                        }
                        else
                        {
                            std::cerr << "Empty message." << std::endl;
                        }
                    }
                }
                else
                {
                    std::cout << "Error (500): " << response.servermessage() << std::endl;
                }
            }
            else
            {
                std::cerr << "Respuesta sin código." << std::endl;
            }
            wait = false;
        }
        else
        {
            std::cerr << "Error parsing server response." << std::endl;
            wait = false;
        }
    }
}

void close_socket(int socket_fd)
{
    if (socket_fd != -1)
    {
        if (close(socket_fd) == -1)
        {
            // Handle error here
            perror("Error closing socket");
        }
    }
}

void showHelp()
{
    std::cout << "Usuario: " << username << std::endl;
    std::cout << "Estado: " << userStatus << std::endl;
    std::cout << "Comandos disponibles:" << std::endl;
    std::cout << "- /usuarios: Despliega los usuarios conectados." << std::endl;
    std::cout << "- /buscar <usuario>: Busca un usuario por su nombre." << std::endl;
    std::cout << "- /mensaje <mensaje>: Envía un mensaje a todos los usuarios conectados." << std::endl;
    std::cout << "- /privado <usuario> <mensaje>: Envía un mensaje privado a un usuario." << std::endl;
    std::cout << "- /estado <estado>: Cambia el estado del usuario (ACTIVO, OCUPADO, INACTIVO)." << std::endl;
    std::cout << "- /ayuda: Muestra este menú de ayuda." << std::endl;
    std::cout << "- /salir: Cierra la sesión y termina el programa." << std::endl;
}

void handleUserInput(int clientSocket, const std::string &username)
{
    bool shouldExit = false;
    std::string input;
    while (shouldExit == false)
    {
        std::cout << "> ";
        std::getline(std::cin, input);

        if (input.empty())
        {
            continue;
        }

        if (input == "/usuarios")
        {
            chat::ClientPetition connectedUsersPetition;
            connectedUsersPetition.set_option(2);
            chat::UserRequest *userRequest = connectedUsersPetition.mutable_users();
            userRequest->set_user("everyone");

            std::string serializedConnectedUsersMessage;
            connectedUsersPetition.SerializeToString(&serializedConnectedUsersMessage);

            send(clientSocket, serializedConnectedUsersMessage.c_str(), serializedConnectedUsersMessage.length(), 0);
        }
        else if (input.substr(0, 8) == "/buscar ")
        {
            std::string userToSearch = input.substr(8);

            chat::ClientPetition searchUserPetition;
            searchUserPetition.set_option(5);
            chat::UserRequest *userRequest = searchUserPetition.mutable_users();
            userRequest->set_user(userToSearch);

            std::string serializedSearchUserMessage;
            searchUserPetition.SerializeToString(&serializedSearchUserMessage);

            send(clientSocket, serializedSearchUserMessage.c_str(), serializedSearchUserMessage.length(), 0);
        }
        else if (input.substr(0, 9) == "/mensaje ")
        {
            std::string message = input.substr(9);

            chat::ClientPetition messagePetition;
            messagePetition.set_option(4);
            chat::MessageCommunication *messageComm = messagePetition.mutable_messagecommunication();

            messageComm->set_recipient("everyone");
            messageComm->set_message(message);
            messageComm->set_sender(username);

            std::string serializedMessagePetition;
            messagePetition.SerializeToString(&serializedMessagePetition);

            send(clientSocket, serializedMessagePetition.c_str(), serializedMessagePetition.length(), 0);
        }
        else if (input.substr(0, 9) == "/privado ")
        {
            std::istringstream iss(input.substr(9));
            std::string recipient, message;
            if (iss >> recipient)
            {
                std::getline(iss, message);

                chat::ClientPetition messagePetition;
                messagePetition.set_option(4);
                chat::MessageCommunication *messageComm = messagePetition.mutable_messagecommunication();

                messageComm->set_recipient(recipient);
                messageComm->set_message(message);
                messageComm->set_sender(username);

                std::string serializedMessagePetition;
                messagePetition.SerializeToString(&serializedMessagePetition);

                send(clientSocket, serializedMessagePetition.c_str(), serializedMessagePetition.length(), 0);
            }
            else
            {
                std::cout << "Formato inválido. Uso: /privado <usuario> <mensaje>" << std::endl;
            }
        }
        else if (input.substr(0, 8) == "/estado ")
        {
            std::string status = input.substr(8);

            if (status != "ACTIVO" && status != "OCUPADO" && status != "INACTIVO")
            {
                std::cout << "Estado inválido. Los estados válidos son: ACTIVO, OCUPADO, INACTIVO." << std::endl;
                continue;
            }

            chat::ClientPetition statusChangePetition;
            statusChangePetition.set_option(3);
            chat::ChangeStatus *changeStatus = statusChangePetition.mutable_change();
            changeStatus->set_username(username);
            changeStatus->set_status(status);

            std::string serializedStatusChange;
            statusChangePetition.SerializeToString(&serializedStatusChange);

            send(clientSocket, serializedStatusChange.c_str(), serializedStatusChange.length(), 0);
        }
        else if (input == "/ayuda")
        {
            showHelp();
        }
        else if (input == "/salir")
        {
            std::cout << "Cerrando sesión..." << std::endl;
            shouldExit = true;
            break;
        }
        else
        {
            std::cout << "Comando inválido. Ingrese /ayuda para ver los comandos disponibles." << std::endl;
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        std::cerr << "Uso: ./client <dirección IP> <puerto> <nombre de usuario>" << std::endl;
        return 1;
    }

    std::string serverIP = argv[1];
    int port = std::stoi(argv[2]);
    username = argv[3];

    int clientSocket = connectToServer(serverIP, port, username);

    if (clientSocket == -1)
    {
        close(clientSocket);
        return 1;
    }

    if (!registerUser(clientSocket, username))
    {
        close(clientSocket);
        return 1;
    }

    std::thread receiveThread(receiveMessages, clientSocket);

    std::cout << "Conectado al servidor. Ingrese /ayuda para ver los comandos disponibles." << std::endl;

    handleUserInput(clientSocket, username);
    receiveThread.detach();
    // receiveThread.join();
    close(clientSocket);

    return 0;
}
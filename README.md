# OS-Project1
## Chat Application
This is a simple chat application implemented in C++ using Protocol Buffers for communication between the server and clients. The application allows users to connect to the server, send messages, and perform various actions such as displaying connected users, searching for users, sending group messages, sending direct messages, and changing user status. <br>

### 1. Prerequisites <br>

Install the Protocol Buffers compiler:
```bash
sudo apt install protobuf-compiler
```

Install the Protocol Buffers library for C++:
```bash
sudo apt-get install libprotobuf-dev
```

### 2. Setup <br>
Clone the repository:
```bash
git clone https://github.com/your-username/chat-application.git
cd chat-application
```


Compile the protocol:
```bash
protoc --cpp_out=. chat.proto
```

Compile the server:
```bash
g++ -std=c++11 -pthread -Wl,--no-as-needed server.cpp chat.pb.cc -lprotobuf -o server
```


Compile the client:
```bash
g++ -std=c++11 -pthread -Wl,--no-as-needed client.cpp chat.pb.cc -lprotobuf -o client
```

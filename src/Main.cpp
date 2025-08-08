#include <string_view>
#include <iostream>

#include "FileUploader.h"

void printHelpMessage() {
    std::cout << "This is a simple program to upload a file to a server.\n"
                 "Mandatory arguments:\n"
                 "-u    username\n"
                 "-p    password (Use quotes in case of whitespaces)\n"
                 "-f    file to upload (Use quotes in case of whitespaces)\n"
                 "Other arguments:"
                 "-h    print this help message\n";
}

int main(int argc, char* argv[])
{
    const char* username {nullptr};
    const char* password {nullptr};
    const char* file {nullptr};
    for(int i = 1; i < argc -1; ++i) {
        const std::string_view argument {argv[i]};
        if (argument == "-h") {
            printHelpMessage();
            return 0;
        }
        else if (argument == "-u") {
            username = argv[++i];
        }
        else if (argument == "-p") {
            password = argv[++i];
        }
        else if (argument == "-f") {
            file = argv[++i];
        }
    }


    if (username == nullptr || password == nullptr || file == nullptr) {
        std::cerr << "Not all mandatory arguments provided" << std::endl;
        printHelpMessage();
        return 1;
    }
    uploadFile(username, password, file);

    return 0;
}
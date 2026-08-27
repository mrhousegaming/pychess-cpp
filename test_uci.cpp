#include <iostream>
#include <string>
int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::string line;
    std::cout << "ready\n" << std::flush;
    while (std::getline(std::cin, line)) {
        std::cout << "echo: " << line << "\n" << std::flush;
        if (line == "quit") break;
    }
    return 0;
}

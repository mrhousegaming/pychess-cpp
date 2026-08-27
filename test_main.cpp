#include <iostream>
#include <string>
int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "uci") {
            std::cout << "id name Test\nuciok\n";
        } else if (line == "quit") {
            break;
        }
    }
    return 0;
}

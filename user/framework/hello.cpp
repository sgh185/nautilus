#include <iostream>

#include <cstdio>

class HelloFriend {
public:
  void hello() {
    printf("Hello, world!\n");
  }
};

int main() {
  HelloFriend fnd;
  fnd.hello();
  // std::cout << "Hello, world!" << std::endl;
  return 0;
}
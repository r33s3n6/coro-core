#include <iostream>

class protected_class {
    protected:
    int protected_int = 0;
    friend friend_class;
};

class friend_class {
    
    public:
    protected_class* operator->() {
        return &pc;
    }
    private:
    protected_class pc;
};

int main() {
    friend_class fc;
    fc->protected_int = 5;
    std::cout << fc->protected_int << std::endl;
    return 0;
}
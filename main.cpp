#include <iostream>
#include <cstring>
#include "malloc.cpp"

int main()
{
    SimpleDoublyLinkedMalloc my_alloc(1);

    std::cout << "Test 1: Malloc and Free\n";
    int *arr = (int *)my_alloc.malloc(5 * sizeof(int));
    if (arr != nullptr)
    {
        for (int i = 0; i < 5; i++)
        {
            arr[i] = (i + 1) * 10;
        }

        std::cout << "Array values: ";
        for (int i = 0; i < 5; i++)
        {
            std::cout << arr[i] << " ";
        }
        std::cout << "\n";

        my_alloc.free(arr);
        std::cout << "Freed successfully\n\n";
    }

    std::cout << "Test 2: Splitting\n";
    void *p1 = my_alloc.malloc(32);
    void *p2 = my_alloc.malloc(32);
    std::cout << "Address p1: " << p1 << "\n";
    std::cout << "Address p2: " << p2 << "\n";
    my_alloc.free(p1);
    my_alloc.free(p2);
    std::cout << "Freed p1 and p2\n\n";

    std::cout << "Test 3: Calloc\n";
    int *zero_arr = (int *)my_alloc.calloc(5, sizeof(int));
    if (zero_arr != nullptr)
    {
        std::cout << "Calloc values: ";
        for (int i = 0; i < 5; i++)
        {
            std::cout << zero_arr[i] << " ";
        }
        std::cout << "\n";
        my_alloc.free(zero_arr);
        std::cout << "Freed calloc\n\n";
    }

    std::cout << "Test 4: Realloc\n";
    char *str = (char *)my_alloc.malloc(16);
    if (str != nullptr)
    {
        std::strcpy(str, "Hello");
        std::cout << "Before realloc: " << str << "\n";

        str = (char *)my_alloc.realloc(str, 64);
        std::strcat(str, " World from Custom Malloc!");
        std::cout << "After realloc: " << str << "\n";

        my_alloc.free(str);
        std::cout << "Freed realloc\n\n";
    }

    return 0;
}

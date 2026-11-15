#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

const uint8_t MAGIC_MARKER = 0x43;
const size_t PAGE_SIZE = 4096;
const size_t ALIGNMENT = 16;

inline size_t align_up(size_t size)
{
    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

struct alignas(ALIGNMENT) Block
{
    uint8_t marker;
    bool is_free;
    uint32_t length;
    Block *prev;
    Block *next;

    void *payload()
    {
        return reinterpret_cast<void *>(this + 1);
    }

    static Block *from_payload(void *ptr)
    {
        return reinterpret_cast<Block *>(ptr) - 1;
    }
};

struct alignas(ALIGNMENT) GlobalHeader
{
    uint8_t marker;
    bool lock;
    uint32_t amount_of_blocks;
    uint16_t amount_of_pages;
    Block *first_block;
};

class SimpleDoublyLinkedMalloc
{
private:
    void *raw_mem;
    size_t total_bytes;
    GlobalHeader *my_header;

    void split_block(Block *block, size_t size)
    {
        if (block->length < size + sizeof(Block) + ALIGNMENT)
        {
            return;
        }

        auto *new_block = reinterpret_cast<Block *>(reinterpret_cast<char *>(block->payload()) + size);
        new_block->marker = MAGIC_MARKER;
        new_block->is_free = true;
        new_block->length = block->length - size - sizeof(Block);
        new_block->prev = block;
        new_block->next = block->next;

        if (block->next != nullptr)
        {
            block->next->prev = new_block;
        }

        block->length = size;
        block->next = new_block;

        my_header->amount_of_blocks++;
    }

    void merge_blocks(Block *block)
    {
        if (block->prev != nullptr && block->prev->is_free)
        {
            Block *prev_block = block->prev;
            prev_block->length += sizeof(Block) + block->length;
            prev_block->next = block->next;

            if (block->next != nullptr)
            {
                block->next->prev = prev_block;
            }

            my_header->amount_of_blocks--;
            block = prev_block;
        }

        if (block->next != nullptr && block->next->is_free)
        {
            Block *next_block = block->next;
            block->length += sizeof(Block) + next_block->length;
            block->next = next_block->next;

            if (next_block->next != nullptr)
            {
                next_block->next->prev = block;
            }

            my_header->amount_of_blocks--;
        }
    }

public:
    explicit SimpleDoublyLinkedMalloc(size_t initial_pages = 1)
        : raw_mem(nullptr), total_bytes(0), my_header(nullptr)
    {
        total_bytes = initial_pages * PAGE_SIZE;

#ifdef _WIN32
        raw_mem = ::VirtualAlloc(NULL, total_bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (raw_mem == NULL)
        {
            std::exit(1);
        }
#else
        raw_mem = ::mmap(NULL, total_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (raw_mem == MAP_FAILED)
        {
            std::exit(1);
        }
#endif

        my_header = reinterpret_cast<GlobalHeader *>(raw_mem);
        my_header->marker = MAGIC_MARKER;
        my_header->lock = false;
        my_header->amount_of_blocks = 1;
        my_header->amount_of_pages = static_cast<uint16_t>(initial_pages);

        auto *first = reinterpret_cast<Block *>(reinterpret_cast<char *>(raw_mem) + sizeof(GlobalHeader));
        size_t available_payload = total_bytes - sizeof(GlobalHeader) - sizeof(Block);

        first->marker = MAGIC_MARKER;
        first->is_free = true;
        first->length = static_cast<uint32_t>(available_payload);
        first->prev = nullptr;
        first->next = nullptr;

        my_header->first_block = first;
    }

    ~SimpleDoublyLinkedMalloc()
    {
#ifdef _WIN32
        if (raw_mem != nullptr)
        {
            ::VirtualFree(raw_mem, 0, MEM_RELEASE);
        }
#else
        if (raw_mem != nullptr && raw_mem != MAP_FAILED)
        {
            ::munmap(raw_mem, total_bytes);
        }
#endif
    }

    void *malloc(size_t size)
    {
        if (size == 0 || size > SIZE_MAX - ALIGNMENT)
        {
            return nullptr;
        }

        size = align_up(size);

        Block *curr = my_header->first_block;

        while (curr != nullptr)
        {
            if (curr->marker != MAGIC_MARKER)
            {
                std::exit(1);
            }

            if (curr->is_free && curr->length >= size)
            {
                split_block(curr, size);
                curr->is_free = false;
                return curr->payload();
            }

            curr = curr->next;
        }

        return nullptr;
    }

    void free(void *ptr)
    {
        if (ptr == nullptr)
        {
            return;
        }

        Block *block = Block::from_payload(ptr);

        if (block->marker != MAGIC_MARKER)
        {
            std::exit(1);
        }

        if (block->is_free)
        {
            return;
        }

        block->is_free = true;
        merge_blocks(block);
    }

    void *calloc(size_t num, size_t size)
    {
        if (size != 0 && num > SIZE_MAX / size)
        {
            return nullptr;
        }

        size_t total_size = num * size;
        void *ptr = this->malloc(total_size);
        if (ptr != nullptr)
        {
            std::memset(ptr, 0, total_size);
        }
        return ptr;
    }

    void *realloc(void *ptr, size_t new_size)
    {
        if (ptr == nullptr)
        {
            return this->malloc(new_size);
        }

        if (new_size == 0)
        {
            this->free(ptr);
            return nullptr;
        }

        if (new_size > SIZE_MAX - ALIGNMENT)
        {
            return nullptr;
        }

        Block *block = Block::from_payload(ptr);

        if (block->marker != MAGIC_MARKER || block->is_free)
        {
            std::exit(1);
        }

        new_size = align_up(new_size);

        if (block->length >= new_size)
        {
            split_block(block, new_size);
            return ptr;
        }

        if (block->next != nullptr && block->next->is_free &&
            (block->length + sizeof(Block) + block->next->length) >= new_size)
        {
            Block *next_block = block->next;
            block->length += sizeof(Block) + next_block->length;
            block->next = next_block->next;
            if (next_block->next != nullptr)
            {
                next_block->next->prev = block;
            }
            my_header->amount_of_blocks--;

            split_block(block, new_size);
            return ptr;
        }

        void *new_ptr = this->malloc(new_size);
        if (new_ptr == nullptr)
        {
            return nullptr;
        }

        std::memcpy(new_ptr, ptr, block->length);
        this->free(ptr);

        return new_ptr;
    }
};

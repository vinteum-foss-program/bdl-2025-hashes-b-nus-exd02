#include <cstdint>
#include <iomanip>
#include <iostream>
#include <tbb/parallel_for.h>
#include <tbb/spin_mutex.h>
#include <tbb/concurrent_unordered_map.h>
#include <tbb/task_group.h>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

tbb::spin_mutex print_mutex;

#define DEBUG

#ifdef DEBUG
#define debug_print(x) \
    do { \
        tbb::spin_mutex::scoped_lock lock(print_mutex); \
        std::cerr << "\033[32m[DEBUG]\033[0m " << x << '\n'; \
    } while (0)
#else
#define debug_print(...) do {} while (0)
#endif

std::string simple_hash(std::string_view str) {
    int hash_val = 0;
    for (char c : str)
        hash_val = ((hash_val << 5) - hash_val + c) & 0xffffffff;

    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8) << hash_val;

    return ss.str();
}

constexpr char PRINTABLE_ASCII[] =
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";
constexpr size_t PRINTABLE_ASCII_SIZE = sizeof(PRINTABLE_ASCII) - 1;
constexpr int MAX_STR_LEN = 8;

// transform a given number into a unique string
std::string index_to_string(uint64_t n, size_t max_str_len = MAX_STR_LEN) {
    // fills the string with the first printable ascii char
    std::string str(MAX_STR_LEN, PRINTABLE_ASCII[0]);

    for(size_t i = 0; i < max_str_len; i++) {
        str[i] = PRINTABLE_ASCII[n % PRINTABLE_ASCII_SIZE];
        n /= PRINTABLE_ASCII_SIZE;
    }

    return str;
}

// returns two strings that has hash collision
std::pair<std::string, std::string> find_collision_hash(uint64_t maximum_combinations) {
    // key is the hash, result is string that originated that hash
    // map["hash"] -> str
    tbb::concurrent_unordered_map <std::string, std::string> hash_to_str_map;
    std::pair<std::string, std::string> result = {"", ""};

    // Create a group context so it can be interrupted latter
    tbb::task_group_context ctx;

    tbb::parallel_for(tbb::blocked_range<uint64_t>(0, maximum_combinations, 4096), [&](const tbb::blocked_range<uint64_t>& r) {
        for (uint64_t i = r.begin(); i != r.end(); ++i) {
            if (ctx.is_group_execution_cancelled()) {
                return;
            }
            std::string str = index_to_string(i);
            std::string hash = simple_hash(str);

            auto it = hash_to_str_map.find(hash);
            if (it != hash_to_str_map.end()) {
                result = {str, it->second};
                debug_print("thread [" << std::this_thread::get_id() << "] found the collision [" << result.first << "] and [" << result.second << "]");
                ctx.cancel_group_execution();
                return;
            }

            hash_to_str_map[hash] = str;
            debug_print("thread [" << std::this_thread::get_id() << "] transformed [" << i << "] to string [" << str << "] hash [" << hash << "]");
        }
    }, ctx);

    return result;
}

int main() {
    unsigned n_threads = std::thread::hardware_concurrency();
    debug_print("Number of threads: " << n_threads);

    // this is the maximum of combinations
    // of 8 printable ascii characters - 1
    debug_print("ASCII Printable chars: " << PRINTABLE_ASCII_SIZE);
    uint64_t maximum_combinations = 1;
    for (int i = 1; i <= MAX_STR_LEN; i++)
        maximum_combinations *= PRINTABLE_ASCII_SIZE;

    debug_print("Max Combinations: " << maximum_combinations);

    auto collision = find_collision_hash(maximum_combinations);

    if (collision.first != "") {
        std::cout << "Collision: \"" << collision.first << "\", \"" << collision.second << "\"" << "\n";
    } else {
        debug_print("There is no collisions!");
    }

    return 0;
}

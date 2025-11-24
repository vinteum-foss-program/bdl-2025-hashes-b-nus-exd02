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
#include <openssl/sha.h>

tbb::spin_mutex print_mutex;

#ifdef DEBUG
#define debug_print(x) \
    do { \
        tbb::spin_mutex::scoped_lock lock(print_mutex); \
        std::cerr << "\033[32m[DEBUG]\033[0m " << x << '\n'; \
    } while (0)
#else
#define debug_print(...) do {} while (0)
#endif

std::string sha256(std::string_view data) {
    unsigned char digest[SHA256_DIGEST_LENGTH]; // 32 bytes

    SHA256(reinterpret_cast<const unsigned char*>(data.data()),
           data.size(),
           digest);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        oss << std::setw(2) << static_cast<int>(digest[i]);

    return oss.str();
}

constexpr char PRINTABLE_ASCII[] =
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";
constexpr size_t PRINTABLE_ASCII_SIZE = sizeof(PRINTABLE_ASCII) - 1;

// converts the number to base62
// then gets the respective character in the PRINTABLE_ASCII
std::string index_to_string(uint64_t n) {
    std::string str;

    if (n == 0) return { PRINTABLE_ASCII[0] };

    while (n > 0) {
        str.push_back(PRINTABLE_ASCII[n % PRINTABLE_ASCII_SIZE]);
        n /= PRINTABLE_ASCII_SIZE;
    }

    return str;
}

// Find a string that hashed has a certain prefix
std::string find_str_with_hash_prefix(uint64_t maximum_tries, std::string_view hash_prefix) {
    std::string result;

    // Create a group context so it can be interrupted latter
    tbb::task_group_context ctx;

    tbb::parallel_for(tbb::blocked_range<uint64_t>(0, maximum_tries, 4096), [&](const tbb::blocked_range<uint64_t>& r) {
        for (uint64_t i = r.begin(); i != r.end(); ++i) {
            if (ctx.is_group_execution_cancelled()) {
                return;
            }
            std::string str = "bitcoin";
            str += index_to_string(i);
            std::string hash = sha256(str);

            std::string curr_hash_prefix = hash.substr(0,hash_prefix.size());
            if (curr_hash_prefix == hash_prefix) {
                result = str;
                debug_print("thread [" << std::this_thread::get_id() << "] found a hash [" << hash << "] with target prefix [" << hash_prefix << "]");
                ctx.cancel_group_execution();
                return;
            }

            debug_print("thread:[" << std::this_thread::get_id() << "]  i:[" << i << "] str:[" << str << "] prefix:[" << curr_hash_prefix << "] (no match)");
        }
    }, ctx);

    return result;
}

int main() {
    unsigned n_threads = std::thread::hardware_concurrency();
    uint64_t max_tries = UINT64_MAX;

    debug_print("Number of threads: " << n_threads);
    debug_print("ASCII Printable chars: " << PRINTABLE_ASCII_SIZE);
    debug_print("Max tries: " << max_tries);

    std::vector<std::string_view> prefixes = {"cafe", "faded", "decade"};
    for (auto prefix : prefixes) {
        std::cout << "Searching for strings that has the prefix \"" << prefix << "\"...\n";
        auto str = find_str_with_hash_prefix(max_tries, prefix);
        if (!str.empty()) {
            std::cout << "String \"" << str << "\" generates a hash with the desired prefix!\n";
        } else {
            std::cout << "No strings found that generates this prefix\n";
        }
    }

    return 0;
}

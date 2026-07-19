#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <random>
#include <cstring>
#include <iomanip>
#include <secp256k1.h>
#include <openssl/sha.h>
#include <openssl/ripemd.h>

const std::string BASE58_CHARS = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

// High-speed inline Base58 encoder to bypass heap allocations
std::string encodeBase58(const unsigned char* input, size_t len) {
    std::vector<int> digits(40, 0);
    int digits_len = 1;

    for (size_t i = 0; i < len; ++i) {
        int carry = input[i];
        for (int j = 0; j < digits_len; ++j) {
            carry += digits[j] << 8;
            digits[j] = carry % 58;
            carry /= 58;
        }
        while (carry > 0) {
            digits[digits_len] = carry % 58;
            carry /= 58;
            digits_len++;
        }
    }

    std::string result = "";
    for (size_t i = 0; i < len && input[i] == 0; ++i) {
        result += '1';
    }
    for (int i = digits_len - 1; i >= 0; --i) {
        result += BASE58_CHARS[digits[i]];
    }
    return result;
}

// Low-overhead pipeline translating curve points directly to a Base58 address
std::string pubKeyToAddress(secp256k1_context* ctx, const secp256k1_pubkey& pubkey) {
    // Modern, future-proof pipeline translating curve points to a Base58 address
std::string pubKeyToAddress(secp256k1_context* ctx, const secp256k1_pubkey& pubkey) {
    unsigned char serialized_pub;
    size_t serialized_len = 33;
    
    secp256k1_ec_pubkey_serialize(ctx, serialized_pub, &serialized_len, &pubkey, SECP256K1_EC_COMPRESSED);

    // 1. SHA-256 Hash
    unsigned char sha256_res[SHA256_DIGEST_LENGTH];
    SHA256(serialized_pub, serialized_len, sha256_res);

    // 2. Modern OpenSSL 3.0+ EVP RIPEMD-160 Hash
    unsigned char ripemd_res[RIPEMD160_DIGEST_LENGTH + 5];
    ripemd_res[0] = 0x00; // Bitcoin Mainnet Network Byte

    unsigned int ripemd_len = 0;
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, EVP_ripemd160(), nullptr);
    EVP_DigestUpdate(mdctx, sha256_res, SHA256_DIGEST_LENGTH);
    EVP_DigestFinal_ex(mdctx, ripemd_res + 1, &ripemd_len);
    EVP_MD_CTX_free(mdctx);
}
    // 3. Double SHA-256 Checksum
    unsigned char checksum_sha1[SHA256_DIGEST_LENGTH];
    unsigned char checksum_sha2[SHA256_DIGEST_LENGTH];
    SHA256(ripemd_res, RIPEMD160_DIGEST_LENGTH + 1, checksum_sha1);
    SHA256(checksum_sha1, SHA256_DIGEST_LENGTH, checksum_sha2);

    // 4. Inject Checksum
    std::memcpy(ripemd_res + RIPEMD160_DIGEST_LENGTH + 1, checksum_sha2, 4);

    return encodeBase58(ripemd_res, RIPEMD160_DIGEST_LENGTH + 5);
}

void generatorWorker(int thread_id) {
    // Isolated contexts prevent thread locking bottlenecks
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    
    std::random_device rd;
    std::mt19937_64 rng(rd() ^ thread_id);
    
    unsigned char priv_key[32];

    while (true) {
        // Generate random 256-bit base key parameters
        for (int i = 0; i < 32; i += 8) {
            uint64_t r = rng();
            std::memcpy(priv_key + i, &r, 8);
        }

        // Fast 16-bit iteration loop modifying only the last 2 bytes of memory
        for (uint16_t counter = 0; counter < 65535; ++counter) {
            std::memcpy(priv_key + 30, &counter, 2);

            if (!secp256k1_ec_seckey_verify(ctx, priv_key)) continue;

            secp256k1_pubkey pubkey;
            if (!secp256k1_ec_pubkey_create(ctx, &pubkey, priv_key)) continue;

            std::string address = pubKeyToAddress(ctx, pubkey);

            // Periodically log out generated pairs to verify raw pipeline speed
            if (counter % 20000 == 0) {
                std::stringstream ss;
                ss << "[Thread " << thread_id << "] Addr: " << address << " | Priv: ";
                for(int i = 0; i < 32; ++i) {
                    ss << std::hex << std::setw(2) << std::setfill('0') << (int)priv_key[i];
                }
                ss << "\n";
                std::cout << ss.str() << std::flush;
            }
        }
    }
    secp256k1_context_destroy(ctx);
}

int main() {
    unsigned int threads = std::thread::hardware_concurrency();
    
    std::cout << "🚀 Initializing Ultra-Fast 16-Bit Multi-Threaded Address Generator...\n";
    std::cout << "🎛️  Spawning " << threads << " independent core workers...\n\n";

    std::vector<std::thread> worker_threads;
    for (unsigned int i = 0; i < threads; ++i) {
        worker_threads.push_back(std::thread(generatorWorker, i));
    }

    for (auto& t : worker_threads) {
        if (t.joinable()) t.join();
    }

    return 0;
}


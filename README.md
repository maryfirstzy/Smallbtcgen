Build compiler 
g++ -O3 -std=c++17 -pthread generator.cpp -lsecp256k1 -lcrypto -o fast_bitcoin_gen

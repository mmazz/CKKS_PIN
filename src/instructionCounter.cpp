#include "openfhe.h"
#include "utils.h"
#include "cryptocontext-ser.h"
#include "key/key-ser.h"
#include "scheme/ckksrns/ckksrns-ser.h"
extern "C" void start_measurement() __attribute__((used, noinline));
extern "C" void end_measurement() __attribute__((used, noinline));

extern "C" void start_measurement() {
    asm volatile("nop");
}

extern "C" void end_measurement() {
    asm volatile("nop");
}

int main(int argc, char* argv[]) {
    std::cout << "Starting Client" << '\n' << std::endl;
    std::vector<double> input = load_vector(DATAFOLDER+input_vec);
    std::cout << "Input deserilized" << '\n' << std::endl;


    CryptoContext<DCRTPoly> cc;
    lbcrypto::CryptoContextFactory<lbcrypto::DCRTPoly>::ReleaseAllContexts();
    if (!Serial::DeserializeFromFile(DATAFOLDER + ccLocation, cc, SerType::BINARY)) {
        std::cerr << "I cannot read serialized data from: " << DATAFOLDER << ccLocation << std::endl;
        std::exit(1);
    }
    std::cout << "Client CC deserialized";

    KeyPair<DCRTPoly> keys;  // We do NOT have a secret key. The client
    // should not have access to this
    if (!Serial::DeserializeFromFile(DATAFOLDER + pubKeyLocation, keys.publicKey, SerType::BINARY)) {
        std::cerr << "I cannot read serialized data from: " << DATAFOLDER << pubKeyLocation << std::endl;
        std::exit(1);
    }
    std::cout << "Client KP deserialized" << '\n' << std::endl;

    if (!Serial::DeserializeFromFile(DATAFOLDER + secKeyLocation, keys.secretKey, SerType::BINARY)) {
        std::cerr << "I cannot read serialized data from: " << DATAFOLDER << secKeyLocation << std::endl;
        std::exit(1);
    }
    std::cout << "Client KS deserialized" << '\n' << std::endl;


    lbcrypto::PseudoRandomNumberGenerator::SetPRNGSeed(0);
    Plaintext golden_result;
start_measurement();
    Plaintext ptxt1 = cc->MakeCKKSPackedPlaintext(input);
    auto c = cc->Encrypt(keys.publicKey, ptxt1);
    cc->Decrypt(keys.secretKey, c, &golden_result);
end_measurement();
//    golden_result->SetLength(4);
//    std::vector<double> result_vec = golden_result->GetRealPackedValue();
//    float norm2_abs = norm2(result_vec, result_vec, 4);
//    std::cout << "Norm2: " << norm2_abs << std::endl;
    return 0;
}


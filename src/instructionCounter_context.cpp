#include "constants-defs.h"
#include "openfhe.h"
#include "utils.h"
#include <tuple>
// header files needed for serialization
#include "cryptocontext-ser.h"
#include "key/key-ser.h"
#include "scheme/ckksrns/ckksrns-ser.h"

#include <iostream>
#include <fstream>
#include <vector>

using namespace lbcrypto;
void demarcate(const std::string& msg) {
    std::cout << std::setw(50) << std::setfill('*') << '\n' << std::endl;
    std::cout << msg << std::endl;
    std::cout << std::setw(50) << std::setfill('*') << '\n' << std::endl;
}

std::tuple<CryptoContext<DCRTPoly>, KeyPair<DCRTPoly>, int> serverSetupAndWrite(int ringDim, int multDepth,int firstModSize, int scaleModSize,
                                                          int batchSize, ScalingTechnique rescaleTech) {
    CCParams<CryptoContextCKKSRNS> parameters;
    parameters.SetMultiplicativeDepth(multDepth);
    parameters.SetScalingModSize(scaleModSize);
    parameters.SetFirstModSize(firstModSize);
    parameters.SetBatchSize(batchSize);
    parameters.SetRingDim(ringDim);
    parameters.SetScalingTechnique(rescaleTech);
    parameters.SetSecurityLevel(HEStd_NotSet);
    std::vector<double> vec = uniform_dist(batchSize, 0, 8, 0, false);

    CryptoContext<DCRTPoly> serverCC = GenCryptoContext(parameters);

    serverCC->Enable(PKE);

    std::cout << "Cryptocontext generated" << std::endl;

    KeyPair<DCRTPoly> serverKP = serverCC->KeyGen();
    std::cout << "Keypair generated" << std::endl;

    demarcate("Part 2: Data Serialization (server)");

    save_vector(vec, DATAFOLDER + input_vec);

    if (!Serial::SerializeToFile(DATAFOLDER + ccLocation, serverCC, SerType::BINARY)) {
        std::cerr << "Error writing serialization of the crypto context to "
                     "cryptocontext.txt"
                  << std::endl;
        std::exit(1);
    }

    std::cout << "Cryptocontext serialized" << std::endl;

    if (!Serial::SerializeToFile(DATAFOLDER + pubKeyLocation, serverKP.publicKey, SerType::BINARY)) {
        std::cerr << "Exception writing public key to pubkey.txt" << std::endl;
        std::exit(1);
    }
    std::cout << "Public key serialized" << std::endl;

    if (!Serial::SerializeToFile(DATAFOLDER + secKeyLocation, serverKP.secretKey, SerType::BINARY)) {
        std::cerr << "Exception writing secret key to seckey.txt" << std::endl;
        std::exit(1);
    }
    std::cout << "Secret key serialized" << std::endl;
    return std::make_tuple(serverCC, serverKP, vec.size());
}

int main(int argc, char* argv[]) {

    ScalingTechnique rescaleTech = FIXEDMANUAL;
    uint32_t firstModSize    = 60;
    uint32_t scaleModSize    = 40;
    uint32_t logN = 3;
    uint32_t ringDim     = 1 << logN;
    uint32_t multDepth = 0;
    uint32_t batchSize = ringDim >> 1;

    auto tupleCryptoContext_KeyPair = serverSetupAndWrite(ringDim, multDepth, firstModSize, scaleModSize, batchSize, rescaleTech);
    const int cryptoContextIdx = 0;
    const int keyPairIdx       = 1;
    auto cc                         = std::get<cryptoContextIdx>(tupleCryptoContext_KeyPair);
    auto keys                         = std::get<keyPairIdx>(tupleCryptoContext_KeyPair);
    std::vector<double> input = load_vector(DATAFOLDER+input_vec);

    lbcrypto::PseudoRandomNumberGenerator::SetPRNGSeed(0);
    Plaintext golden_result;
    Plaintext ptxt1 = cc->MakeCKKSPackedPlaintext(input);
    auto c = cc->Encrypt(keys.publicKey, ptxt1);
    cc->Decrypt(keys.secretKey, c, &golden_result);
    return 0;
}


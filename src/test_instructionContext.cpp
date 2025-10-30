#include "constants-defs.h"
#include "openfhe.h"
#include "utils.h"
#include <tuple>
#include <iostream>
#include <fstream>
#include <vector>

using namespace lbcrypto;
int main(int argc, char* argv[]) {

    ScalingTechnique rescaleTech = FIXEDMANUAL;
    uint32_t firstModSize    = 60;
    uint32_t scaleModSize    = 40;
    uint32_t logN = 3;
    uint32_t ringDim     = 1 << logN;
    uint32_t multDepth = 0;
    uint32_t batchSize = ringDim >> 1;
    CCParams<CryptoContextCKKSRNS> parameters;
    parameters.SetMultiplicativeDepth(multDepth);
    parameters.SetScalingModSize(scaleModSize);
    parameters.SetFirstModSize(firstModSize);
    parameters.SetBatchSize(batchSize);
    parameters.SetRingDim(ringDim);
    parameters.SetScalingTechnique(rescaleTech);
    parameters.SetSecurityLevel(HEStd_NotSet);
    CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);
    cc->Enable(PKE);
    cc->Enable(LEVELEDSHE);

    auto keys = cc->KeyGen();

    std::vector<double> input = load_vector(DATAFOLDER+input_vec);

    lbcrypto::PseudoRandomNumberGenerator::SetPRNGSeed(0);
    Plaintext golden_result;
    Plaintext ptxt1 = cc->MakeCKKSPackedPlaintext(input);
    auto c = cc->Encrypt(keys.publicKey, ptxt1);
    cc->Decrypt(keys.secretKey, c, &golden_result);
    return 0;
}

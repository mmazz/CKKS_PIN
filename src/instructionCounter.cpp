#include "openfhe.h"
#include "utils.h"
extern "C" void start_measurement();
extern "C" void end_measurement();

asm(
    ".global start_measurement       \n"
    ".type   start_measurement, @function \n"
    "start_measurement:              \n"
    "    nop                   \n"
    "    ret                   \n"
    ".global end_measurement         \n"
    ".type   end_measurement, @function \n"
    "end_measurement:                \n"
    "    nop                   \n"
    "    ret                   \n"
);
int main(int argc, char* argv[]) {
    uint32_t firstMod    = 60;
    uint32_t scaleMod    = 50;
    uint32_t logN = 3;
    uint32_t ringDim     = 1 << logN;
    uint32_t multDepth = 0;
    uint32_t batchSize = ringDim >> 1;

    ScalingTechnique rescaleTech = FIXEDMANUAL;
    CCParams<CryptoContextCKKSRNS> parameters;
    parameters.SetMultiplicativeDepth(multDepth);
    parameters.SetScalingModSize(scaleMod);
    parameters.SetFirstModSize(firstMod);
    parameters.SetBatchSize(batchSize);
    parameters.SetRingDim(ringDim);
    parameters.SetScalingTechnique(rescaleTech);
    parameters.SetSecurityLevel(HEStd_NotSet);
    CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);
    cc->Enable(PKE);
    cc->Enable(LEVELEDSHE);

    auto keys = cc->KeyGen();

    std::vector<double> input = uniform_dist(batchSize, 0, 8, 0, false);
    lbcrypto::PseudoRandomNumberGenerator::SetPRNGSeed(0);
    Plaintext golden_result;
start_measurement();
    Plaintext ptxt1 = cc->MakeCKKSPackedPlaintext(input);
    auto c = cc->Encrypt(keys.publicKey, ptxt1);
    cc->Decrypt(keys.secretKey, c, &golden_result);
end_measurement();
    return 0;
}


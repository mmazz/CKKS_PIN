#include "openfhe.h"
#include "utils.h"

extern "C" __attribute__((noinline, used)) void start_profiling_marker() {
    asm volatile ("" ::: "memory");
}

extern "C" __attribute__((noinline, used)) void stop_profiling_marker() {
    asm volatile ("" ::: "memory");
}


int testVariable = 0;
int main(int argc, char* argv[]) {

    uint32_t RNS_size    = 1;
    uint32_t firstMod    = 60;
    uint32_t scaleMod    = 50;
    uint32_t logN        = 6;
    uint32_t gap         = 0;
    int logMin           = 0;
    int logMax           = 2;
    uint32_t ringDim     = 1 << logN;

    uint32_t multDepth = RNS_size;

    uint32_t batchSize = ringDim >> 1;
    if(gap>0)
        batchSize = batchSize >> gap;
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

    auto keys = cc->KeyGen();

    Plaintext result_bitFlip;
    Plaintext result_golden;
    std::vector<double> input = uniform_dist(batchSize, logMin, logMax, 0, false);
    Plaintext ptxt_orig = cc->MakeCKKSPackedPlaintext(input);
    auto c_orig = cc->Encrypt(keys.publicKey, ptxt_orig);
    cc->Decrypt(keys.secretKey, c_orig, &result_golden);
    result_golden->SetLength(batchSize);
    std::vector<double> result_golden_vec = result_golden->GetRealPackedValue();
start_profiling_marker();
    Plaintext ptxt1 = cc->MakeCKKSPackedPlaintext(input);
    auto c = cc->Encrypt(keys.publicKey, ptxt1);
    cc->Decrypt(keys.secretKey, c, &result_bitFlip);
stop_profiling_marker();
    result_bitFlip->SetLength(batchSize);
    std::vector<double> result_bitFlip_vec = result_bitFlip->GetRealPackedValue();
    double norm2_val = norm2(result_golden_vec, result_bitFlip_vec, batchSize);
    std::cout << norm2_val << std::endl;
    return 0;
}


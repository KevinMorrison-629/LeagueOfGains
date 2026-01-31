/*
 * LINKER FIX FOR WINDOWS
 * This file manually provides the definition for bn_deleter to prevent LNK2019 errors.
 */

// Handle the macro definition cleanly to avoid C4005 warnings
#ifdef DPP_BUILD
// It's already defined (likely by build system), so we don't need to do anything
#else
// Define it so this specific file exports the symbol instead of importing it
#define DPP_BUILD
#endif

#include <dpp/dpp.h>
#include <openssl/bn.h>

namespace dpp
{
    // FIX: Added 'noexcept' to match the declaration in dpp/bignum.h
    void bignumber::bn_deleter::operator()(openssl_bignum *b) const noexcept
    {
        if (b)
        {
            BN_free(reinterpret_cast<BIGNUM *>(b));
        }
    }
} // namespace dpp
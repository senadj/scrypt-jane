#include <stdlib.h>
#include <xmmintrin.h>

extern "C"
{
    #include "scrypt-jane.h"
}

#include "scrypt-mine.h"

void scrypt_hash( const void* input, size_t inputlen, uint32_t *res, unsigned char Nfactor )
{
    return scrypt((const unsigned char*)input, inputlen,
                  (const unsigned char*)input, inputlen,
                  Nfactor, 0, 0, (unsigned char*)res, 32);
}


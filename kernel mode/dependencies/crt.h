namespace crt
{
    template <typename t>
    __forceinline int strlen(t str) {
        if (!str)
        {
            return 0;
        }

        t buffer = str;

        while (*buffer)
        {
            *buffer++;
        }

        return (int)(buffer - str);
    }

    bool strcmp(const char* src, const char* dst)
    {
        if (!src || !dst)
        {
            return true;
        }

        const auto src_sz = crt::strlen(src);
        const auto dst_sz = crt::strlen(dst);

        if (src_sz != dst_sz)
        {
            return true;
        }

        for (int i = 0; i < src_sz; i++)
        {
            if (src[i] != dst[i])
            {
                return true;
            }
        }

        return false;
    }

    INT kmemcmp(const void* s1, const void* s2, size_t n)
    {
        const unsigned char* p1 = (const unsigned char*)s1;
        const unsigned char* end1 = p1 + n;
        const unsigned char* p2 = (const unsigned char*)s2;
        int                   d = 0;
        for (;;) {
            if (d || p1 >= end1) break;
            d = (int)*p1++ - (int)*p2++;
            if (d || p1 >= end1) break;
            d = (int)*p1++ - (int)*p2++;
            if (d || p1 >= end1) break;
            d = (int)*p1++ - (int)*p2++;
            if (d || p1 >= end1) break;
            d = (int)*p1++ - (int)*p2++;
        }
        return d;
    }

    INT kMemcmp(const void* str1, const void* str2, size_t count)
    {
        register const unsigned char* s1 = (const unsigned char*)str1;
        register const unsigned char* s2 = (const unsigned char*)str2;
        while (count-- > 0)
        {
            if (*s1++ != *s2++)
                return s1[-1] < s2[-1] ? -1 : 1;
        }
        return 0;
    }

    VOID* kmemcpy(void* dest, const void* src, size_t len)
    {
        char* d = (char*)dest;
        const char* s = (const char*)src;
        while (len--)
            *d++ = *s++;
        return dest;
    }

    VOID* kmemset(void* dest, UINT8 c, size_t count)
    {
        size_t blockIdx;
        size_t blocks = count >> 3;
        size_t bytesLeft = count - (blocks << 3);
        UINT64 cUll =
            c
            | (((UINT64)c) << 8)
            | (((UINT64)c) << 16)
            | (((UINT64)c) << 24)
            | (((UINT64)c) << 32)
            | (((UINT64)c) << 40)
            | (((UINT64)c) << 48)
            | (((UINT64)c) << 56);

        UINT64* destPtr8 = (UINT64*)dest;
        for (blockIdx = 0; blockIdx < blocks; blockIdx++) destPtr8[blockIdx] = cUll;

        if (!bytesLeft) return dest;

        blocks = bytesLeft >> 2;
        bytesLeft = bytesLeft - (blocks << 2);

        UINT32* destPtr4 = (UINT32*)&destPtr8[blockIdx];
        for (blockIdx = 0; blockIdx < blocks; blockIdx++) destPtr4[blockIdx] = (UINT32)cUll;

        if (!bytesLeft) return dest;

        blocks = bytesLeft >> 1;
        bytesLeft = bytesLeft - (blocks << 1);

        UINT16* destPtr2 = (UINT16*)&destPtr4[blockIdx];
        for (blockIdx = 0; blockIdx < blocks; blockIdx++) destPtr2[blockIdx] = (UINT16)cUll;

        if (!bytesLeft) return dest;

        UINT8* destPtr1 = (UINT8*)&destPtr2[blockIdx];
        for (blockIdx = 0; blockIdx < bytesLeft; blockIdx++) destPtr1[blockIdx] = (UINT8)cUll;

        return dest;
    }

    int MemCmp(const void* s1, const void* s2, size_t n)
    {
        const unsigned char* p1 = (const unsigned char*)s1;
        const unsigned char* end1 = p1 + n;
        const unsigned char* p2 = (const unsigned char*)s2;
        int                   d = 0;
        for (;;) {
            if (d || p1 >= end1) break;
            d = (int)*p1++ - (int)*p2++;
            if (d || p1 >= end1) break;
            d = (int)*p1++ - (int)*p2++;
            if (d || p1 >= end1) break;
            d = (int)*p1++ - (int)*p2++;
            if (d || p1 >= end1) break;
            d = (int)*p1++ - (int)*p2++;
        }
        return d;
    }

    void* MemCpy(void* dest, const void* src, size_t len)
    {
        char* d = (char*)dest;
        const char* s = (const char*)src;
        while (len--)
            *d++ = *s++;
        return dest;
    }

    INT kstrcmp(const char* s1, const char* s2)
    {
        while (*s1 == *s2++)
            if (*s1++ == 0)
                return (0);
        return (*(unsigned char*)s1 - *(unsigned char*)--s2);
    }

    SIZE_T StrLen(const char* str)
    {
        const char* s;
        for (s = str; *s; ++s);
        return (s - str);
    }

    int StrnCmp(const char* s1, const char* s2, size_t n)
    {
        if (n == 0)
            return (0);
        do {
            if (*s1 != *s2++)
                return (*(unsigned char*)s1 - *(unsigned char*)--s2);
            if (*s1++ == 0)
                break;
        } while (--n != 0);
        return (0);
    }

    char* StrStr(const char* s, const char* find)
    {
        char c, sc;
        size_t len;
        if ((c = *find++) != 0)
        {
            len = StrLen(find);
            do
            {
                do
                {
                    if ((sc = *s++) == 0)
                    {
                        return (NULL);
                    }
                } while (sc != c);
            } while (StrnCmp(s, find, len) != 0);
            s--;
        }
        return ((char*)s);
    }
}


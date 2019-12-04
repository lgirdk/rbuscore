#include <rtRetainable.h>

void rtRetainable_retainInternal(rtRetainable* r)
{
    __sync_add_and_fetch(&r->refCount, 1);
}

void rtRetainable_releaseInternal(rtRetainable* r, void (*destructor)(rtRetainable*))
{
    __sync_sub_and_fetch(&r->refCount, 1);

    if(r->refCount == 0)
    {
        destructor(r);
    }
}

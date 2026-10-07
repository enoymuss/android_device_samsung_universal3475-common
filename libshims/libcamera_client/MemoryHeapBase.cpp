#include <binder/MemoryHeapBase.h>
#include <new>
#include <sys/types.h>

// Eski imza: MemoryHeapBase(int fd, size_t size, uint32_t flags, uint32_t offset)
// Android 10'da offset parametresi off_t oldu, bu yüzden köprü gerekiyor.
extern "C" __attribute__((visibility("default")))
void _ZN7android14MemoryHeapBaseC1Eijjj(void* thisptr, int fd, size_t size,
                                         uint32_t flags, uint32_t offset) {
    new (thisptr) android::MemoryHeapBase(fd, size, flags, static_cast<off_t>(offset));
}

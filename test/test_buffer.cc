#include "core/buffer.h"

using namespace tube;

void
test_cow()
{
    Buffer buf, other;
    {
        buf.append((const byte*) "heefasdf", 5);
        Buffer newbuf;
        newbuf.append((const byte*) "he", 2);
        buf = newbuf;
        other = newbuf;
    }
    buf.write_to_fd(2);
    other.write_to_fd(2);
}

int
main(int argc, char *argv[])
{
    test_cow();
    return 0;
}

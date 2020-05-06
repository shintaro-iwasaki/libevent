## Argobots-aware libevent

This is under development and only for testing.
We will move to another repository once it becomes stable.

### How to compile libevent with Argobots

```
sh autogen.sh
./configure --enable-pthread-backend=argobots --with-argobots=ARGOBOTS_INSTALL_DIR
make -j NJOBS
make install
```
Now configure does not check availability of `libabt.so` and `abt.h`, which should be fixed in the future commit.
Please check if `libabt.so` is in `ARGOBOTS_INSTALL_DIR/lib` and `abt.h` in `ARGOBOTS_INSTALL_DIR/include`.

### Bug Report

Please send an email to Shintaro Iwasaki (siwasaki@anl.gov).

